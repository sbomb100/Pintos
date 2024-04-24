#define _GNU_SOURCE
#include "lib/user/pthread.h"
// #include <stdatomic.h>
#include "lib/stdatomic.h"
// #include <stdlib.h>
#include "lib/stdlib.h"
// #include <stdint.h>
#include "lib/stdint.h"
// #include <stdbool.h>
#include "lib/stdbool.h"
// #include "<stdio.h>"
#include "lib/stdio.h"
// #include <signal.h>
#include "lib/user/malloc.h"

#include "threadpool.h"

#define DEQUE_SIZE 64

void srand(unsigned int seed);
int rand(void);

static uint32_t next = 1;

// Seed the random number generator
void srand(unsigned int seed) {
    next = seed;
}

// Generate a random number
int rand(void) {
    next = next * 1103515245 + 12345;
    return (unsigned int)(next/65536) % 32768;
}

enum future_status {
    NOT_STARTED,
    IN_PROGRESS,
    COMPLETED,
};

struct list_elem 
  {
    struct list_elem *prev;     /* Previous list element. */
    struct list_elem *next;     /* Next list element. */
  };

/* List. */
struct list 
  {
    struct list_elem head;      /* List head. */
    struct list_elem tail;      /* List tail. */
  };

struct thread_pool {
    struct worker * workers;
    int nthreads;
    char pad[48];
};

struct worker {	
    tid_t t;
    atomic_bool shutting_down;
    struct thread_pool * pool;	
    struct deque * local_deque;
    struct list * future_recycler;
    char pad[24];
};

struct future {
    _Atomic(struct worker *) w_help;
    _Atomic(enum future_status) status;
    struct list_elem elem;
    void* args;
    void* result;
    fork_join_task_t task;
    struct thread_pool * pool;
};

struct array {
    atomic_size_t size;
    _Atomic(struct future *) * buffer;
};

struct deque {
    atomic_size_t top, bottom;
    _Atomic(struct array *) array;
};

#define list_entry(LIST_ELEM, STRUCT, MEMBER)           \
        ((STRUCT *) ((uint8_t *) &(LIST_ELEM)->next     \
                     - offsetof (STRUCT, MEMBER.next)))

/*thread-local worker struct. Will be NULL for external threads.*/
static _Thread_local struct worker * current_worker; 

static void
list_init (struct list *list)
{
  list->head.prev = NULL;
  list->head.next = &list->tail;
  list->tail.prev = &list->head;
  list->tail.next = NULL;
}

static struct list_elem *
list_begin (struct list *list)
{
  return list->head.next;
}

static inline struct list_elem *
list_end (struct list *list)
{
  return &list->tail;
}

static inline bool
list_empty (struct list *list)
{
  return list_begin (list) == list_end (list);
}

static inline void
list_insert (struct list_elem *before, struct list_elem *elem)
{
  elem->prev = before->prev;
  elem->next = before;
  before->prev->next = elem;
  before->prev = elem;
}

static inline void
list_push_front (struct list *list, struct list_elem *elem)
{
  list_insert (list_begin (list), elem);
}

static inline struct list_elem *
list_remove (struct list_elem *elem)
{
  elem->prev->next = elem->next;
  elem->next->prev = elem->prev;
  return elem->next;
}

static inline struct list_elem *
list_front (struct list *list)
{
  return list->head.next;
}

static inline struct list_elem *
list_pop_front (struct list *list)
{
  struct list_elem *front = list_front (list);
  list_remove (front);
  return front;
}

static inline struct deque * initialize() {
    struct deque * q = malloc(sizeof(struct deque));
    struct array * a = malloc(sizeof(struct array));

    a->buffer = malloc(DEQUE_SIZE * sizeof(struct future *));
    a->size = DEQUE_SIZE;

    q->top = 0;
    q->bottom = 0;
    q->array = a;
    
    return q;
}

static inline void destroy(struct deque * q) {
    struct array * a = q->array;
    
    free(a->buffer);
    free(a);
    free(q);
}

static inline struct future * take(struct deque * q) {
    size_t b = atomic_load_explicit(&q->bottom, memory_order_relaxed) - 1;
    struct array * a = atomic_load_explicit(&q->array, memory_order_relaxed);
    atomic_store_explicit(&q->bottom, b, memory_order_relaxed);
    atomic_thread_fence(memory_order_seq_cst);

    size_t t = atomic_load_explicit(&q->top, memory_order_relaxed);

    struct future * f = NULL;
    if ( t <= b ) {
        f = atomic_load_explicit(&a->buffer[b % a->size], memory_order_relaxed);
        if ( t == b ) {
            if ( !atomic_compare_exchange_strong_explicit(&q->top, &t, t + 1, memory_order_seq_cst, memory_order_relaxed)) {
                atomic_store_explicit(&q->bottom, b + 1, memory_order_relaxed);
                return NULL;
            }
            atomic_store_explicit(&q->bottom, b + 1, memory_order_relaxed);
        }
    } else {
        atomic_store_explicit(&q->bottom, b + 1, memory_order_relaxed);
        return NULL;
    }

    return f;
}

