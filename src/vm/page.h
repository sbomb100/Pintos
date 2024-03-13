#ifndef VM_PAGE_H
#define VM_PAGE_H

//The header file for a supplemental page table entry


#include <stdint.h>
#include <hash.h>
#include "threads/palloc.h"
#include "filesys/file.h"
#include "vm/frame.h"
#include "threads/synch.h"

struct spt_entry
{
	// hash table elem
	struct hash_elem elem;
    //virtual address
    void *vaddr;       
    struct frame *frame;
    int page_status; //0: all 0 1: in swap 2: in file 3: in frame
    uint32_t *pagedir; //holder for owner page directory, used instead of holding owner thread
	// if we mmap file
	struct file * file;
    size_t bytes_read;
	off_t offset;
	bool is_stack;
	bool writable;
    struct spinlock lock; //syncro lock
	struct thread * t; 
    int swap_block; // FOR SWAP TABLE
    //maybe?
    int swap_index; //to know where is in swap fastest
};

//need funcitons for
//getting spt entry from hash : DONE
//putting in hash : DONE
//removing from hash : DONE
//initialization: initialized in process.c

// Moved to process.c
// unsigned page_hash (const struct hash_elem *, void *);
// bool is_page_before (const struct hash_elem *, const struct hash_elem *, void *);
// void destroy_page (struct hash_elem *, void *);
struct spt_entry * get_page_from_hash (void *);


#endif