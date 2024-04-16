#include "filesys/cache.h"
#include "filesys/filesys.h"
#include "devices/timer.h"
#include "threads/thread.h"
#include "threads/malloc.h"
#include <string.h>
#include <stdio.h>

#define MAX_CACHE_SIZE 64

static struct cache_block cache[MAX_CACHE_SIZE];
struct lock all_cache_lock;
/* Number of currently allocated caches */
static int num_cache_blocks = 0;



static struct cache_block *find_cache_block (block_sector_t sector);
static struct cache_block *is_in_cache (block_sector_t sector);
static struct cache_block *cache_eviction (void);
static void write_behind (void *aux);
static void read_ahead (void *aux);

struct read_ahead_sector {
    block_sector_t sector;
    struct list_elem elem;
};

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
    lock_init(&all_cache_lock);
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
    cond_init(&read_ahead_cond);
    thread_create("write_behind", NICE_DEFAULT, write_behind, NULL);
    thread_create("read_ahead", NICE_DEFAULT, read_ahead, NULL);
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
    lock_acquire(&all_cache_lock);
    struct cache_block *b = find_cache_block(sector);
    lock_acquire(&b->cache_lock);
    if (b->sector != sector) {
        // if the block is dirty, write it back to disk
        if (b->dirty) {
            block_write(fs_device, b->sector, b->data);
            b->dirty = false;
            b->valid = false;
        }
        // read the block from disk
        block_read(fs_device, sector, b->data);
        b->sector = sector;
        cache_misses++;
    } else {
        cache_hits++;
    }
    if (exclusive) {
        if (b->num_readers > 0 || b->num_writers > 0) {
            cond_wait(&b->is_available, &b->cache_lock);
        }
        b->num_writers++;
    } else {
        b->num_readers++;
    }
    lock_release(&b->cache_lock);
    lock_release(&all_cache_lock);
    return b;

}

/*
 * If the block is already in the cache, return. If not, look for free space.
 * If there aren't any free spaces, evict.
 */
static struct cache_block *find_cache_block (block_sector_t sector) {
    // lock_acquire(&all_cache_lock);
    struct cache_block *b = is_in_cache(sector);
    if (b == NULL) { /* Cache Miss */
        if (num_cache_blocks < MAX_CACHE_SIZE) {
            b = &cache[num_cache_blocks];
            num_cache_blocks++;

            // /* Cache block init */
            // b->sector = sector;
            // b->dirty = false;
            // b->valid = true;
            // b->num_readers = 0;
            // b->num_writers = 0;
            // b->num_pending_requests = 0;

        } else {
            b = cache_eviction();
        }
    }
    b->use_bit = true;
    // lock_release(&all_cache_lock);
    return b;
}

static struct cache_block *is_in_cache (block_sector_t sector) {
    for (int i = 0; i < MAX_CACHE_SIZE; i++) {
        if (cache[i].sector == sector) {
            return &cache[i];
        }
    }
    return NULL;
}

/*
 * Evicts a cache block (eventually via clock algorithm :D)
 */
static struct cache_block *cache_eviction (void) {
    struct cache_block *candidate = NULL;
    while (candidate == NULL) {
        for (int i = 0; i < MAX_CACHE_SIZE; i++) {
            if (!cache[i].use_bit) {
                candidate = &cache[i];
                break;
            } else {
                cache[i].use_bit = false;
            }
        }
    }
    if (candidate->dirty) {
        block_write(fs_device, candidate->sector, candidate->data);
        candidate->dirty = false;
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
    b->use_bit = true;
    cond_broadcast(&b->is_available, &b->cache_lock);
    lock_release(&b->cache_lock);
    
}

/* 
 * Read cache block from disk, returns pointer to data
 */
void * cache_read_block (struct cache_block *b) {
    lock_acquire(&b->cache_lock);
    b->use_bit = true;
    if (!b->valid) {
        block_read(fs_device, b->sector, b->data);
        b->valid = true;
    }
    lock_release(&b->cache_lock);
    return b->data;
}

/*
 * Zeroes out the block and returns a pointer to the zeroed data.
 */
void * cache_zero_block (struct cache_block *b) {
    lock_acquire(&b->cache_lock);
    memset(b->data, 0, BLOCK_SECTOR_SIZE);
    b->valid = true;
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

void flush_cache(void) {
    for (int i = 0; i < MAX_CACHE_SIZE; i++) {
        struct cache_block *b = &cache[i];
        lock_acquire(&b->cache_lock);
        if (b->dirty) {
            block_write(fs_device, b->sector, b->data);
            b->dirty = false;
        }
        lock_release(&b->cache_lock);

    }
}

/* 
 * Writes dirty blocks to disk on a regular interval asynchronously.
 */
static void write_behind (void *aux UNUSED) {
    for (;;) {
        timer_sleep(1000);
        flush_cache();
    }
}


void send_read_ahead_request(block_sector_t ahead_sector) {
    struct read_ahead_sector *ras = malloc(sizeof(struct read_ahead_sector));
    if (ras == NULL) {
        return;
    }
    ras->sector = ahead_sector;

    lock_acquire(&read_ahead_lock);
    list_push_back(&read_ahead_list, &ras->elem);
    cond_signal(&read_ahead_cond, &read_ahead_lock);
    lock_release(&read_ahead_lock);
}


static void read_ahead (void *aux UNUSED) {
    for (;;) {
        lock_acquire(&read_ahead_lock);
        while (list_empty(&read_ahead_list)) {
            cond_wait(&read_ahead_cond, &read_ahead_lock);
        }
        struct read_ahead_sector *ras = list_entry(list_pop_front(&read_ahead_list), struct read_ahead_sector, elem);
        lock_release(&read_ahead_lock);
        struct cache_block *b = cache_get_block(ras->sector, false);
        cache_put_block(b);
        free(ras);
    }
}