static inline void resize(struct deque * q) {
    struct array * a = atomic_load(&q->array);
    size_t t = atomic_load_explicit(&q->top, memory_order_relaxed);
    size_t b = atomic_load_explicit(&q->bottom, memory_order_relaxed);

    size_t size = a->size;
    struct array * newA = malloc(sizeof(struct array));
    newA->buffer = malloc(2 * size * sizeof(struct future *));
    newA->size = 2 * size;

    for ( size_t i = t; i < b; i++ ) {
        atomic_store_explicit(&newA->buffer[i % (size * 2)], atomic_load_explicit(&a->buffer[i % (size)], memory_order_relaxed), memory_order_relaxed);
    }

    atomic_store(&q->array, newA);
    free(a->buffer);
    free(a);
}

static inline void push(struct deque * q, struct future * f) {
    size_t b = atomic_load_explicit(&q->bottom, memory_order_relaxed);
    size_t t = atomic_load_explicit(&q->top, memory_order_acquire);
    struct array * a = atomic_load_explicit(&q->array, memory_order_relaxed);

    if (b - t > a->size - 1) {
        resize(q);
        a = atomic_load_explicit(&q->array, memory_order_relaxed);
    }

    atomic_store_explicit(&a->buffer[b % a->size], f, memory_order_relaxed);
    atomic_thread_fence(memory_order_release);
    atomic_store_explicit(&q->bottom, b + 1, memory_order_relaxed);
}

static inline struct future * steal(struct deque * q) {
    size_t t = atomic_load_explicit(&q->top, memory_order_acquire);
    atomic_thread_fence(memory_order_seq_cst);
    size_t b = atomic_load_explicit(&q->bottom, memory_order_acquire);
    struct future * f = NULL;
    if ( t < b ) {
        struct array * a = atomic_load_explicit(&q->array, memory_order_consume);
        f = atomic_load_explicit(&a->buffer[t % a->size], memory_order_relaxed);

        if ( !atomic_compare_exchange_strong_explicit(&q->top, &t, t + 1, memory_order_seq_cst, memory_order_relaxed) ) {
            return NULL;
        }
    }

    return f;
}

static inline void complete_task(struct future * fut) {
    atomic_store_explicit(&fut->w_help, current_worker, memory_order_relaxed);
    atomic_store_explicit(&fut->status, IN_PROGRESS, memory_order_relaxed);
    fut->result = fut->task(fut->pool, fut->args);
    atomic_store_explicit(&fut->status, COMPLETED, memory_order_relaxed);
}

/* Starting routine for worker created by p_thread. */
static inline void * worker_function(void * arg) {
    current_worker = arg;

    uint8_t n = current_worker->pool->nthreads - 1;

    struct future * fut;
    // struct timespec ts = {
    //     .tv_sec = 0,
    //     .tv_nsec = 1.
    // };

    while ( !atomic_load_explicit(&current_worker->shutting_down, memory_order_relaxed) ) {
        if ( (fut = steal(current_worker->pool->workers[rand() & n].local_deque)) != NULL ) {
            complete_task(fut);
        }
        else {
            // nanosleep(&ts, NULL);
        }
    }

    return NULL;
}

/* Create a new thread pool with no more than n threads. */
struct thread_pool * thread_pool_new(int nthreads) {
    struct thread_pool * thread_pool = malloc(sizeof(struct thread_pool));
    thread_pool->workers = malloc(nthreads * sizeof(struct worker));
    size_t rc, i;

    thread_pool->nthreads = nthreads;  

    // struct pthread_t t[nthreads];
    // struct pthread_t t[nthreads];
    // pthread_attr_t attr[nthreads];

    // cpu_set_t mask;
    // for ( i = 0; i < nthreads; i++ ) {
    //     CPU_ZERO(&mask);
    //     CPU_SET(i, &mask);
    //     CPU_SET(i + 32, &mask);

    //     pthread_attr_init(&attr[i]);
    //     rc = pthread_attr_setaffinity_np(&attr[i], sizeof(mask), &mask);
    //     if ( rc != 0 ) {
    //         perror("pthread_setaffinity_np");
    //     }

    //     rc = pthread_attr_setstacksize(&attr[i], 8388608);
    //     if ( rc != 0 ) {
    //         perror("pthread_attr_setstacksize");
    //     }
    // }

