#include "vm/swap.h"
#include <stdio.h>
#include <stdint.h>
#include <bitmap.h>
#include "devices/block.h"
#include "vm/frame.h"
#include "threads/vaddr.h"
#include "threads/malloc.h"
#include "threads/palloc.h"
#include "threads/synch.h"

static struct bitmap *used_blocks;
struct block *block_swap;
static struct lock block_lock;
// Block sector size is 512 bytes => 8 sectors = 1 page

// create bitmap
void swap_init(void)
{
    lock_init(&block_lock);
    used_blocks = bitmap_create(1024);
    block_swap = block_get_role(BLOCK_SWAP);
}

void swap_insert(struct spt_page_entry *p)
{
    lock_acquire(&block_lock);
    char *c = (char *)p->frame->paddr;
    size_t sector_num = bitmap_scan_and_flip(used_blocks, 0, 1, false);
    p->swap_block = sector_num;
    for (int i = 0; i < 8; i++)
    {
        block_write(block_swap, sector_num * 8 + i, c);
        c += 512;
    }
    lock_release(&block_lock);
}

/* Read from swap into the page */
void swap_get(struct spt_page_entry *p)
{
    lock_acquire(&block_lock);
    char *c = (char *)p->frame->paddr;
    unsigned read_sector = p->swap_block;
    int i;
    for (i = 0; i < 8; i++)
    {
        block_read(block_swap, read_sector * 8 + i, c);
        c += 512;
    }
    bitmap_reset(used_blocks, read_sector);
    lock_release(&block_lock);
}

void swap_free(struct spt_page_entry *p)
{
    lock_acquire(&block_lock);
    bitmap_reset(used_blocks, p->swap_block);
    lock_release(&block_lock);
}