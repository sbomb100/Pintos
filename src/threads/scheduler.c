#include "threads/scheduler.h"
#include "threads/cpu.h"
#include "threads/interrupt.h"
#include "list.h"
#include "threads/spinlock.h"
#include <debug.h>
#include <stdbool.h>

/* Scheduling. */
#define TIME_SLICE 4            /* # of timer ticks to give each thread. */

static const uint32_t prio_to_weight[40] = {
    /* -20 */    88761, 71755, 56483, 46273, 36291,
    /* -15 */    29154, 23254, 18705, 14949, 11916,
    /* -10 */    9548, 7620, 6100, 4904, 3906,
    /*  -5 */    3121, 2501, 1991, 1586, 1277,
    /*   0 */    1024, 820, 655, 526, 423,
    /*   5 */    335, 272, 215, 172, 137,
    /*  10 */    110, 87, 70, 56, 45,
    /*  15 */    36, 29, 23, 18, 15,
  };

/*
    Find the min vruntime of running/ready threads.
*/
int64_t min_vruntime(struct list * ready_list, struct thread * curr) {
    if ( list_empty(ready_list) ) {
        if ( curr == NULL ) {
            return 0;
        }
        return curr->vruntime;
    }
    struct thread * min = list_entry(list_front(ready_list), struct thread, elem);
    return min->vruntime < curr->vruntime ? min->vruntime : curr->vruntime;
}

/*
    Updates the running thread's vruntime.
*/
void update_vruntime(struct ready_queue * rq) {
    if( rq->curr == NULL ) return;

    int64_t d = timer_gettime() - rq->curr->wake_tick;
    uint32_t w_0 = prio_to_weight[20];
    uint32_t w = prio_to_weight[rq->curr->nice + 20];
    rq->curr->vruntime += d * (w_0 / w);
}

/*
 * In the provided baseline implementation, threads are kept in an unsorted list.
 *
 * Threads are added to the back of the list upon creation.
 * The thread at the front is picked for scheduling.
 * Upon preemption, the current thread is added to the end of the queue
 * (in sched_yield), creating a round-robin policy if multiple threads
 * are in the ready queue.
 * Preemption occurs every TIME_SLICE ticks.
 */

bool vruntime_cmp(const struct list_elem *a, const struct list_elem *b, void * aux UNUSED) {
    struct thread * aT = list_entry(a, struct thread, elem);
    struct thread * bT = list_entry(b, struct thread, elem);

    if ( aT->vruntime < bT->vruntime ) {
        return true;
    }

    if ( aT->vruntime == bT->vruntime && aT->tid < bT->tid ) {
        return true;
    }
    return false;
}

int64_t max(int64_t x, int64_t y) {
    return x > y ? x : y;
}

/* Called from thread_init () and thread_init_on_ap ().
   Initializes data structures used by the scheduler. 
 
   This function is called very early.  It is unsafe to call
   thread_current () at this point.
 */
void
sched_init (struct ready_queue *curr_rq)
{
  list_init (&curr_rq->ready_list);
}

/* Called from thread.c:wake_up_new_thread () and
   thread_unblock () with the CPU's ready queue locked.
   rq is the ready queue that t should be added to when
   it is awoken. It is not necessarily the current CPU.

   If called from wake_up_new_thread (), initial will be 1.
   If called from thread_unblock (), initial will be 0.

   Returns RETURN_YIELD if the CPU containing rq should
   be rescheduled when this function returns, else returns
   RETURN_NONE */
enum sched_return_action
sched_unblock (struct ready_queue *rq_to_add, struct thread *t, int initial)
{
  if ( initial == 1 ) {
    t->vruntime = min_vruntime(&rq_to_add->ready_list, rq_to_add->curr);
  }
  else {
    t->vruntime = max(t->vruntime, min_vruntime(&rq_to_add->ready_list, rq_to_add->curr) - 20000000);
  }
  //list_push_back (&rq_to_add->ready_list, &t->elem);
  list_insert_ordered(&rq_to_add->ready_list, &t->elem, vruntime_cmp, NULL);
  rq_to_add->nr_ready++;

  update_vruntime(rq_to_add);
  /* CPU is idle */
  if (!rq_to_add->curr || t->vruntime < rq_to_add->curr->vruntime )
    {
      return RETURN_YIELD;
    }
  return RETURN_NONE;
}

/* Called from thread_yield ().
   Current thread is about to yield.  Add it to the ready list

   Current ready queue is locked upon entry.
 */
void
sched_yield (struct ready_queue *curr_rq, struct thread *current)
{
  //list_push_back (&curr_rq->ready_list, &current->elem);
  update_vruntime(curr_rq);
  list_insert_ordered(&curr_rq->ready_list, &current->elem, vruntime_cmp, NULL);
  curr_rq->nr_ready ++;
}

/* Called from next_thread_to_run ().
   Find the next thread to run and remove it from the ready list
   Return NULL if the ready list is empty.

   If the thread returned is different from the thread currently
   running, a context switch will take place.

   Called with current ready queue locked.
 */
struct thread *
sched_pick_next (struct ready_queue *curr_rq)
{
  if (list_empty (&curr_rq->ready_list))
    return NULL;

  struct thread *ret = list_entry(list_pop_front (&curr_rq->ready_list), struct thread, elem);
  curr_rq->nr_ready--;
  ret->wake_tick = timer_gettime();
  return ret;
}

/* Called from thread_tick ().
 * Ready queue rq is locked upon entry.
 *
 * The currently running thread is preempted when it has run for longer than
 * the "ideal runtime", as shown in B.4
 *
 * Returns RETURN_YIELD if current thread should yield
 * when this function returns, else returns RETURN_NONE.
 */
enum sched_return_action
sched_tick (struct ready_queue *curr_rq, struct thread *current)
{
  /* Enforce preemption. */
  unsigned long n = current == NULL ? curr_rq->nr_ready : curr_rq->nr_ready + 1;
  uint32_t w = prio_to_weight[current->nice + 20];
  uint32_t s = w;

  for ( struct list_elem * e = list_begin(&curr_rq->ready_list); e != list_end(&curr_rq->ready_list); e = list_next(e)) {
    struct thread * t = list_entry(e, struct thread, elem);
    s += prio_to_weight[t->nice + 20];
  } 
  
  int64_t ideal_runtime = 4000000 * n * w / s;
  update_vruntime(curr_rq);
  
  if (current->vruntime >= ideal_runtime)
    {
      /* Start a new time slice. */
      curr_rq->thread_ticks = 0;

      return RETURN_YIELD;
    }
  return RETURN_NONE;
}

/* Called from thread_block (). The base scheduler does
   not need to do anything here, but your scheduler may. 

   'cur' is the current thread, about to block.
 */
void
sched_block (struct ready_queue *rq, struct thread *current UNUSED)
{
  update_vruntime(rq);
}
/*
  Add a thread back to queue
*/
void
sched_thread (struct ready_queue *rq UNUSED, struct thread *current UNUSED)
{
  //if blocked, remove from ready queue?
  ;
}