    // srand(time(NULL));

    for ( i = 0; i < (size_t)nthreads; i++ ) {
        thread_pool->workers[i].pool = thread_pool;	
        thread_pool->workers[i].local_deque = initialize();	

        struct list *new_recycler = malloc(sizeof(struct list));
        list_init(new_recycler);
        thread_pool->workers[i].future_recycler = new_recycler;

        thread_pool->workers[i].t = i;	
        thread_pool->workers[i].shutting_down = false;
    }	   

    for ( i = 0; i < (size_t)nthreads; i++ ) {
        // rc = pthread_create(&thread_pool->workers[i].t, &attr[i], worker_function, &thread_pool->workers[i]);
        rc = pthread_create((start_routine)worker_function, &thread_pool->workers[i]);
        if ( rc != 0 ) {
            // perror("pthread_create");
        }

        // rc = pthread_attr_destroy(&attr[i]);
        if ( rc != 0 ) {
            // perror("pthread_attr_destroy");
        }
    }

    return thread_pool;
}

/* 
 * Shutdown this thread pool in an orderly fashion.  
 * Tasks that have been submitted but not executed may or
 * may not be executed.
 *
 * Deallocate the thread pool object before returning. 
 */
void thread_pool_shutdown_and_destroy(struct thread_pool * pool) {
    int i;
    for ( i = 0; i < pool->nthreads; i++ ) {
        pool->workers[i].shutting_down = true;
    }
    
    //wait for each worker to exit.
    for ( i = 0; i < pool->nthreads; i++ ) {
        if ( pthread_join(pool->workers[i].t) != 0 ) {
            // perror("pthread_join");
        }
    }

    for ( int i = 0; i < pool->nthreads; i++ ) {
        for ( struct list_elem * e = list_begin(pool->workers[i].future_recycler); e != list_end(pool->workers[i].future_recycler); ) {
        struct future * fut = list_entry(e, struct future, elem);
        e = list_remove(e);

            free(fut);
        }
        free(pool->workers[i].future_recycler);
        destroy(pool->workers[i].local_deque);
    }

    free(pool->workers);
    free(pool);
}

/* 
 * Submit a fork join task to the thread pool and return a
 * future.  The returned future can be used in future_get()
 * to obtain the result.
 * 'pool' - the pool to which to submit
 * 'task' - the task to be submitted.
 * 'data' - data to be passed to the task's function
 *
 * Returns a future representing this computation.
 */
struct future * thread_pool_submit(struct thread_pool *pool, fork_join_task_t task, void * data) {
    struct future * future;
    if ( current_worker != NULL ) {
        if ( list_empty(current_worker->future_recycler) ) {
            future = malloc(sizeof(struct future));
        }
        else {
            struct list_elem * e = list_pop_front(current_worker->future_recycler);
            future = list_entry(e, struct future, elem);
        }

        future->task = task;
        future->args = data;
        atomic_store_explicit(&future->status, NOT_STARTED, memory_order_relaxed);
        future->pool = pool;

        push(current_worker->local_deque, future);
    }
    else {
        future = malloc(sizeof(struct future));

        future->task = task;
        future->args = data;
        atomic_store_explicit(&future->status, NOT_STARTED, memory_order_relaxed);
        future->pool = pool;

        push(pool->workers[0].local_deque, future);
    }
    
    return future; 
}

/* Make sure that the thread pool has completed the execution
 * of the fork join task this future represents.
 *
 * Returns the value returned by this task.
 */
inline void * future_get(struct future * fut) {
    if ( current_worker == NULL ) {
        while ( atomic_load_explicit(&fut->status, memory_order_relaxed) != COMPLETED );
    }
    else {
        struct future * f;
        while ( atomic_load_explicit(&fut->status, memory_order_relaxed) == NOT_STARTED ) {
            if ( (f = take(current_worker->local_deque)) != NULL ) {
                complete_task(f);
            }
        }
        
        struct worker * w;
        while ( atomic_load_explicit(&fut->status, memory_order_relaxed) != COMPLETED ) {
            w = atomic_load_explicit(&fut->w_help, memory_order_relaxed);
            if ( w != NULL ) {
                while ( (f = steal(w->local_deque)) != NULL ) {
                    complete_task(f);
                    if ( atomic_load_explicit(&fut->status, memory_order_relaxed) == COMPLETED ) {
                        break;
                    }
                }
            }
        }
    }
    return fut->result;
}

/* Deallocate this future.  Must be called after future_get() */
inline void future_free(struct future * fut) {
    if ( current_worker != NULL ) {
        list_push_front(current_worker->future_recycler, &fut->elem);
    }
    else {
        free(fut);
    }
}
