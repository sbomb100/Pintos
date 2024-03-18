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

static struct list frame_list; //frame list
static struct lock frame_table_lock; //lock for the frame table

/**
 * Set up frame table
*/
void frame_init(){

    list_init(&frame_list);
    lock_init(&frame_table_lock);

    if ( !lock_held_by_current_thread(&frame_table_lock)) lock_acquire(&frame_table_lock);
    void* addr = palloc_get_page(PAL_USER | PAL_ZERO);
    while(addr != NULL){ //creates a frame struct for every page created, and puts it into list
        struct frame* frame_entry = malloc(sizeof(struct frame));
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
    if ( !lock_held_by_current_thread(&frame_table_lock)) lock_acquire(&frame_table_lock);
    for ( struct list_elem * e = list_begin(&frame_list); e != list_end(&frame_list); e = list_next(e) ) {
        struct frame * f = list_entry(e, struct frame, elem);
        if ( f->pinned ) continue;

        swap_insert(f->page);
        list_remove(e);
        list_push_back(&frame_list, e);
        lock_release(&frame_table_lock);
        return f;
    }

    lock_release(&frame_table_lock);
    return NULL;
}

/**
 * 
*/
void frame_allocate_page(struct spt_entry* page){
    //lock_acquire(&frame_table_lock);
    struct frame* f = find_frame();
    f->page = page;
    page->frame = f;
    //lock_release(&frame_table_lock);
}


/**
 * frees frame
*/
void free_frame(struct frame *f){
    lock_acquire(&frame_table_lock);
    f->pinned = false;
    f->page = NULL;
    f->paddr = addr;
    f->unused_count = 0;
    list_remove(f->elem);
    list_push_back(&frame_list, &f->elem);
    lock_release(&frame_table_lock);
}