/* Tests pthread_create and pthread_join with no shared data. */

#include <string.h>
#include <pthread.h>
#include <stdint.h>
#include "tests/lib.h"
#include "tests/main.h"

#define NUM_THREADS 10

int nums[NUM_THREADS];

static void worker_function(void * args) {
    int * i = (int *) args;
    nums[*i]++;
}

void
test_main(void)
{
    int i;
    for ( i = 0; i < NUM_THREADS; i++ ) {
        nums[i] = 0;
    }

    tid_t t[NUM_THREADS];

    for ( i = 0; i < NUM_THREADS; i++ ) {
        t[i] = pthread_create(worker_function, &i);
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