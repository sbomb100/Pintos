#include <pthread.h>
#include "tests/lib.h"
#include "tests/main.h"

#define NUM_THREADS 20

pthread_lock_t lock;
int counter = 0;

static void increment(void *) {
    for ( int i = 0; i < 1000; i++ ) {
        lock_acquire(&lock);
        counter++;
        lock_release(&lock);
    }
}

void test_main(void) {
    tid_t threads[NUM_THREADS];

    lock_init(&lock);

    msg("Value of lock: %d\n", lock);

    // Create threads
    for (intptr_t i = 0; i < NUM_THREADS; i++) {
        threads[i] = pthread_create(increment, (void *)i);
        CHECK(threads[i] != TID_ERROR, "pthread_create %zd", i);
    }
 
    // Join threads
    for (intptr_t i = 0; i < NUM_THREADS; i++) {
        CHECK(pthread_join(threads[i]), "pthread_join %zd", i);
    }

    if ( counter != 20000 ) {
        fail("%d != 20000", counter);
    }
}
