/* Tests pthread_create and pthread_join with no shared data. */
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <stdint.h>
#include "tests/lib.h"
#include "tests/main.h"

#define NUM_THREADS 10

int nums[NUM_THREADS];

static void worker_function(void * args) {
    intptr_t i = (intptr_t) args;
    for (long long j = 0; j < 20000000000L; j++){
        volatile long long k = j + j;
    }
    printf("output text\n");
    nums[i]++;
}

void
test_main(void)
{
    intptr_t i;
    for ( i = 0; i < NUM_THREADS; i++ ) {
        nums[i] = 0;
    }

    tid_t t[NUM_THREADS];

    for ( i = 0; i < NUM_THREADS; i++ ) {
        t[i] = pthread_create(worker_function, (void *) i);
        CHECK( t[i] != TID_ERROR, "pthread_create %zd", i);
    }

    for ( i = 0; i < NUM_THREADS; i++ ) {
        CHECK( pthread_join(t[i]), "pthread_join %zd", i);
    }

    for ( i = 0; i < NUM_THREADS; i++ ) {
        if ( nums[i] != 1 ) {
            fail("num[%zd] != 1", i);
        }
    }
}