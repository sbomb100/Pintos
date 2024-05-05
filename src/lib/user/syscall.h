#ifndef __LIB_USER_SYSCALL_H
#define __LIB_USER_SYSCALL_H

#include <stdint.h>
#include <stdbool.h>
#include <debug.h>

/* Process identifier. */
typedef int pid_t;
#define PID_ERROR ((pid_t) -1)

/* Map region identifier. */
typedef int mapid_t;
#define MAP_FAILED ((mapid_t) -1)

/* User thread identifier. */
typedef int tid_t;
#define TID_ERROR ((tid_t) -1)

/* User synchronization identifiers. */
typedef struct pthread_lock_s {
    int val;
} pthread_lock_t;

typedef struct pthread_sema_s {
    pthread_lock_t lock;
    int count;
} pthread_sema_t;

typedef struct pthread_cond_s {
    int total_seq;
    int wakeup_seq;
    int woken_seq;
    int broadcast_seq;
} pthread_cond_t;

/* Function pointer definitions. */
typedef void (*start_routine)(void *);
typedef void (*wrapper_func)(start_routine, void *);

/* Maximum characters in a filename written by readdir(). */
#define READDIR_MAX_LEN 14

/* Typical return values from main() and arguments to exit(). */
#define EXIT_SUCCESS 0          /* Successful execution. */
#define EXIT_FAILURE 1          /* Unsuccessful execution. */

/* Projects 2 and later. */
void halt (void) NO_RETURN;
void exit (int status) NO_RETURN;
pid_t exec (const char *file);
int wait (pid_t);
bool create (const char *file, unsigned initial_size);
bool remove (const char *file);
int open (const char *file);
int filesize (int fd);
int read (int fd, void *buffer, unsigned length);
int write (int fd, const void *buffer, unsigned length);
void seek (int fd, unsigned position);
unsigned tell (int fd);
void close (int fd);

/* Project 3 and optionally project 4. */
mapid_t mmap (int fd, void *addr);
void munmap (mapid_t);

/* Project 4 only. */
bool chdir (const char *dir);
bool mkdir (const char *dir);
bool readdir (int fd, char name[READDIR_MAX_LEN + 1]);
bool isdir (int fd);
int inumber (int fd);

/* User threads syscalls. */
tid_t sys_pthread_create(wrapper_func wf, start_routine sr, void *args);
void sys_pthread_exit(void) NO_RETURN;
bool sys_pthread_join(tid_t tid);

/* User malloc syscalls. */
void * sbrk(intptr_t increment);

/* User synch syscalls. */
void futex_wait(void * addr);
void futex_wake(void * addr, int val);

int64_t sys_timer_ticks(void);
int64_t sys_timer_elapsed(int64_t ticks);
#endif /* lib/user/syscall.h */
