#include "filesys/cache.h"
#include "filesys/filesys.h"
#include "devices/timer.h"
#include "threads/thread.h"
#include <string.h>
#include <stdio.h>

#define MAX_CACHE_SIZE 64

static struct cache_block cache[MAX_CACHE_SIZE];
// struct lock cache_lock;



static struct cache_block *find_cache_block (block_sector_t sector);
static struct cache_block *cache_eviction (void);
// static void write_behind (void *aux);
// static void read_ahead (void *aux);

/* Read ahead daemon support */
struct condition read_ahead_cond;
struct lock read_ahead_lock;
struct list read_ahead_list;

// testing variables
int cache_hits = 0;
int cache_misses = 0;

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
    // thread_create("write_behind", NICE_DEFAULT, write_behind, NULL);
    // thread_create("read_ahead", NICE_DEFAULT, read_ahead, NULL);
}

/* 
 * Shuts down the cache. 
 */
void cache_shutdown(void) {
    flush_cache();
    printf("Cache hits: %d\n", cache_hits);
    printf("Cache misses: %d\n", cache_misses);
    // among other things...
}

/*
 * Acquires a cache block for the given sector.
 */
struct cache_block * cache_get_block (block_sector_t sector, bool exclusive) {
    
    struct cache_block *b = find_cache_block(sector);
    lock_acquire(&b->cache_lock);
    if (b->sector != sector) {
        // if the block is dirty, write it back to disk
        if (b->dirty) {
            block_write(fs_device, b->sector, b->data);
            b->dirty = false;
        }
        // read the block from disk
        block_read(fs_device, sector, b->data);
        b->sector = sector;
        cache_misses--;
    } else {
        cache_hits++;
    }
    if (exclusive) {
        if (b->num_readers > 0 || b->num_writers > 0) {
            cond_wait(&b->is_available, &b->cache_lock);
        }
        b->num_writers++;
    } else {
        if (b->num_writers > 0) {
            cond_wait(&b->is_available, &b->cache_lock);
        }
        b->num_readers++;
    }
    lock_release(&b->cache_lock);
    return b;

}

/*
 * If the block is already in the cache, return. If not, look for free space.
 * If there aren't any free spaces, evict.
 */
static struct cache_block *find_cache_block (block_sector_t sector) {
    for (int i = 0; i < MAX_CACHE_SIZE; i++) { // probably condense-able into a single loop
        if (cache[i].sector == sector) {
            return &cache[i];
        }
    }
    for (int i = 0; i < MAX_CACHE_SIZE; i++) {
        if (!cache[i].valid) {
            return &cache[i];
        }
    }
    // evict a cache block
    struct cache_block *evicted = cache_eviction();
    return evicted;
    // return &cache[0];
}

/*
 * Evicts a cache block (eventually via clock algorithm :D)
 */
static struct cache_block *cache_eviction (void) {
    struct cache_block *candidate = NULL;
    while (candidate == NULL) {
        for (int i = 0; i < MAX_CACHE_SIZE; i++) {
            if (cache[i].num_readers == 0 && cache[i].num_writers == 0) {
                candidate = &cache[i];
                break;
            }
        }
    }
    ASSERT(candidate != NULL);
    return candidate;
}


/* 
 * Release access to cache block.
 */
void cache_put_block (struct cache_block *b) {
    // ASSERT(b->valid);
    lock_acquire(&b->cache_lock);
    if (b->num_writers > 0) {
        b->num_writers--;
    } else if (b->num_readers > 0) {
        b->num_readers--;
    }
    cond_broadcast(&b->is_available, &b->cache_lock);
    lock_release(&b->cache_lock);
    
}

/* 
 * Read cache block from disk, returns pointer to data
 */
void * cache_read_block (struct cache_block *b) {
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
// static void write_behind (void *aux UNUSED) {
//     for (;;) {
//         timer_sleep(30000); /* Sleeps for 30000 ticks. Adjust as necessary. */
//         flush_cache();
//     }
// }

// static void read_ahead (void *aux UNUSED) {
//     for (;;) {
//         // struct cache_block *cache = NULL;
//         // if (list_empty(&read_ahead_list)) {
//         //     cond_wait(&read_ahead_cond, &read_ahead_lock);
//         // }
//         // cache = list_entry(list_pop_front(&read_ahead_list), struct cache_block, read_ahead_elem);
//         // block_read(fs_device, cache->sector, cache->data);
//     }
// }