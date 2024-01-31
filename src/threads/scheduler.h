#ifndef THREADS_SCHEDULER_H_
#define THREADS_SCHEDULER_H_

#include <stdint.h>
#include "threads/thread.h"
#include "threads/synch.h"

enum sched_return_action {
  RETURN_NONE,
  RETURN_YIELD,
};

/*
 * Data structure for the ready queue, which keeps track of a CPU's
 * READY threads.  Ready queues may use different representations
 * (lists, trees, etc.) depending on which policy they use to make
 * scheduling decisions.
 *
 * There is 1 ready queue per CPU, embedded in struct cpu (cpu.c)
 */
struct ready_queue
{
  /* The following fields are likely needed no matter which policy
   * your ready list implements. */
  struct spinlock lock;       /* Protects all fields in this struct.
                               * Also protects internal fields in struct thread
                               * such as status in all threads pointing
                               * to this ready_queue via their cpu->rq.
                               */
  struct thread *curr;        /* Pointer to current thread on this CPU.
                                 NULL if CPU is idle. */
  struct thread *idle_thread; /* This CPU's idle thread. */

  /* The following fields are specific to a preemptive, round-robin
   * scheduling policy.  You may need to change them in your
   * implementation of project 1. */
  unsigned thread_ticks;      /* Number of ticks since last preemption */
  struct list ready_list;     /* List of ready threads. */
  unsigned long nr_ready;     /* number of elements in ready_list.
                                 Allows O(1) access. */
};

int64_t min_vruntime(struct list *, struct thread *);
void update_vruntime(struct ready_queue *);
bool vruntime_cmp(const struct list_elem *, const struct list_elem *, void *);
int64_t max(int64_t x, int64_t y);
void sched_init (struct ready_queue *);
enum sched_return_action sched_unblock (struct ready_queue *, struct thread *, int );
void sched_yield (struct ready_queue *, struct thread *);
struct thread *sched_pick_next (struct ready_queue *);
enum sched_return_action sched_tick (struct ready_queue *, struct thread *);
void sched_block (struct ready_queue *, struct thread *);

#endif /* THREADS_SCHEDULER_H_ */
