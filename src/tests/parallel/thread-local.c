/* Tests the functionality of thread local storage. 
    1. The TLS of the main thread should be NULL
    2. pthread_tls_store should properly store memory.
    3. pthread_tls_load should properly load old memory.
    4. subsequent pthread_tls_store calls should update the TLS block.
*/

#include <string.h>
#include <pthread.h>
#include <stdint.h>
#include "tests/lib.h"
#include "tests/main.h"

#define NUM_THREADS 10

struct tls {
    intptr_t num;
};

bool pass[NUM_THREADS];

static void worker_function(void * args) {
    intptr_t i = (intptr_t) args;
    struct tls var_1 = {
        .num = i
    };

    pthread_tls_store(&var_1, sizeof(struct tls));

    struct tls * var_2 = pthread_tls_load();
    if ( var_2->num != i ) {
        pass[i] = false;
        return;
    }

    var_2->num = i + 10;

    pthread_tls_store(var_2, sizeof(struct tls));

    struct tls * var_3 = pthread_tls_load();
    if ( var_3->num == i + 10 ) {
        pass[i] = true;
    }
    else {
        pass[i] = false;
    }
}

void
test_main(void)
{
    /* if ( pthread_tls_load != NULL ) {
        fail("main thread TLS is non-null");
    } */

    intptr_t i;
    tid_t t[NUM_THREADS];

    for ( i = 0; i < NUM_THREADS; i++ ) {
        pass[i] = false;
    }

    for ( i = 0; i < NUM_THREADS; i++ ) {
        t[i] = pthread_create(worker_function, (void *) i);
        CHECK( t[i] != TID_ERROR, "pthread_create %zd", i);
    }

    for ( i = 0; i < NUM_THREADS; i++ ) {
        CHECK( pthread_join(t[i]), "pthread_join %zd", i);
    }

    for ( i = 0; i < NUM_THREADS; i++ ) {
        if ( !pass[i] ) {
            fail("pass[%zd] = false", i);
        }
    }
}