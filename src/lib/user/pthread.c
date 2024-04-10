#include <pthread.h>
#include <syscall.h>

struct pthread_t_anon {
    tid_t tid;
};

struct pthread_mutex_t_anon {
    int id;
};

struct pthread_semaphore_t_anon {
    int id;
};

bool pthread_create(pthread_t * t, void *(*start_routine)(void *), void *args) {
    tid_t tid;
    if ( (tid = sys_pthread_create(start_routine, args)) != TID_ERROR ) {
        t->tid = tid;
        return true;
    }
    return false;
}

bool pthread_join(pthread_t * t) {
    return sys_pthread_join(t->tid) == t->tid;
}

bool pthread_mutex_init(pthread_mutex_t * mutex) {
    return false;
}

void pthread_mutex_lock(pthread_mutex_t * mutex) {
    return;
}

void pthread_mutex_unlock(pthread_mutex_t * mutex) {
    return;
}

bool pthread_semaphore_init(pthread_semaphore_t * sema) {
    return false;
}

void pthread_semaphore_down(pthread_semaphore_t * sema) {
    return;
}

void pthread_semaphore_up(pthread_semaphore_t * sema) {
    return;
}
