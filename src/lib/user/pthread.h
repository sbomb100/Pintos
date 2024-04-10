#ifndef __LIB_USER_PTHREAD_H
#define __LIB_USER_PTHREAD_H

#include <syscall.h>

/* User thread creation/join. */
tid_t pthread_create(start_routine task, void * args);
bool pthread_join(tid_t tid);
void pthread_exit(void) NO_RETURN;

/* User locks. */
bool pthread_mutex_init(pthread_lock_t mutex UNUSED);
void pthread_mutex_lock(pthread_lock_t mutex UNUSED);
void pthread_mutex_unlock(pthread_lock_t mutex UNUSED);

/* User semaphores. */
bool pthread_semaphore_init(pthread_sema_t sema UNUSED);
void pthread_semaphore_down(pthread_sema_t sema UNUSED);
void pthread_semaphore_up(pthread_sema_t sema UNUSED);

#endif