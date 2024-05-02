/*
 * Simple, 64-bit allocator based on implicit free lists,
 * first fit placement, and boundary tag coalescing, as described
 * in the CS:APP2e text. Blocks must be aligned to 16 byte
 * boundaries. Minimum block size is 16 bytes.
 *
 * This version is loosely based on
 * http://csapp.cs.cmu.edu/3e/ics3/code/vm/malloc/mm.c
 * but unlike the book's version, it does not use C preprocessor
 * macros or explicit bit operations.
 *
 * It follows the book in counting in units of 4-byte words,
 * but note that this is a choice (my actual solution chooses
 * to count everything in bytes instead.)
 *
 * You should use this code as a starting point for your
 * implementation.
 *
 * First adapted for CS3214 Summer 2020 by gback
 */

#include "lib/user/malloc.h"
#include "lib/user/list.h"
#include <string.h>
#include <stdbool.h>
#include "lib/user/pthread.h"
#include "lib/user/syscall.h"
#include <stdio.h>

#define ALIGNMENT 8
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~(ALIGNMENT-1))

typedef struct block {
    size_t size;
    struct block *next;
} block_t;

block_t *head = NULL;

pthread_lock_t malloc_lock;
static bool initialized = false;

// create init function
int malloc_init(void) {
    pthread_mutex_init(&malloc_lock);
    return 0;
}

void *malloc(size_t size) {
  if (!initialized) {
    malloc_init();
    initialized = true;
  }
    size_t total_size;
    block_t *block;

    pthread_mutex_lock(&malloc_lock);

    // Align the size
    size = ALIGN(size);

    if (head == NULL) {
        // First call to malloc, initialize head
        block = sbrk(0);
        if (sbrk(sizeof(block_t) + size) == (void *)-1) {
            pthread_mutex_unlock(&malloc_lock);
            return NULL;
        }
        block->size = size;
        block->next = NULL;
        head = block;
    } else {
        block = head;
        while (block->next != NULL)
            block = block->next;

        total_size = sizeof(block_t) + size;
        if (sbrk(total_size) == (void *)-1) {
            pthread_mutex_unlock(&malloc_lock);
            return NULL;
        }

        block->next = sbrk(0);
        block->next->size = size;
        block->next->next = NULL;
    }

    pthread_mutex_unlock(&malloc_lock);

    return (void *)(block + 1);
}

void free(void *ptr UNUSED) {
    return;
}
