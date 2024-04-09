#ifndef __LIB_USER_PTHREAD_H
#define __LIB_USER_PTHREAD_H

#include <stdbool.h>

struct pthread_t;
struct pthread_mutex_t;
struct pthread_semaphore_t;

/* User thread creation/join. */
bool pthread_create(struct pthread_t * t, void *(*start_routine)(void *), void *args);
bool pthread_join(struct pthread_t * t);

/* User locks. */
bool pthread_mutex_init(struct pthread_mutex_t *);
void pthread_mutex_lock(struct pthread_mutex_t *);
void pthread_mutex_unlock(struct pthread_mutex_t *);

/* User semaphores. */
bool pthread_semaphore_init(struct pthread_semaphore_t *);
void pthread_semaphore_down(struct pthread_semaphore_t *);
void pthread_semaphore_up(struct pthread_semaphore_t *);

#endif