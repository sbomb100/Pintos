#include <pthread.h>
#include <syscall.h>

struct pthread_t {
    tid_t tid;
};

struct pthread_mutex_t {
    int id;
};

struct pthread_semaphore_t {
    int id;
}

bool pthread_create(struct pthread_t * t, void *(*start_routine)(void *), void *args) {
    tid_t tid;
    if ( (tid = sys_pthread_create(start_routine, args)) != TID_ERROR ) {
        t = {
            .tid = tid,
        };
        return true;
    }
    return false;
}

bool pthread_join(struct pthread_t * t) {
    return sys_pthread_join(t->tid) == t->tid;
}

bool pthread_mutex_init(struct pthread_mutex_t * mutex) {
    return false;
}

void pthread_mutex_lock(struct pthread_mutex_t * mutex) {
    return;
}

void pthread_mutex_unlock(struct pthread_mutex_t * mutex) {
    return;
}

bool pthread_semaphore_init(struct pthread_semaphore_t * sema) {
    return false;
}

void pthread_semaphore_down(struct pthread_semaphore_t * sema) {
    return;
}

void pthread_semaphore_up(struct pthread_semaphore_t * sema) {
    return;
}
