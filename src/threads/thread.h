#ifndef THREADS_THREAD_H
#define THREADS_THREAD_H

#include <debug.h>
#include <list.h>
#include <stdint.h>
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

enum process_status {
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

#define TID_ERROR ((tid_t)-1) /* Error value for tid_t. */
#define PID_ERROR ((pid_t)-1) /* Error value for pid_t. */
#define THREAD_NAME_MAX 16
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

#ifdef USERPROG
   /* Owned by userprog/process.c. */
   uint32_t *pagedir;           /* Page directory. */

   // struct file **fdToFile;      /* Array of 128 file descriptors. */
   struct list fdToFile;        /* List of file descriptors. */
   struct file *exec_file;      /* Thread's executable, to be given write access at the end of the process. */
   int fd;                      /* File descriptor for the process. */

   struct process *parent;      /* The parent process of the thread. */
   struct list children;        /* Child processes spawned by the parent. */
   struct lock children_lock;   /* Lock for inserting/removing processes from the children list. */
#endif

   /* Virtual Memory */
   struct hash spt;             /* Supplemental Page Table. */
   struct lock spt_lock;        /* Lock for inserting/removing pages from the spt. */
   size_t num_stack_pages;      /* The total number of stack pages in the thread. Starts at 1 but can grow to 2048. */

   struct list mmap_list;       /* List of mmapped files. */
   size_t num_mapped;           /* Number of mmapped files, serves as fd to be handed to user. */

   struct dir *cwd;             /* Current working directory. */

   /* Owned by thread.c. */
   unsigned magic; /* Detects stack overflow. */
};

struct file_descriptor {
    int fd;                     /* File descriptor. */
    bool is_dir;                 /* 1 if the file is a directory, 0 otherwise. */
    struct file *file;          /* File pointer. */
    struct dir *dir;            /* Directory pointer. */
    struct list_elem elem;      /* List_elem for the thread's fdToFile list. */
};

struct process {
    pid_t pid;                      /* Process ID. */
    int exit_status;                /* Exit status of the process. */
    enum process_status status;     /* Process state. */
    struct semaphore wait_sema;     /* Semaphore to signal waiting parent process. */
    struct list_elem elem;          /* List_elem for the parent process's children list. */
    struct lock process_lock;       /* Lock for process state, to be accessed by itself or its parent when orphanized. */
    struct dir *cwd;                /* Current working directory. */
    int fd;                         /* File descriptor for the process. */ 
};

/* VM MMAP */
struct mapped_item
{
   mapid_t id;              /* mmap ID. */
   struct spt_entry* page;  /* Page table entry of mmapped file. */
   struct list_elem elem;   /* List_elem for the parent's mmap_list. */
};

void thread_init(void);
void thread_init_on_ap(void);
void thread_start_idle_thread(void);
void thread_tick(void);
void thread_print_stats(void);

typedef void thread_func(void *aux);
tid_t thread_create(const char *name, int priority, thread_func *, void *);

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
