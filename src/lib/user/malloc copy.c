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
#include <stdio.h>
#include "userprog/syscall.h"

#define ALIGNMENT 16

struct boundary_tag {
  int inuse : 1; // inuse bit
  int size : 31; // size of block, in words
                 // block size
};

/* FENCE is used for heap prologue/epilogue. */
const struct boundary_tag FENCE = {.inuse = 1, .size = 0};

/* A C struct describing the beginning of each block.
 * For implicit lists, used and free blocks have the same
 * structure, so one struct will suffice for this example.
 *
 * If each block is aligned at 12 mod 16, each payload will
 * be aligned at 0 mod 16.
 */
struct /*__attribute__((__packed__)) */ block {
  struct boundary_tag header; /* offset 0, at address 12 mod 16 */
  union {            // a union forces us to have a payload or just an elem
    char payload[0]; /* offset 4, at address 0 mod 16 */
    struct list_elem elem; /* pointer elem for explicit list */
  };
};

/* Basic constants and macros */
#define WSIZE                                                                  \
  sizeof(struct boundary_tag)  /* Word and header/footer size (bytes) */
#define MIN_BLOCK_SIZE_WORDS 8 /* Minimum block size in words */
#define CHUNKSIZE (1 << 9)    /* Extend heap by this amount (words) */
#define NUM_SEGREGATES 20      /* Size of the array of free lists 70 10 */

static inline size_t max(size_t x, size_t y) { return x > y ? x : y; }

static inline size_t log_2(size_t n) {
    size_t i = 30 - __builtin_clz(n);

    if (i > NUM_SEGREGATES - 1) {
        return NUM_SEGREGATES - 1;
    }

    return i;
}

/* Pointer comparison to allow address-ordered freeing */
static inline bool compare_pointer(const struct list_elem *a,
                                   const struct list_elem *b, void *aux UNUSED) {
  return b <= a;
}
// takes bytes and converts to size in words
//QUESTION: how does this work?
static size_t align(size_t size) {
  return (size + ALIGNMENT - 1) & ~(ALIGNMENT - 1);
}

static bool is_aligned(size_t size) __attribute__((__unused__));
//block size in bytes
static bool is_aligned(size_t size) { return size % ALIGNMENT == 0; }

/* Global variables */
static struct block *heap_listp = 0; /* Pointer to first block */
static struct list free_list[NUM_SEGREGATES];
static bool left_or_right;

/* Function prototypes for internal helper routines */
static struct block *extend_heap(size_t words);
static struct block *place(struct block *bp, size_t asize);
static struct block *find_fit(size_t asize);
static struct block *coalesce(struct block *bp);

/* Helpful Helpers*/
// static struct block *
// mergeblocks_add_remaining_to_free_list(struct block *left_most_block,
//                                        size_t merged_length,
//                                        size_t b_requested_size);
// static inline void *get_pointer(void *b);

/* Given a block, obtain previous's block footer.
   Works for left-most block also. */
static struct boundary_tag *prev_blk_footer(struct block *blk) {
  return &blk->header - 1;
}

/* Return if block is free */
static bool blk_free(struct block *blk) { return !blk->header.inuse; }

/* Return size of block in words */
static size_t blk_size(struct block *blk) { return blk->header.size; }

/* Given a block, obtain pointer to previous block.
   Not meaningful for left-most block. */
static struct block *prev_blk(struct block *blk) {
  struct boundary_tag *prevfooter = prev_blk_footer(blk);
  ASSERT(prevfooter->size != 0);
  return (struct block *)((void *)blk - WSIZE * prevfooter->size);
}

/* Given a block, obtain pointer to previous block.
   returns zero for left-most block. 
static struct block *prev_blk_or_return_zero(struct block *blk) {
  struct boundary_tag *prevfooter = prev_blk_footer(blk);
  if (prevfooter->size != 0)
    return 0; //
  return (struct block *)((void *)blk - WSIZE * prevfooter->size);
}*/

/* Given a block, obtain pointer to next block.
   Not meaningful for right-most block. */
static struct block *next_blk(struct block *blk) {
  ASSERT(blk_size(blk) != 0);
  return (struct block *)((void *)blk + WSIZE * blk->header.size);
}

/* Given a block, obtain its footer boundary tag */
static struct boundary_tag *get_footer(struct block *blk) {
  return ((void *)blk + WSIZE * blk->header.size) - sizeof(struct boundary_tag);
}

/* Set a block's size and inuse bit in header and footer */
static void set_header_and_footer(struct block *blk, int size, int inuse) {
  blk->header.inuse = inuse;
  blk->header.size = size;
  *get_footer(blk) = blk->header; /* Copy header to footer */
}

/* Mark a block as used and set its size. */
static void mark_block_used(struct block *blk, int size) {
  set_header_and_footer(blk, size, 1);
}

/* Mark a block as free and set its size. */
static void mark_block_free(struct block *blk, int size) {
  set_header_and_footer(blk, size, 0);
}

/*
 * malloc_init - Initialize the memory manager
 */
