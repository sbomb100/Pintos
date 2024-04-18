#include <pthread.h>
#include <stdio.h>
/* Opaque wrapper for create. */
void start_thread(start_routine task, void *);

tid_t pthread_create(start_routine task, void * args) {
    return sys_pthread_create(start_thread, task, args);
}

bool pthread_join(tid_t tid) {
    return sys_pthread_join(tid);
}

void pthread_exit() {
    sys_pthread_exit();
    NOT_REACHED();
}

void start_thread(start_routine task, void * args) {
    (*task)(args);
    pthread_exit();
}

bool pthread_mutex_init(pthread_lock_t mutex UNUSED) {
    return false;
}

void pthread_mutex_lock(pthread_lock_t mutex UNUSED) {
    return;
}

void pthread_mutex_unlock(pthread_lock_t mutex UNUSED) {
    return;
}

bool pthread_semaphore_init(pthread_sema_t sema UNUSED) {
    return false;
}

void pthread_semaphore_down(pthread_sema_t sema UNUSED) {
    return;
}

void pthread_semaphore_up(pthread_sema_t sema UNUSED) {
    return;
}
