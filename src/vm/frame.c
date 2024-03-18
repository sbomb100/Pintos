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
    void* addr = palloc_get_page(PAL_USER | PAL_ZERO);
    while(addr != NULL){ //creates a frame struct for every page created, and puts it into list
        struct frame* frame_entry = malloc(sizeof(struct frame));
        frame_entry->pinned = false;
        frame_entry->page = NULL;
        frame_entry->paddr = addr;
        list_push_back(&frame_list, &frame_entry->elem);
        addr = palloc_get_page(PAL_USER | PAL_ZERO);

    }
}

/**
 * find a empty frame
*/
struct frame* find_frame(){
    struct list_elem *e;
    lock_acquire(&frame_table_lock);
    for (e = list_begin(&frame_list); e != list_end (&frame_list); e = list_next (e)) {
        struct frame *f = list_entry (e, struct frame, elem);
        if(f->page == NULL){
            lock_release(&frame_table_lock);
            return f;
        }
    }
    lock_release(&frame_table_lock);
    //no empty frame found
    //could this just become LRU by grabbing the first frame, then once grabbed push it to the back instead?
    //struct frame* f = evict();
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
    f->page = NULL;
    lock_release(&frame_table_lock);
}