int malloc_init(void) {
  ASSERT(offsetof(struct block, payload) == 4);
  ASSERT(sizeof(struct boundary_tag) == 4);

  /* Create the initial empty heap */
  for (int i = 0; i < NUM_SEGREGATES; i++) {
    list_init(&free_list[i]);
  }

  left_or_right = true;

  struct boundary_tag *initial = sbrk(4 * sizeof(struct boundary_tag));
  if (initial == NULL)
    return -1;

  /* We use a slightly different strategy than suggested in the book.
   * Rather than placing a min-sized prologue block at the beginning
   * of the heap, we simply place two fences.
   * The consequence is that coalesce() must call prev_blk_footer()
   * and not prev_blk() because prev_blk() cannot be called on the
   * left-most block.
   */
  initial[2] = FENCE; /* Prologue footer */
  heap_listp = (struct block *)&initial[3];
  initial[3] = FENCE; /* Epilogue header */

  /* Extend the empty heap with a free block of CHUNKSIZE bytes */
  if (extend_heap(CHUNKSIZE) == NULL)
    return -1;

  return 0;
}

/*
 * malloc - Allocate a block with at least size bytes of payload
 */
void *malloc(size_t size) {
  struct block *bp;

  /* Ignore spurious requests */
  if (size == 0)
    return NULL;

  /* Adjust block size to include overhead and alignment reqs. */
  /* account for tags */
  size_t bsize = align(size + 2 * sizeof(struct boundary_tag));
  if (bsize < size)
    return NULL; /* integer overflow */

  /* Adjusted block size in words */
  size_t awords =
      max(MIN_BLOCK_SIZE_WORDS, bsize / WSIZE); /* respect minimum size */

  /* Search the free list for a fit */
  if ((bp = find_fit(awords)) != NULL) {
    bp = place(bp, awords);
    ASSERT(is_aligned(blk_size(bp) * WSIZE));
    return bp->payload;
  }

  /* No fit found. Get more memory and place the block */
  size_t extendwords =
      max(awords, CHUNKSIZE); /* Amount to extend heap if no fit */

  void *brk = thread_current()->pcb->heap_break;

  struct block *last_block = (struct block *)brk - 1;
  last_block = prev_blk(last_block);
  //! prev_blk_footer(last_block)->inuse
  if (!prev_blk_footer(last_block)->inuse) {
    extendwords -= prev_blk_footer(last_block)->size;
  }

  if ((bp = extend_heap(extendwords)) == NULL)
    return NULL;

  bp = place(bp, awords);
  ASSERT(is_aligned(blk_size(bp) * WSIZE));
  return bp->payload;
}

/*
 * free - Free a block
 */
void free(void *bp) {
  ASSERT(heap_listp != 0); // assert that malloc_init was called
  if (bp == 0)
    return;

  /* Find block from user pointer */
  struct block *blk = bp - offsetof(struct block, payload);

  mark_block_free(blk, blk_size(blk));
  blk = coalesce(blk);
  list_push_front(&free_list[1], &blk->elem);
}

/*
 * coalesce - Boundary tag coalescing. Return ptr to coalesced block
 */
static struct block *coalesce(struct block *bp) {
  bool prev_alloc =
      prev_blk_footer(bp)->inuse;            /* is previous block allocated? */
  bool next_alloc = !blk_free(next_blk(bp)); /* is next block allocated? */
  size_t size = blk_size(bp);

  if (prev_alloc && next_alloc) { /* Case 1 */
    // both are allocated, nothing to coalesce
    return bp;
  }

  else if (prev_alloc && !next_alloc) { /* Case 2 */
    // combine this block and next block by extending it
    list_remove(&next_blk(bp)->elem);
    mark_block_free(bp, size + blk_size(next_blk(bp)));
  }

  else if (!prev_alloc && next_alloc) { /* Case 3 */
    // combine previous and this block by extending previous
    bp = prev_blk(bp);
    list_remove(&bp->elem);
    mark_block_free(bp, size + blk_size(bp));
  }

  else { /* Case 4 */
    // combine all previous, this, and next block into one
    list_remove(&next_blk(bp)->elem);
    mark_block_free(prev_blk(bp),
                    size + blk_size(next_blk(bp)) + blk_size(prev_blk(bp)));
    bp = prev_blk(bp);
    list_remove(&bp->elem);
  }
  return bp;
}
/*
 * The remaining routines are internal helper routines
 */

/*
 * extend_heap - Extend heap with free block and return its block pointer
 */
static struct block *extend_heap(size_t words) {
  void *bp = sbrk(words * WSIZE);

  if (bp == NULL)
    return NULL;

  /* Initialize free block header/footer and the epilogue header.
   * Note that we overwrite the previous epilogue here. */
  struct block *blk = bp - sizeof(FENCE);
  mark_block_free(blk, words);
  next_blk(blk)->header = FENCE;

  /* Coalesce if the previous block was free */
  blk = coalesce(blk);
  list_push_back(&free_list[1], &blk->elem);
  return blk;
}

