#ifndef FILESYS_CACHE_H
#define FILESYS_CACHE_H

#include "devices/block.h"
#include "threads/synch.h"
#include "filesys/inode.h"

struct cache_block {
    block_sector_t sector;
    bool dirty; /* True if block has been modified, false otherwise */
    bool valid; /* True if block is valid, false otherwise */
    int num_readers; /* Number of readers currently accessing the block */
    int num_writers; /* Number of writers currently accessing the block */
    int num_pending_requests; /* Number of pending requests for the block */
    struct lock cache_lock; /* Lock for the cache block */
    struct condition is_available;
    uint8_t data[BLOCK_SECTOR_SIZE];
    struct list_elem read_ahead_elem;
};

/* Intializes the cache */
void cache_init(void);
/* Either grant exclusive or shared access */
struct cache_block * cache_get_block (block_sector_t sector, bool exclusive);
/* Release access to cache block */
void cache_put_block(struct cache_block *b);
/* Read cache block from disk, returns pointer to data */
void *cache_read_block(struct cache_block *b);
/* Fill cache block with zeros, returns pointer to data */
void *cache_zero_block(struct cache_block *b);
/* Mark cache block dirty (must be written back) */
void cache_mark_block_dirty(struct cache_block *b);
/* Closes down the cache, writing back all dirty blocks, etc. */
void cache_shutdown(void);
/* Writes all dirty blocks back to disk */
void flush_cache(void);


#endif /* filesys/cache.h */