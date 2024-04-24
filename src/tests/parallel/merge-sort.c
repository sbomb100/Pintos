#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include "lib/random.c"
#include "lib/user/malloc.h"
#include "tests/lib.h"
#include "tests/main.h"
#include "tests/parallel/threadpool.h"

#define insertion_sort_threshold 16
#define min_task_size 1000
#define nthreads 4

/* ------------------------------------------------------------- 
 * Utilities: insertion sort.
 */
static void insertionsort(int *a, int lo, int hi) 
{
    int i;
    for (i = lo+1; i <= hi; i++) {
        int j = i;
        int t = a[j];
        while (j > lo && t < a[j - 1]) {
            a[j] = a[j - 1];
            --j;
        }
        a[j] = t;
    }
}


static void
merge(int * a, int * b, int bstart, int left, int m, int right)
{
    if (a[m] <= a[m+1])
        return;

    memcpy(b + bstart, a + left, (m - left + 1) * sizeof (a[0]));
    int i = bstart;
    int j = m + 1;
    int k = left;

    while (k < j && j <= right) {
        if (b[i] < a[j])
            a[k++] = b[i++];
        else
            a[k++] = a[j++];
    }
    memcpy(a + k, b + i, (j - k) * sizeof (a[0]));
}

/* ------------------------------------------------------------- 
 * Serial implementation.
 */
static void
mergesort_internal(int * array, int * tmp, int left, int right)
{
    if (right - left < insertion_sort_threshold) {
        insertionsort(array, left, right);
    } else {
        int m = (left + right) / 2;
        mergesort_internal(array, tmp, left, m);
        mergesort_internal(array, tmp, m + 1, right);
        merge(array, tmp, 0, left, m, right);
    }
}

static void
mergesort_serial(int * array, int n)
{
    if (n < insertion_sort_threshold) {
        insertionsort(array, 0, n);
    } else {
        int * tmp = malloc(sizeof(int) * (n / 2 + 1));
        mergesort_internal(array, tmp, 0, n-1);
        free (tmp);
    }
}

/* ------------------------------------------------------------- 
 * Parallel implementation.
 */

/* msort_task describes a unit of parallel work */
struct msort_task {
    int *array;
    int *tmp;
    int left, right;
}; 


/* Parallel mergesort */
static void  
mergesort_internal_parallel(struct thread_pool * threadpool, struct msort_task * s)
{
    int * array = s->array;
    int * tmp = s->tmp;
    int left = s->left;
    int right = s->right;

    if (right - left <= min_task_size) {
        mergesort_internal(array, tmp + left, left, right);
    } else {
        int m = (left + right) / 2;

        struct msort_task mleft = {
            .left = left,
            .right = m,
            .array = array,
            .tmp = tmp
        };
        struct future * lhalf = thread_pool_submit(threadpool, 
                                   (fork_join_task_t) mergesort_internal_parallel,  
                                   &mleft);

        struct msort_task mright = {
            .left = m + 1,
            .right = right,
            .array = array,
            .tmp = tmp
        };
        mergesort_internal_parallel(threadpool, &mright);
        future_get(lhalf);
        future_free(lhalf);
        merge(array, tmp, left, left, m, right);
    }
}

static void 
mergesort_parallel(int *array, int N) 
{
    int * tmp = malloc(sizeof(int) * (N));
    struct msort_task root = {
        .left = 0, .right = N-1, .array = array, .tmp = tmp
    };

    struct thread_pool * threadpool = thread_pool_new(nthreads);
    struct future * top = thread_pool_submit(threadpool,
                                             (fork_join_task_t) mergesort_internal_parallel,
                                             &root);
    future_get(top);
    future_free(top);
    thread_pool_shutdown_and_destroy(threadpool);
    free (tmp);
}



void test_main (void) {
    int N = 100000;
    int * array = (int *) malloc(sizeof(int) * N);
    int * array2 = (int *) malloc(sizeof(int) * N);
    int i;
    for (i = 0; i < N; i++) {
        array[i] = random_ulong() % N;
        array2[i] = array[i];
    }

    mergesort_serial(array, N);
    mergesort_parallel(array2, N);

    for (i = 0; i < N; i++) {
        if (array[i] != array2[i]) {
            fail("arrays differ at index %d", i);
        }
    }

    // pass();
}