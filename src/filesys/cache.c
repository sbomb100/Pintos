#include "filesys/cache.h"
#include "filesys/filesys.h"
#include "devices/timer.h"
#include "threads/thread.h"
#include <string.h>

#define MAX_CACHE_SIZE 64

static struct cache_block cache[MAX_CACHE_SIZE];
// struct lock cache_lock;

// static struct cache_block *find_cache_block (block_sector_t sector);
static void write_behind (void *aux);
static void read_ahead (void *aux);

/* Read ahead daemon support */
struct condition read_ahead_cond;
struct lock read_ahead_lock;
struct list read_ahead_list;

/* 
 * Initializes the cache. 
 */
void cache_init (void) {
    // lock_init(&cache_lock);
    for (int i = 0; i < MAX_CACHE_SIZE; i++) {
        cache[i].sector = -1;
        cache[i].dirty = false;
        cache[i].valid = false;
        cache[i].num_readers = 0;
        cache[i].num_writers = 0;
        cache[i].num_pending_requests = 0;
        lock_init(&cache[i].cache_lock);
        cond_init(&cache[i].is_available);
    }
    list_init(&read_ahead_list);
    lock_init(&read_ahead_lock);
    thread_create("write_behind", NICE_DEFAULT, write_behind, NULL);
    thread_create("read_ahead", NICE_DEFAULT, read_ahead, NULL);
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
    struct cache_block *free_block = NULL;
    // lock_acquire(&cache_lock);
    for (int i = 0; i < MAX_CACHE_SIZE; i++) {
        struct cache_block *temp = &cache[i];
        lock_acquire(&temp->cache_lock);
        if (temp->sector == sector) {
            if (exclusive) {
                temp->num_writers++;
                if (temp->num_readers > 0 || temp->num_pending_requests > 0) {
                    cond_wait(&temp->is_available, &temp->cache_lock);
                }
            } else {
                temp->num_readers++;
                // not sure if we need a wait here
            }
            lock_release(&temp->cache_lock);
            return temp;
        } else if (free_block == NULL && !temp->valid) {
            free_block = temp;
        }
        lock_release(&temp->cache_lock);
    }
    if (free_block != NULL) {
        lock_acquire(&free_block->cache_lock);
        free_block->sector = sector;
        free_block->dirty = false;
        free_block->valid = true;
        free_block->num_readers = 1;
        lock_release(&free_block->cache_lock);
        return free_block;
    }
    /* Eviction! */
    for (int i = 0; i < MAX_CACHE_SIZE; i++) {
        struct cache_block *candidate = &cache[i];
        lock_acquire(&cache->cache_lock);
        // TODO stuff
        lock_release(&cache->cache_lock);
    }
    return NULL;
}

/*
 * Remove cache block from cache. If the block is dirty, write it to disk.
 */
void cache_put_block (struct cache_block *b) {
    lock_acquire(&b->cache_lock);
    if (b->dirty) {
        block_write(fs_device, b->sector, b->data);
    }
    b->dirty = false;
    b->sector = -1;
    b->valid = false;
    b->num_readers = 0;
    b->num_writers = 0;
    b->num_pending_requests = 0;
    // cond_broadcast(&b->signaling_variable, &b->cache_lock);
    lock_release(&b->cache_lock);
}

/* 
 * Read cache block from disk, returns pointer to data
 */
void * cache_read_block (struct cache_block *b) {
    lock_acquire(&b->cache_lock);
    block_read(fs_device, b->sector, b->data);
    lock_release(&b->cache_lock);
    return b->data;
}

/*
 * Zeroes out the block and returns a pointer to the zeroed data.
 */
void * cache_zero_block (struct cache_block *b) {
    lock_acquire(&b->cache_lock);
    memset(b->data, 0, BLOCK_SECTOR_SIZE);
    lock_release(&b->cache_lock);
    return b->data;
}

/* 
 * Marks a block as dirty. 
 */
void cache_mark_block_dirty(struct cache_block *b) {
    lock_acquire(&b->cache_lock);
    b->dirty = true;
    lock_release(&b->cache_lock);
}

/*
 * Looks for a buffer in the cache. If one cannot be found, evict?
 */
// static struct cache_block * find_cache_block (block_sector_t sector) {
//     return NULL;
// }

void flush_cache(void) {
    for (int i = 0; i < MAX_CACHE_SIZE; i++) {
        struct cache_block *cache = &cache[i];
        // will it try to lock an already existing lock?
    }
}

/* 
 * Writes dirty blocks to disk on a regular interval asynchronously.
 */
static void write_behind (void *aux UNUSED) {
    for (;;) {
        timer_sleep(30000); /* Sleeps for 30000 ticks. Adjust as necessary. */
        flush_cache();
    }
}

static void read_ahead (void *aux UNUSED) {
    for (;;) {
        struct cache_block *cache = NULL;
        if (list_empty(&read_ahead_list)) {
            cond_wait(&read_ahead_cond, &read_ahead_lock);
        }
        cache = list_entry(list_pop_front(&read_ahead_list), struct cache_block, read_ahead_elem);
        block_read(fs_device, cache->sector, cache->data);
    }
}