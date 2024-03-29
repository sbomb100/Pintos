#include "filesys/cache.h"
#include "devices/timer.h"

#define MAX_CACHE_SIZE 64

static struct cache_block cache[MAX_CACHE_SIZE];

static struct cache_block *find_cache_block (block_sector_t sector);
static void write_behind (void);
static void read_ahead (void);

/* 
 * Initializes the cache. 
 */
void cache_init (void) {
    for (int i = 0; i < MAX_CACHE_SIZE; i++) {
        cache[i]->sector = -1;
        cache[i]->dirty = false;
        cache[i]->valid = false;
        cache[i]->num_readers = 0;
        cache[i]->num_writers = 0;
        cache[i]->num_pending_requests = 0;
        lock_init(&cache[i]->lock);
        cond_init(&cache[i]->signaling_variable);
    }
    thread_create("write_behind", PRI_DEFAULT, write_behind, NULL);
    thread_create("read_ahead", PRI_DEFAULT, read_ahead, NULL);
}

/* 
 * Shuts down the cache. 
 */
void cache_shutdown(void) {
    flush_cache();
    // among other things...
}

/*
 * If the block is already in the cache, return. If not, look for free space.
 * If there aren't any free spaces, evict using the clock algorithm.
 */
struct cache_block * cache_get_block (block_sector_t sector, bool exclusive) {
    for (int i = 0; i < MAX_CACHE_SIZE; i++) {
        struct cache_block *cache = &cache[i];
        lock_acquire(&cache->lock);
        
    }
}

void cache_put_block (struct cache_block *b) {
    
}

void cache_read_block (struct cache_block *b) {
    
}

/*
 * Zeroes out the block and returns a pointer to the zeroed data.
 */
void * cache_zero_block (struct cache_block *b) {
    lock_acquire(&b->lock);
    memset(b->data, 0, BLOCK_SECTOR_SIZE);
    lock_release(&b->lock);
    return b->data;
}

/* 
 * Marks a block as dirty. 
 */
void cache_mark_block_dirty(struct cache_block *b) {
    lock_acquire(&b->lock);
    b->dirty = true;
    lock_release(&b->lock);
}

/*
 * Looks for a buffer in the cache. If one cannot be found, evict?
 */
static struct cache_block * find_cache_block (block_sector_t sector) {
    return NULL;
}

void flush_cache(void) {
    for (int i = 0; i < MAX_CACHE_SIZE; i++) {
        struct cache_block *cache = &cache[i];
        // TODO implement :D
    }
}

/* 
 * Writes dirty blocks to disk on a regular interval asynchronously.
 */
static void write_behind (void) {
    for (;;) {
        timer_sleep(30000); /* Sleeps for 30000 ticks. Adjust as necessary. */
        flush_cache();
    }
}

static void read_ahead (void) {

}