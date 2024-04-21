#include <pthread.h>
#include <stdio.h>
#include <string.h>

#define BITMASK(SHIFT, CNT) (((1ul << (CNT)) - 1) << (SHIFT))

/* Page offset (bits 0:12). */
#define PGSHIFT 0                          /* Index of first offset bit. */
#define PGBITS  12                         /* Number of offset bits. */
#define PGSIZE  (1 << PGBITS)              /* Bytes in a page. */
#define PGMASK  BITMASK(PGSHIFT, PGBITS)   /* Page offset bits (0:12). */

/* Round up to nearest page boundary. */
static inline void *pg_round_up (const void *va) {
  return (void *) (((uintptr_t) va + PGSIZE - 1) & ~PGMASK);
}

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

void pthread_tls_store(void * storage, size_t size) {
    void * esp;
    asm("mov %%esp, %0" : "=g"(esp));

    void * stack = pg_round_up(esp);
    memcpy(stack, storage, size);
}

void * pthread_tls_load() {
    void * esp;
    asm("mov %%esp, %0" : "=g"(esp));

    return pg_round_up(esp);
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
