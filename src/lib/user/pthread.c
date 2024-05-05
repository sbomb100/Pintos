#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <atomic-ops.h>
#include <limits.h>

#define BITMASK(SHIFT, CNT) (((1ul << (CNT)) - 1) << (SHIFT))

/* Page offset (bits 0:12). */
#define PGSHIFT 0                          /* Index of first offset bit. */
#define PGBITS  12                         /* Number of offset bits. */
#define PGSIZE  (1 << PGBITS)              /* Bytes in a page. */
#define PGMASK  BITMASK(PGSHIFT, PGBITS)   /* Page offset bits (0:12). */
#define PHYS_BASE 0xc0000000

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

/* Checks if the top of the stack is PHYS_BASE. */
bool is_main_thread() {
    void * esp;
    asm("mov %%esp, %0" : "=g"(esp));
    return pg_round_up(esp) == (void *) PHYS_BASE;
}

/* Futex-driven mutex API inspired by Red Hat:
   https://cis.temple.edu/~giorgio/cis307/readings/futex.pdf

   int address is treated as an enumeration:
   0 = unlocked, uncontested
   1 = locked, uncontested
   2 = locked, contested
*/
void pthread_mutex_init(pthread_lock_t * mutex) {
    mutex->val = 0;
}

void pthread_mutex_lock(pthread_lock_t * mutex) {
    int old = 0;
    int new = 1;
    if ( !atomic_cmpxchg(&mutex->val, &old, &new) ) {
        int c = mutex->val;
        if ( c != 2 ) {
            c = atomic_xchg(&mutex->val, 2);
        }

        while ( c != 0 ) {
            futex_wait(&mutex->val);
            c = atomic_xchg(&mutex->val, 2);
        }
    }
}

void pthread_mutex_unlock(pthread_lock_t * mutex) {
    if ( atomic_deci(&mutex->val) != 1 ) {
        mutex->val = 0;
        futex_wake(&mutex->val, 1);
    }
}

/* Futex-driven semaphore API inspired by glibc: 
   https://sourceware.org/git/?p=glibc.git;a=blob;f=nptl/DESIGN-sem.txt;h=17eb0c11c876dc8677c22ee74f461f82fdded61d;hb=HEAD 
*/
void pthread_semaphore_init(pthread_sema_t * sema, int value) {
    pthread_mutex_init(&sema->lock);
    sema->count = value;
}

void pthread_semaphore_post(pthread_sema_t * sema) {
    pthread_mutex_lock(&sema->lock);
    sema->count++;
    int n = sema->count;
    futex_wake(&sema->count, n + 1);
    pthread_mutex_unlock(&sema->lock);
}

void pthread_semaphore_wait(pthread_sema_t * sema) {
    pthread_mutex_lock(&sema->lock);
    for ( ;; ) {
        if ( sema->count == 0 ) {
            pthread_mutex_unlock(&sema->lock);
            futex_wait(&sema->count);
            pthread_mutex_lock(&sema->lock);
        }
        else {
            sema->count--;
            break;
        }
    }
    pthread_mutex_unlock(&sema->lock);
}

void pthread_cond_init(pthread_cond_t * cond) {
    cond->total_seq = 0;
    cond->wakeup_seq = 0;
    cond->woken_seq = 0;
    cond->broadcast_seq = 0;
}

void pthread_cond_wait(pthread_cond_t * cond, pthread_lock_t * mutex) {
    cond->total_seq++;
    int val, seq;
    val = seq = cond->wakeup_seq;
    int bc = cond->broadcast_seq;

    for ( ;; ) {
        if ( cond->wakeup_seq == val ) {
            pthread_mutex_unlock(mutex);
            futex_wait(&cond->wakeup_seq);
            pthread_mutex_lock(mutex);
        }
        
        if ( bc != cond->broadcast_seq ) {
            goto bc_out;
        }

        val = cond->wakeup_seq;

        if ( val != seq && cond->woken_seq != val ) {
            break;
        }
    }

    cond->woken_seq++;

bc_out:

    if ( bc == cond->broadcast_seq ) {
        cond->wakeup_seq++;
        cond->woken_seq++;
    }

    futex_wake(&cond->wakeup_seq, INT_MAX);
}

void pthread_cond_signal(pthread_cond_t * cond) {
    if ( cond->total_seq > cond->wakeup_seq ) {
        cond->wakeup_seq++;
        futex_wake(&cond->wakeup_seq, 1);
    }
}

void pthread_cond_broadcast(pthread_cond_t * cond) {
    if ( cond->total_seq > cond->wakeup_seq ) {
        cond->wakeup_seq = cond->total_seq;
        cond->woken_seq = cond->total_seq;
        cond->broadcast_seq++;
        futex_wake(&cond->wakeup_seq, INT_MAX);
    }
}