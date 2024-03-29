#include "vm/frame.h"
#include <stdio.h>
#include <bitmap.h>
#include <round.h>
#include "vm/page.h"
#include "vm/swap.h"
#include "userprog/pagedir.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "threads/malloc.h"
#include "threads/palloc.h"
#include "threads/synch.h"
#include "userprog/syscall.h"

static struct list frame_list;       /* Frame list */
static struct lock frame_table_lock; /* Frame table lock */

/*
 * Set up frame table
 */
void frame_init()
{

    list_init(&frame_list);
    lock_init(&frame_table_lock);

    if (!lock_held_by_current_thread(&frame_table_lock))
        lock_acquire(&frame_table_lock);
    void *addr = palloc_get_page(PAL_USER | PAL_ZERO);
    /* Creates a frame struct for every page created, and puts it into list */
    while (addr != NULL)
    {

        struct frame *frame_entry = malloc(sizeof(struct frame));
        frame_entry->pinned = false;
        frame_entry->page = NULL;
        frame_entry->paddr = addr;
        list_push_front(&frame_list, &frame_entry->elem);
        addr = palloc_get_page(PAL_USER | PAL_ZERO);
    }
    lock_release(&frame_table_lock);
}

/**
 * Find a usable frame for an incoming frame request.
 */
struct frame *find_frame(struct spt_entry * page)
{
    if (!lock_held_by_current_thread(&frame_table_lock))
    {
        lock_acquire(&frame_table_lock);
    }
    for (struct list_elem *e = list_begin(&frame_list); e != list_end(&frame_list); e = list_next(e))
    {
        struct frame *f = list_entry(e, struct frame, elem);
        if (f->pinned)
            continue;

        else if (f->page == NULL)
        {
            list_remove(e);
            list_push_back(&frame_list, e);
            f->page = page;
            page->frame = f;
            lock_release(&frame_table_lock);
            return f;
        }
        
    }
    struct frame *f;
        
    f = evict();
    ASSERT(f != NULL);
    list_remove(&f->elem);
    list_push_back(&frame_list, &f->elem);
    f->page = page;
    page->frame = f;
    lock_release(&frame_table_lock);
    return f;
}

/*
 * Frees frame
 */
void free_frame(struct frame *f)
{
    if (!lock_held_by_current_thread(&frame_table_lock))
    {
        lock_acquire(&frame_table_lock);
    }
    f->pinned = false;
    f->page = NULL;
    list_remove(&f->elem);
    list_push_back(&frame_list, &f->elem);
    lock_release(&frame_table_lock);
}

/*
 * Eviction - choose a frame to clear out and saves/swaps as needed
 */
struct frame *evict(void)
{
    struct frame *candidate = NULL;
    for (struct list_elem *e = list_begin(&frame_list); e != list_end(&frame_list); e = list_next(e)) {
        struct frame *f = list_entry(e, struct frame, elem);
        ASSERT(f != NULL);
        ASSERT(f->page != NULL);
        if (!f->page->pinned) {
            bool page_accessed = pagedir_is_accessed(f->page->pagedir, f->page->vaddr);
            ASSERT(f->page != NULL);
            if (!page_accessed) {
                candidate = f;
                break;
            } else {
                pagedir_set_accessed(f->page->pagedir, f->page->vaddr, false);
            }
        }
    }
    if (candidate == NULL) {
        /* 2nd run. Unless all the pages are pinned, this should find a candidate. */
        for (struct list_elem *e = list_begin(&frame_list); e != list_end(&frame_list); e = list_next(e)) {
            struct frame *f = list_entry(e, struct frame, elem);
            if (!f->page->pinned) {
                bool page_accessed = pagedir_is_accessed(f->page->pagedir, f->page->vaddr);
                if (!page_accessed) {
                    candidate = f;
                    break;
                } else {
                    pagedir_set_accessed(f->page->pagedir, f->page->vaddr, false);
                }
            }
        }
    }
    ASSERT(candidate != NULL);
    ASSERT(candidate->page != NULL);
    if (candidate->page->page_status == 0) {
        if (pagedir_is_dirty(candidate->page->pagedir, candidate->page->vaddr)) {
            lock_file();
            file_write_at(candidate->page->file, candidate->page->vaddr, candidate->page->bytes_read, candidate->page->offset);
            unlock_file();
        }
    } else {
        candidate->page->pinned = true;
        swap_insert(candidate->page);
        candidate->page->pinned = false;
    }
    pagedir_clear_page(candidate->page->pagedir, candidate->page->vaddr);
    return candidate;
}