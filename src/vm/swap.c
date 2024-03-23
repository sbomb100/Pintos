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
bool swap_init_v = false;
// Block sector size is 512 bytes => 8 sectors = 1 page

// create bitmap
void swap_init(void)
{
    
    if(!swap_init_v){
    lock_init(&block_lock);
    used_blocks = bitmap_create(BLOCK_SECTOR_SIZE);
    block_swap = block_get_role(BLOCK_SWAP);
    }
    swap_init_v = true;
}

void swap_insert(struct spt_entry *p)
{
    //swap_init();
    lock_acquire(&block_lock);
    char *c = (char *)p->frame->paddr;
    size_t sector_num = bitmap_scan_and_flip(used_blocks, 0, 1, false);
    p->swap_index = sector_num;
    p->page_status = 1;
    for (int i = 0; i < (PGSIZE / BLOCK_SECTOR_SIZE); i++)
    {
        block_write(block_swap, sector_num * (PGSIZE / BLOCK_SECTOR_SIZE) + i, (uint8_t *) c + i * BLOCK_SECTOR_SIZE);
    }
    lock_release(&block_lock);
}

/* Read from swap into the page */
void swap_get(struct spt_entry *p)
{
    lock_acquire(&block_lock);
    char *c = (char *)p->frame->paddr;
    unsigned read_sector = p->swap_index;
    int i;
    for (i = 0; i < (PGSIZE / BLOCK_SECTOR_SIZE); i++)
    {
        //instead of c maybe (uint8_t *) upage + i * BLOCK_SECTOR_SIZE) FIX?
        block_read(block_swap, read_sector * (PGSIZE / BLOCK_SECTOR_SIZE) + i, (uint8_t *) c + i * BLOCK_SECTOR_SIZE);
    }
    bitmap_reset(used_blocks, read_sector);
    lock_release(&block_lock);
}

void swap_free(struct spt_entry *p)
{
    lock_acquire(&block_lock);
    bitmap_reset(used_blocks, p->swap_index);
    lock_release(&block_lock);
}