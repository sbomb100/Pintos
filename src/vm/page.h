#ifndef VM_PAGE_H
#define VM_PAGE_H

#include <stdint.h>
#include <hash.h>
#include "threads/palloc.h"
#include "filesys/file.h"
#include "vm/frame.h"
#include "threads/synch.h"

struct spt_entry
{
	struct hash_elem elem; /* Hash table elem */
    void *vaddr; /* Page's virtual address */
    struct frame *frame; /* Frame that holds this page */
    int page_status; /* 0: mmaped 1: in swap 2: in file 3: in frame */
    uint32_t *pagedir; /* Holder for owner page directory, used instead of holding owner thread */

    /* MMAP */
	struct file * file;
    size_t bytes_read;
    size_t bytes_zero;
	off_t offset;

	bool is_stack;
	bool writable;
    bool pinned;
    struct spinlock lock;
    int swap_index; /* Used for swap table */
};

unsigned page_hash (const struct hash_elem *, void *);
bool is_page_before (const struct hash_elem *, const struct hash_elem *, void *);
void destroy_page (struct hash_elem *, void *);
struct spt_entry * get_page_from_hash (void *);

#endif