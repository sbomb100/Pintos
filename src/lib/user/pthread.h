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
bool is_main_thread(void);

/* User locks. */
void pthread_mutex_init(pthread_lock_t * mutex);
void pthread_mutex_lock(pthread_lock_t * mutex);
void pthread_mutex_unlock(pthread_lock_t * mutex);

/* User semaphores. */
void pthread_semaphore_init(pthread_sema_t * sema, int value);
void pthread_semaphore_post(pthread_sema_t * sema);
void pthread_semaphore_wait(pthread_sema_t * sema);

/* User condition variables. */
void pthread_cond_init(pthread_cond_t * cond);
void pthread_cond_wait(pthread_cond_t * cond, pthread_lock_t * mutex);
void pthread_cond_signal(pthread_cond_t * cond);
void pthread_cond_broadcast(pthread_cond_t * cond);

#endif