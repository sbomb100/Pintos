#ifndef __LIB_USER_PTHREAD_H
#define __LIB_USER_PTHREAD_H

#include <syscall.h>
#include <stddef.h>

/* User thread creation/join. */
tid_t pthread_create(start_routine task, void * args);
bool pthread_join(tid_t tid);
void pthread_exit(void) NO_RETURN;

/* Thread-local storage. */
void pthread_tls_store(void * storage, size_t size);
void * pthread_tls_load(void);

/* User locks. */
pthread_lock_t pthread_mutex_init(void);
void pthread_mutex_lock(pthread_lock_t mutex);
void pthread_mutex_unlock(pthread_lock_t mutex);

/* User semaphores. */
pthread_sema_t pthread_semaphore_init(int value);
void pthread_semaphore_down(pthread_sema_t sema);
void pthread_semaphore_up(pthread_sema_t sema);

/* User condition variables. */
pthread_cond_t pthread_cond_init(void);
void pthread_cond_wait(pthread_cond_t cond UNUSED, pthread_lock_t UNUSED);
void pthread_cond_signal(pthread_cond_t cond UNUSED);

#endif