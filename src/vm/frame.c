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

static struct list frame_list;       // frame list
static struct lock frame_table_lock; // lock for the frame table

/**
 * Set up frame table
 */
void frame_init()
{

    list_init(&frame_list);
    lock_init(&frame_table_lock);

    if (!lock_held_by_current_thread(&frame_table_lock))
        lock_acquire(&frame_table_lock);
    void *addr = palloc_get_page(PAL_USER | PAL_ZERO);
    while (addr != NULL)
    { // creates a frame struct for every page created, and puts it into list
        
        struct frame *frame_entry = malloc(sizeof(struct frame));
        frame_entry->pinned = false;
        frame_entry->page = NULL;
        frame_entry->paddr = addr;
        frame_entry->unused_count = 0;
        list_push_back(&frame_list, &frame_entry->elem);
        addr = palloc_get_page(PAL_USER | PAL_ZERO);
    }
    lock_release(&frame_table_lock);
}

/**
 * Find a usable frame for an incoming frame request.
*/
struct frame* find_frame(){
    if ( !lock_held_by_current_thread(&frame_table_lock)){ 
        lock_acquire(&frame_table_lock);
    }
    for ( struct list_elem * e = list_begin(&frame_list); e != list_end(&frame_list); e = list_next(e) ) {
        struct frame * f = list_entry(e, struct frame, elem);
        if ( f->pinned ) continue;

        
        if (f->page == NULL){
            list_remove(e);
            list_push_back(&frame_list, e);
            lock_release(&frame_table_lock);
            return f;
        }
        else{
            
            f = evict();
            ASSERT(f!=NULL);
            //swap_insert(f->page);
            list_remove(&f->elem);
            list_push_back(&frame_list, &f->elem);
            lock_release(&frame_table_lock);
            return f;
        
        }
    }

    lock_release(&frame_table_lock);
    return NULL; //only returns null if all is pinned
}

/**
 *
 */
void frame_allocate_page(struct spt_entry *page)
{
    // lock_acquire(&frame_table_lock);
    struct frame *f = find_frame();
    f->page = page;
    page->frame = f;
    // lock_release(&frame_table_lock);
}

/**
 * frees frame
 */
void free_frame(struct frame *f)
{
    lock_acquire(&frame_table_lock);
    f->pinned = false;
    f->page = NULL;
    f->unused_count = 0;
    list_remove(&f->elem);
    list_push_back(&frame_list, &f->elem);
    lock_release(&frame_table_lock);
}

/**
 * eviction - choose a frame to clear out
*/
struct frame* evict(void) {
    // struct frame* candidate;
    // int unused_count_record = -1;
    // for ( struct list_elem * e = list_begin(&frame_list); e != list_end(&frame_list); e = list_next(e) ) {
    //     struct frame * f = list_entry(e, struct frame, elem);
    //     bool page_accessed = pagedir_is_accessed(thread->pagedir, f->page->vaddr);

    //     if(page_accessed){
    //         f->unused_count = 0;
    //     }
    //     else{
    //         f->unused_count++;
    //     }

    //     if(f->unused_count > unused_count_record){
    //         candidate = f;
    //         unused_count_record = f->unused_count;
    //     }

    // }

    // ASSERT(candidate!=NULL);
    // candidate->unused_count=0;
    // return candidate;

    struct frame* candidate;
    
    for ( struct list_elem * e = list_begin(&frame_list); e != list_end(&frame_list); e = list_next(e) ) {
        struct frame * f = list_entry(e, struct frame, elem);
        if(!f->pinned){
            bool page_accessed = pagedir_is_accessed(f->page->t->pagedir, f->page->vaddr);
            
            if(!page_accessed){

                candidate = f;
                break;
            }
            else{
                pagedir_set_accessed (f->page->t->pagedir, f->page->vaddr, false);
            }
        }

    }
    if (candidate == NULL){
        //2nd run. Unless all the pages are pinned, this should find a candidate
        for ( struct list_elem * e = list_begin(&frame_list); e != list_end(&frame_list); e = list_next(e) ) {
            struct frame * f = list_entry(e, struct frame, elem);
            if(!f->pinned){
                bool page_accessed = pagedir_is_accessed(f->page->t->pagedir, f->page->vaddr);
                bool page_dirty = pagedir_is_dirty(f->page->t->pagedir, f->page->vaddr);
                if(!page_accessed){

                    candidate = f;
                    break;
                }
                else{
                    pagedir_set_accessed (f->page->t->pagedir, f->page->vaddr, false);
                }
            }

        }
    }
    ASSERT(candidate!=NULL);

    if(candidate->page->page_status == 0){ //mmap
        if(pagedir_is_dirty(candidate->page->t->pagedir, candidate->page->vaddr)){ //dirty mmap = need to write
            lock_file();
            file_write_at(candidate->page->file, candidate->page->vaddr, candidate->page->bytes_read, candidate->page->offset);
            unlock_file();
        }
        //clean mmap = no need to swap? do I need to destroy the page here or something?
    }
    else{
        swap_insert(candidate->page);
    }
    //else if(candidate->page->page_status == 2) //file
    


    return candidate;
   
    
}