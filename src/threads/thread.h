#ifndef THREADS_THREAD_H
#define THREADS_THREAD_H

#include <debug.h>
#include "lib/kernel/list.h"
#include <stdint.h>
#include "lib/kernel/bitmap.h"
#include "filesys/file.h"
#include "threads/synch.h"
#include "lib/kernel/hash.h"
#include "vm/page.h"

/* States in a thread's life cycle. */
enum thread_status
{
   THREAD_RUNNING, /* Running thread. */
   THREAD_READY,   /* Not running but ready to run. */
   THREAD_BLOCKED, /* Waiting for an event to trigger. */
   THREAD_DYING    /* About to be destroyed. */
};

enum process_status
{
   PROCESS_RUNNING, /* Running process. */
   PROCESS_ORPHAN,  /* Parent process exited without waiting for this. */
   PROCESS_ABORT,   /* Looking to abort from fail load. */
   PROCESS_EXIT,    /* Looking to exit normally. */
};

/* Thread identifier type.
   You can redefine this to whatever type you like. */
typedef int mapid_t;
typedef int tid_t;
typedef int pid_t;

/* Function pointer definitions. */
typedef void (*start_routine)(void *);
typedef void (*wrapper_func)(start_routine, void *);

#define TID_ERROR ((tid_t)-1) /* Error value for tid_t. */
#define PID_ERROR ((pid_t)-1) /* Error value for pid_t. */
#define THREAD_NAME_MAX 16
#define MAX_THREADS 32

/* Thread priorities. */
#define NICE_MIN -20   /* Highest priority. */
#define NICE_DEFAULT 0 /* Default priority. */
#define NICE_MAX 19    /* Lowest priority. */

/* A kernel thread or user process.

   Each thread structure is stored in its own 4 kB page.  The
   thread structure itself sits at the very bottom of the page
   (at offset 0).  The rest of the page is reserved for the
   thread's kernel stack, which grows downward from the top of
   the page (at offset 4 kB).  Here's an illustration:

        4 kB +---------------------------------+
             |          kernel stack           |
             |                |                |
             |                |                |
             |                V                |
             |         grows downward          |
             |                                 |
             |                                 |
             |                                 |
             |                                 |
             |                                 |
             |                                 |
             |                                 |
             |                                 |
             +---------------------------------+
             |              magic              |
             |                :                |
             |                :                |
             |               name              |
             |              status             |
        0 kB +---------------------------------+

   The upshot of this is twofold:

      1. First, `struct thread' must not be allowed to grow too
         big.  If it does, then there will not be enough room for
         the kernel stack.  Our base `struct thread' is only a
         few bytes in size.  It probably should stay well under 1
         kB.

      2. Second, kernel stacks must not be allowed to grow too
         large.  If a stack overflows, it will corrupt the thread
         state.  Thus, kernel functions should not allocate large
         structures or arrays as non-static local variables.  Use
         dynamic allocation with malloc() or palloc_get_page()
         instead.

   The first symptom of either of these problems will probably be
   an assertion failure in thread_current(), which checks that
   the `magic' member of the running thread's `struct thread' is
   set to THREAD_MAGIC.  Stack overflow will normally change this
   value, triggering the assertion. */
/* The `elem' member has a dual purpose.  It can be an element in
   the run queue (thread.c), or it can be an element in a
   semaphore wait list (synch.c).  It can be used these two ways
   only because they are mutually exclusive: only a thread in the
   ready state is on the run queue, whereas only a thread in the
   blocked state is on a semaphore wait list. */

struct thread
{
   /* Owned by thread.c. */
   /* Owned by thread.c. */
   tid_t tid;                  /* Thread identifier. */
   enum thread_status status;  /* Thread state. */
   char name[THREAD_NAME_MAX]; /* Name (for debugging purposes). */
   uint8_t *stack;             /* Saved stack pointer. */

   /* Used in timer.c */
   int64_t wake_tick; /* the tick when the thread should be unblocked */
   struct list_elem blocked_elem;

   int nice;                 /* Nice value. */
   struct list_elem allelem; /* List element for all threads list. */

   struct cpu *cpu; /* Points to the CPU this thread is currently bound to.
                       thread_unblock () will add a thread to the rq of
                       this CPU.  A load balancer needs to update this
                       field when migrating threads.
                     */

   /* Shared between thread.c and synch.c. */
   struct list_elem elem; /* List element. */

   /* Used for CFS algorithm. */
   int64_t vruntime;
   int64_t vruntime_0;
   int64_t actual_runtime;

   /* Owned by userprog/process.c. */
   struct file *exec_file;
   struct process *pcb;

 
   /* Owned by thread.c. */
   unsigned magic; /* Detects stack overflow. */

   size_t bit_index;                /* Index returned from bitmap query. */
   struct semaphore join_sema;      /* Semaphore used to join threads to main. */
   struct semaphore exit_sema;      /* Semaphore used to keep the thread page alive until the main thread joins with this thread. */
};

struct process
{
   pid_t pid;
   int exit_status;
   enum process_status status;
   struct semaphore wait_sema;
   struct list_elem elem;
   struct lock process_lock;
   /*USERPROG STUFF*/
   struct lock fdLock;
   struct file **fdToFile;

   /*VM STUFF*/
   uint32_t * pagedir;
   struct hash spt;
   struct lock spt_lock;
   size_t num_stack_pages;

   struct list mmap_list;
   struct lock mmap_lock;
   size_t num_mapped;

   struct process *parent;
   struct list children;
   struct lock counter_lock;
   size_t num_threads_up;

   struct thread * main_thread;     /* The main (external) thread. Designated in process_create. */
   struct thread ** threads;        /* Array of thread pointers, created by pthread_create. */
   struct bitmap * used_threads;    /* Bitmap of address blocks, to be used by spawning pthreads. */

   void * heap_start;
   void * heap_break;
};

/* VM MMAP */
struct mapped_item
{
   mapid_t id;             /* mmap ID. */
   struct spt_entry *page; /* Page table entry of mmapped file. */
   struct list_elem elem;  /* List_elem for the parent's mmap_list. */
};

void thread_init(void);
void thread_init_on_ap(void);
void thread_start_idle_thread(void);
void thread_tick(void);
void thread_print_stats(void);

typedef void thread_func(void *aux);
tid_t thread_create(const char *name, int priority, thread_func *, void *);
struct thread *make_thread_for_proc(const char *name, int nice, thread_func *function, struct process *parent_proc, void *aux);

void thread_block(struct spinlock *);
void thread_unblock(struct thread *);
struct thread *running_thread(void);
struct thread *thread_current(void);
tid_t thread_tid(void);
const char *thread_name(void);

void thread_exit(int status) NO_RETURN;
void thread_yield(void);
void thread_exit_ap(void) NO_RETURN;
/* Performs some operation on thread t, given auxiliary data AUX. */
typedef void thread_action_func(struct thread *t, void *aux);
void thread_foreach(thread_action_func *, void *);
int thread_get_nice(void);
void thread_set_nice(int);

#endif /* threads/thread.h */
