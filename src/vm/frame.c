#include "vm/frame.h"
#include <stdio.h>
#include <bitmap.h>
#include <round.h>
#include <string.h>
#include "vm/page.h"
#include "vm/swap.h"
#include "userprog/pagedir.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "threads/malloc.h"
#include "threads/palloc.h"
#include "threads/synch.h"
#include "userprog/syscall.h"
#include "userprog/process.h"
/* Frame list */
static struct lock frame_table_lock; /* Frame table lock */

static struct list unused_list;
static struct list used_list;

void lock_frame()
{
    lock_acquire(&frame_table_lock);
}

void unlock_frame()
{
    lock_release(&frame_table_lock);
}
/*
 * Set up frame table
 */
void frame_init()
{
    list_init(&unused_list);
    list_init(&used_list);
    lock_init(&frame_table_lock);

    lock_acquire(&frame_table_lock);
    void *addr = palloc_get_page(PAL_USER | PAL_ZERO);
    /* Creates a frame struct for every page created, and puts it into list */
    while (addr != NULL)
    {

        struct frame *frame_entry = malloc(sizeof(struct frame));
        frame_entry->pinned = false;
        frame_entry->page = NULL;
        frame_entry->paddr = addr;
        list_push_front(&unused_list, &frame_entry->elem);
        addr = palloc_get_page(PAL_USER | PAL_ZERO);
    }
    lock_release(&frame_table_lock);
}

/**
 * Find a usable frame for an incoming frame request.
 */
struct frame *find_frame(struct spt_entry *page)
{
    //lock_acquire(&frame_table_lock);
    ASSERT(lock_held_by_current_thread(&frame_table_lock));
    struct list_elem *e = list_begin(&unused_list);
    if (e != list_end(&unused_list))
    {
        struct frame *f = list_entry(e, struct frame, elem);

        // upon preallocated frame, put it in the "been used"
        list_remove(e);
        list_push_back(&used_list, e);
        f->page = page;
        page->frame = f;
        return f;
    }
    // frame could not be snagged from unused  list
    struct frame *f;

    f = evict();
    ASSERT(f != NULL);
    list_remove(&f->elem);
    list_push_back(&used_list, &f->elem);
    f->page = page;
    page->frame = f;
    //lock_release(&frame_table_lock);
    return f;
}

/*
 * Frees frame
 */
void free_frame(struct frame *f)
{
    lock_acquire(&frame_table_lock);
    f->pinned = false;
    f->page = NULL;
    list_remove(&f->elem);
    list_push_back(&unused_list, &f->elem);
    lock_release(&frame_table_lock);
}

/*
 * Eviction - choose a frame to clear out and saves/swaps as needed
 */
struct frame *evict(void)
{
    ASSERT(lock_held_by_current_thread(&frame_table_lock));
    struct frame *candidate = NULL;
    for (struct list_elem *e = list_begin(&used_list); e != list_end(&used_list); e = list_next(e))
    {
        struct frame *f = list_entry(e, struct frame, elem);
        ASSERT(f != NULL);
        ASSERT(f->page != NULL);
        if (!f->page->pinned)
        {
            bool page_accessed = pagedir_is_accessed(f->page->pagedir, f->page->vaddr);
            ASSERT(f->page != NULL);
            if (!page_accessed)
            {
                candidate = f;
                break;
            }
            else
            {
                pagedir_set_accessed(f->page->pagedir, f->page->vaddr, false);
            }
        }
    }
     ASSERT(lock_held_by_current_thread(&frame_table_lock));
    if (candidate == NULL)
    {
        /* 2nd run. Unless all the pages are pinned, this should find a candidate. */
        for (struct list_elem *e = list_begin(&used_list); e != list_end(&used_list); e = list_next(e))
        {
            struct frame *f = list_entry(e, struct frame, elem);
            if (!f->page->pinned)
            {
                bool page_accessed = pagedir_is_accessed(f->page->pagedir, f->page->vaddr);
                if (!page_accessed)
                {
                    candidate = f;
                    break;
                }
                else
                {
                    pagedir_set_accessed(f->page->pagedir, f->page->vaddr, false);
                }
            }
        }
    }
    ASSERT(candidate != NULL);
    ASSERT(candidate->page != NULL);
    candidate->page->pinned = true;

    pagedir_clear_page(candidate->page->pagedir, candidate->page->vaddr);
    if (candidate->page->writable && candidate->page->page_status == 0 && pagedir_is_dirty(candidate->page->pagedir, candidate->page->vaddr))
    {
        lock_file();
        file_write_at(candidate->page->file, candidate->page->vaddr, candidate->page->bytes_read, candidate->page->offset);
        unlock_file();
    }
    else
    {
        swap_insert(candidate->page);
    }

    memset(candidate->paddr, 0, PGSIZE);
    candidate->page->pinned = false;
    return candidate;
}