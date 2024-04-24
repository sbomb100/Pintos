#define _GNU_SOURCE
#include "lib/user/pthread.h"
#include "lib/stdlib.h"
#include "lib/stdint.h"
#include "lib/stdbool.h"
#include "lib/user/malloc.h"

#include "threadpool.h"

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

struct thread_pool {
    int nthreads;
    struct list * global_tasks;
    pthread_cond_t cond;
    pthread_mutex_t global_mutex;
    struct worker * workers;
};

#define list_entry(LIST_ELEM, STRUCT, MEMBER)           \
        ((STRUCT *) ((uint8_t *) &(LIST_ELEM)->next     \
                     - offsetof (STRUCT, MEMBER.next)))

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

static inline void complete_task(struct future * fut) {
}

/* Starting routine for worker created by p_thread. */
static void * worker_function(void * arg) {
}

/* Create a new thread pool with no more than n threads. */
struct thread_pool * thread_pool_new(int nthreads) {
}

/* 
 * Shutdown this thread pool in an orderly fashion.  
 * Tasks that have been submitted but not executed may or
 * may not be executed.
 *
 * Deallocate the thread pool object before returning. 
 */
void thread_pool_shutdown_and_destroy(struct thread_pool * pool) {
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
}

/* Make sure that the thread pool has completed the execution
 * of the fork join task this future represents.
 *
 * Returns the value returned by this task.
 */
inline void * future_get(struct future * fut) {
}

/* Deallocate this future.  Must be called after future_get() */
void future_free(struct future * fut) {
}
