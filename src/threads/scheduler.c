#include "threads/scheduler.h"
#include "threads/cpu.h"
#include "threads/interrupt.h"
#include "list.h"
#include "threads/spinlock.h"
#include "devices/timer.h"
#include <debug.h>
#include <stdbool.h>

/* Scheduling. */
#define TIME_SLICE 4 /* # of timer ticks to give each thread. */

static const int64_t prio_to_weight[40] = {
    /* -20 */ 88761,
    71755,
    56483,
    46273,
    36291,
    /* -15 */ 29154,
    23254,
    18705,
    14949,
    11916,
    /* -10 */ 9548,
    7620,
    6100,
    4904,
    3906,
    /*  -5 */ 3121,
    2501,
    1991,
    1586,
    1277,
    /*   0 */ 1024,
    820,
    655,
    526,
    423,
    /*   5 */ 335,
    272,
    215,
    172,
    137,
    /*  10 */ 110,
    87,
    70,
    56,
    45,
    /*  15 */ 36,
    29,
    23,
    18,
    15,
};

/*
    Find the min vruntime of running/ready threads.
*/
void min_vruntime(struct list *ready_list, struct thread *curr)
{
  int64_t prev_min = get_cpu()->rq.min_vruntime;
  if (list_empty(ready_list))
  {
    if (curr == NULL)
    {
      get_cpu()->rq.min_vruntime = 0;
      return;
    }
    get_cpu()->rq.min_vruntime = curr->vruntime;
    return;
  }
  struct thread *min = list_entry(list_front(ready_list), struct thread, elem);

  if (curr == NULL)
  {
    get_cpu()->rq.min_vruntime = min->vruntime;
    return;
  }
  get_cpu()->rq.min_vruntime = min->vruntime < curr->vruntime ? min->vruntime : curr->vruntime;

  /* Checks if the newly calculated min_vruntime is smaller than the previous.
     Forces min_vruntime to never decrease */
  if (get_cpu()->rq.min_vruntime < prev_min)
  {
    get_cpu()->rq.min_vruntime = prev_min;
  }
}
/*
    Updates the running thread's vruntime.
*/
void update_vruntime(struct ready_queue *rq)
{
  if (rq->curr == NULL)
    return;

  int64_t d = timer_gettime() - rq->curr->vruntime_0;
  int64_t w_0 = prio_to_weight[20];
  int64_t w = prio_to_weight[rq->curr->nice + 20];
  rq->curr->vruntime += d * w_0 / w;
  rq->curr->actual_runtime += d;
  rq->curr->vruntime_0 = timer_gettime();
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

bool vruntime_cmp(const struct list_elem *a, const struct list_elem *b, void *aux UNUSED)
{
  struct thread *aT = list_entry(a, struct thread, elem);
  struct thread *bT = list_entry(b, struct thread, elem);

  if (aT->vruntime < bT->vruntime)
  {
    return true;
  }

  if (aT->vruntime == bT->vruntime && aT->tid < bT->tid)
  {
    return true;
  }
  return false;
}

int64_t max(int64_t x, int64_t y)
{
  return x > y ? x : y;
}

/* Called from thread_init () and thread_init_on_ap ().
   Initializes data structures used by the scheduler.

   This function is called very early.  It is unsafe to call
   thread_current () at this point.
 */
void sched_init(struct ready_queue *curr_rq)
{
  list_init(&curr_rq->ready_list);
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
sched_unblock(struct ready_queue *rq_to_add, struct thread *t, int initial)
{
  update_vruntime(rq_to_add);
  if (initial == 1) // unblock new thread
  {
    min_vruntime(&rq_to_add->ready_list, rq_to_add->curr);
    t->vruntime = get_cpu()->rq.min_vruntime;
    t->actual_runtime = get_cpu()->rq.min_vruntime;
  }
  else // unblock existing thread
  {
    min_vruntime(&rq_to_add->ready_list, rq_to_add->curr);
    t->vruntime = max(t->vruntime, get_cpu()->rq.min_vruntime - 20000000);
    t->actual_runtime = max(t->vruntime, get_cpu()->rq.min_vruntime - 20000000);
  }
  list_insert_ordered(&rq_to_add->ready_list, &t->elem, vruntime_cmp, NULL);
  rq_to_add->nr_ready++;

  /* CPU is idle */
  if (!rq_to_add->curr || t->vruntime < rq_to_add->curr->vruntime)
  {
    return RETURN_YIELD;
  }
  return RETURN_NONE;
}

/* Called from thread_yield ().
   Current thread is about to yield.  Add it to the ready list

   Current ready queue is locked upon entry.
 */
void sched_yield(struct ready_queue *curr_rq, struct thread *current)
{
  update_vruntime(curr_rq);
  list_insert_ordered(&curr_rq->ready_list, &current->elem, vruntime_cmp, NULL);
  curr_rq->nr_ready++;
}

/* Called from next_thread_to_run ().
   Find the next thread to run and remove it from the ready list
   Return NULL if the ready list is empty.

   If the thread returned is different from the thread currently
   running, a context switch will take place.

   Called with current ready queue locked.
 */
struct thread *
sched_pick_next(struct ready_queue *curr_rq)
{
  if (list_empty(&curr_rq->ready_list))
    return NULL;

  struct thread *ret = list_entry(list_pop_front(&curr_rq->ready_list), struct thread, elem);
  curr_rq->nr_ready--;
  ret->vruntime_0 = timer_gettime();
  ret->actual_runtime = 0;
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
sched_tick(struct ready_queue *curr_rq, struct thread *current)
{
  /* Enforce preemption. */
  unsigned long n = current == NULL ? curr_rq->nr_ready : curr_rq->nr_ready + 1;
  int64_t w = prio_to_weight[current->nice + 20];
  int64_t s = w;

  for (struct list_elem *e = list_begin(&curr_rq->ready_list); e != list_end(&curr_rq->ready_list); e = list_next(e))
  {
    struct thread *t = list_entry(e, struct thread, elem);
    s += prio_to_weight[t->nice + 20];
  }

  int64_t ideal_runtime = 4000000 * n * w / s;
  update_vruntime(curr_rq);

  if (current->actual_runtime >= ideal_runtime)
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
void sched_block(struct ready_queue *rq, struct thread *current UNUSED)
{
  update_vruntime(rq);
}
/*
  Add a thread back to queue
*/
/**
 * function for CPU ready queue balancing, called from idle() in threads.c
 */
void sched_load_balance()
{
  
  int mostJobs = -1; /*index num of cpu with highest weight*/
  int64_t weightHighScore = 0; 
  int myWeight = 0;

  for (unsigned int i = 0; i < ncpu; i++) // go through all cpus to find which has highest weight /cpu_load
  {
    spinlock_acquire(&cpus[i].rq.lock);
    
    int64_t totalWeight = 0;
    struct list_elem* e = list_head (&cpus[i].rq.ready_list);
    while ((e = list_next (e)) != list_end (&cpus[i].rq.ready_list)) 
    {
      struct thread *t = list_entry(e, struct thread, elem);
      totalWeight = totalWeight + prio_to_weight[t->nice];
    }

    if (totalWeight > weightHighScore && (&cpus[i] != get_cpu()))
    {
      weightHighScore = totalWeight;
      mostJobs = i;
    }
    else if (&cpus[i] == get_cpu()){ // if this total weight is of the curr cpu
      myWeight = totalWeight;
    }
    
    spinlock_release(&cpus[i].rq.lock);
  }

  // If imbalance is small (imbalance * 4 < busiest_cpu_load) bail
  int64_t imbalance = (weightHighScore - myWeight) / 2;
  if (4 * imbalance < weightHighScore || mostJobs == -1)
  {
    return;
  }
  ASSERT(get_cpu() != &cpus[mostJobs]);

  spinlock_acquire(&cpus[mostJobs].rq.lock);

  spinlock_acquire(&get_cpu()->rq.lock);
  struct cpu* thisCpu = get_cpu();
  while (4 * imbalance >= weightHighScore)
  {
    // take job from busiest cpu
    if (cpus[mostJobs].rq.nr_ready == 0 || list_empty(&cpus[mostJobs].rq.ready_list) ) // fail saf
    {
      break;
    }
    // take off first priority of busiest thread
    struct thread *stolen = list_entry(list_pop_front(&cpus[mostJobs].rq.ready_list), struct thread, elem);
    weightHighScore = weightHighScore - (prio_to_weight[stolen->nice]); //update weight highscore
    min_vruntime(&cpus[mostJobs].rq.ready_list, NULL);
    int64_t busyminv = cpus[mostJobs].rq.min_vruntime;
    cpus[mostJobs].rq.nr_ready--;

    // put stolen job in our queue
    struct ready_queue *readyq = &thisCpu->rq;
    min_vruntime(&readyq->ready_list, NULL);
    int64_t myminv = readyq->min_vruntime;
    stolen->vruntime_0 = stolen->vruntime - busyminv + myminv;
    stolen->cpu = thisCpu;
    list_insert_ordered(&readyq->ready_list, &stolen->elem, vruntime_cmp, NULL);
    myWeight = myWeight + (prio_to_weight[stolen->nice]);
    thisCpu->rq.nr_ready++;

    imbalance = (weightHighScore - myWeight) / 2;
  }

  spinlock_release(&get_cpu()->rq.lock);
  spinlock_release(&cpus[mostJobs].rq.lock);
}