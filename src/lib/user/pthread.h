#ifndef __LIB_USER_PTHREAD_H
#define __LIB_USER_PTHREAD_H

typedef struct pthread_t;
typedef int pthread_mutex_t;
typedef int pthread_semaphore_t;

/* User thread creation/join. */
tid pthread_create(pthread_t * t, void *(*start_routine)(void *), void *args);
bool pthread_join(pthread_t * t);

/* User locks. */
bool pthread_mutex_init(pthread_mutex_t *);
void pthread_mutex_lock(pthread_mutex_t *);
void pthread_mutex_unlock(pthread_mutex_t *);

/* User semaphores. */
bool pthread_semaphore_init(pthread_semaphore_t *);
void pthread_semaphore_down(pthread_semaphore_t *);
void pthread_semaphore_up(pthread_semaphore_t *);

#endif