/*
 * place - Place block of asize words at start of free block bp
 *         and split if remainder would be at least minimum block size

 idea: alternate left/right allocate?
 */
static struct block *place(struct block *bp, size_t asize) {
  size_t csize = blk_size(bp);

    if ((csize - asize) >= MIN_BLOCK_SIZE_WORDS) {
        if ( left_or_right ) {
            list_remove(&bp->elem);
            mark_block_free(bp, csize - asize);
            list_push_front(&free_list[1], &bp->elem);
            //list_insert_ordered(&free_list[log_2(blk_size(bp))], &bp->elem, compare_pointer, NULL);
            bp = next_blk(bp);
            mark_block_used(bp, asize);
        } else {
            mark_block_used(bp, asize);
            list_remove(&bp->elem);
            struct block *next = next_blk(bp);
            mark_block_free(next, csize - asize);
            list_push_front(&free_list[1], &next->elem);
            //list_insert_ordered(&free_list[log_2(blk_size(next))], &next->elem, compare_pointer, NULL);
        }
        left_or_right = !left_or_right;
    } else {
        mark_block_used(bp, csize);
        list_remove(&bp->elem);
    }

  ASSERT(is_aligned(blk_size(bp) * WSIZE ));
  return bp;
}

/*
 * find_fit - Find a fit for a block with asize words
 */
static struct block *find_fit(size_t asize) {
    /* First fit search */
    struct block * best = NULL;

    for ( struct list_elem * e = list_begin(&free_list[1]); e != list_end(&free_list[1]); e = list_begin(&free_list[1]) ) {
        if ( asize <= prev_blk_footer((struct block *) e)->size ) {
            if ( best == NULL ) {
                best = list_entry(e, struct block, elem);
            }
            else if ( blk_size(best) < prev_blk_footer((struct block *) e)->size) {
                best = list_entry(e, struct block, elem);
            }
        }
        list_remove(e);
        
        struct block * b = list_entry(e, struct block, elem);
        list_push_front(&free_list[log_2(blk_size(b))], &b->elem);
    }

    if ( best != NULL ) return best;

    for (int i = log_2(asize); i < NUM_SEGREGATES; i++) {
        for (struct list_elem *e = list_begin(&free_list[i]);
            e != list_end(&free_list[i]); e = list_next(e)) {
            //struct block *b = list_entry(e, struct block, elem);
            if (asize <= prev_blk_footer((struct block *) e)->size ) {
                return list_entry(e, struct block, elem);
            }
        }
    }

    return NULL; /* No fit */
}

// int mm_checkheap(int verbose) {
//   //--is every free block in the list?--
//   // iterate through the heap
//   // for each free block
//   // check to see if that block is in the fblist
//   struct block *blk = heap_listp;

//   while (blk_size(blk) != 0) {
//     if (blk_free(blk)) {
//       struct list_elem *prev = list_prev(&blk->elem);
//       // if the elem of the free block is not in the free list
//       if (&blk->elem != list_next(prev)) {
//         return 1;
//       }
//       struct list_elem *next = list_next(&blk->elem);
//       if (&blk->elem != list_prev(next)) {
//         return 1;
//       }
//     }
//     // check if form is correct
//     struct block *next_block =
//         (struct block *)((void *)blk + WSIZE * blk->header.size);
//     if (blk != prev_blk(next_block)) {
//       return 1;
//     }
//     // What else should I check?
//     // navigate to the next
//     blk = next_block;
//   }

//   //--is every block in the free list marked as free?--
//   //(at this point we know every fb is in list)
//   // loop through free list
//   // for every element in the list
//   for (int i = 0; i < NUM_SEGREGATES; i++) {
//     for (struct list_elem *e = list_begin(&free_list[i]);
//          e != list_end(&free_list[i]); e = list_next(e)) {
//       struct block *b = list_entry(e, struct block, elem);
//       // check the header of the block to see if it is free
//       if (!blk_free(b)) {
//         return 1; // if not return non zero number and print error
//       }

//       struct block *b_pointer = get_pointer(b);

//       //(double loop is inefficient but may be useful)
//       for (struct list_elem *inner_e = list_next(e);
//            inner_e != list_end(&free_list[i]); inner_e = list_next(e)) {
//         struct block *inner_b = list_entry(inner_e, struct block, elem);
//         // get the block's position and see if any copies exist in the list
//         if (b_pointer == get_pointer(inner_b)) {
//           return 1; // if we do have a copy return a non zero number and print
//                     // error
//         }
//       }
//     }
//   }
//   //--are buddies not merged?--
//   // (at this point we know the fblist is valid and all fb's are on the list)
//   // loop through fbllist
//   // for each fb
//   // check if this fb's footer is next to the next fb's header
//   // if yes return non zero number and print error

//   //--are the pointers in the free list point to valid free blocks?--
//   // QUESTION: isnt this the same as maked as free?
//   // not necessarily needed implement if things going crazy

//   //- Any others?
//   // think of it and ask discord
//   return 0;
// }

// // TODO: do i actually need this?
// static inline void *get_pointer(void *b) { return (struct block *)((void *)b); }