//The header file for a supplemental page table entry

#include "vm/frame.h"

#include <debug.h>
#include <inttypes.h>
#include <round.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "userprog/pagedir.h"
#include "userprog/syscall.h"
#include "userprog/process.h"
#include "filesys/directory.h"
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "threads/flags.h"
#include "threads/init.h"
#include "threads/interrupt.h"
#include "threads/palloc.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "threads/synch.h"
#include "lib/kernel/hash.h"

struct spt_page_entry
{
	// hash table elem
	struct hash_elem elem;
    //virtual address
    void *vaddr;       
    struct frame *frame;
    int page_status; //0: all 0 1: in swap 2: in file 3: in frame

	// if we mmap file
	struct file * file;
    size_t bytes_read;
	off_t offset;
	
	bool writable;
    struct spinlock lock; //syncro lock
	struct thread * t; 

    //maybe?
    size_t swap_index; //to know where is in swap fastest
};

//need cuntions for
//getting spt entry from hash
//putting in hash
//removing from hash
//initialization