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
#include "vm/page.h"

static struct bitmap *used_blocks;
struct block *block_swap;
static struct lock block_lock;

/*
 * Creates bitmap
 */
void swap_init(void)
{
    lock_init(&block_lock);
    used_blocks = bitmap_create(BLOCK_SECTOR_SIZE);
    block_swap = block_get_role(BLOCK_SWAP);
}

/*
 * Write the page to swap
 */
void swap_insert(struct spt_entry *p)
{
    lock_acquire(&block_lock);
    size_t sector_num = bitmap_scan_and_flip(used_blocks, 0, 1, false);
    ASSERT(sector_num!=BITMAP_ERROR);
    p->swap_index = sector_num;
    p->page_status = 1;
    for (int i = 0; i < (PGSIZE / BLOCK_SECTOR_SIZE); i++)
    {
        block_write(block_swap, sector_num * (PGSIZE / BLOCK_SECTOR_SIZE) + i, (uint8_t *) p->frame->paddr + i * BLOCK_SECTOR_SIZE);
    }
    lock_release(&block_lock);
}

/*
 * Read from swap into the page
 */
void swap_get(struct spt_entry *p)
{
    lock_acquire(&block_lock);
    char *c = (char *)p->frame->paddr;
    unsigned read_sector = p->swap_index;
    int i;
    for (i = 0; i < (PGSIZE / BLOCK_SECTOR_SIZE); i++)
    {
        block_read(block_swap, read_sector * (PGSIZE / BLOCK_SECTOR_SIZE) + i, (uint8_t *) c + i * BLOCK_SECTOR_SIZE);
    }
    bitmap_reset(used_blocks, read_sector);
    lock_release(&block_lock);
}

/*
 * Free the swap block
 */
void swap_free(struct spt_entry *p)
{
    lock_acquire(&block_lock);
    bitmap_reset(used_blocks, p->swap_index);
    lock_release(&block_lock);
}