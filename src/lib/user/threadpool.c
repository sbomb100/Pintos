#include "lib/user/pthread.h"
#include "lib/stdint.h"
#include "lib/stdbool.h"
#include "lib/user/malloc.h"
#include "lib/user/list.h"

#include <stdio.h>
#include "threadpool.h"

enum future_status {
    NOT_STARTED,
    IN_PROGRESS,
    COMPLETED,
};

struct thread_pool {
    int nthreads;
    struct list * global_tasks;
    pthread_cond_t cond;
    pthread_lock_t global_mutex;
    struct worker * workers;
};

struct worker {	
    tid_t t;
    struct thread_pool * pool;
    struct list * local_tasks;
    pthread_lock_t local_mutex;
    pthread_lock_t shutdown_mutex;
    bool shutting_down;
    bool internal;
};

struct future {
    fork_join_task_t task;
    pthread_cond_t f_cond;
    struct list_elem elem;
    enum future_status status;
    struct worker * w;
    struct worker * w_help;
    struct thread_pool * pool;
    void* args;
    void* result;
};

static bool attempt_work_steal(void);

static void complete_task(struct future * fut, pthread_lock_t mutex) {
    struct worker * current_worker = pthread_tls_load();
    fut->w_help = current_worker;
    fut->status = IN_PROGRESS;
    pthread_mutex_unlock(mutex);
    fut->result = fut->task(fut->pool, fut->args);
    pthread_mutex_lock(mutex);
    fut->status = COMPLETED;
    pthread_cond_signal(fut->f_cond);
}

static bool attempt_work_steal() {
    struct worker * current_worker = pthread_tls_load();

    for ( int i = 0; i < current_worker->pool->nthreads; i++ ) {
        struct worker * w_i = &current_worker->pool->workers[i];

        pthread_mutex_lock(w_i->local_mutex);
        if ( !list_empty(w_i->local_tasks) ) {
            struct list_elem * e = list_pop_back(w_i->local_tasks);
            struct future * fut = list_entry(e, struct future, elem);
            complete_task(fut, w_i->local_mutex);
            pthread_mutex_unlock(w_i->local_mutex);
        }
        pthread_mutex_unlock(w_i->local_mutex);
    }

    return false;
}

static void find_tasks(struct future * f, pthread_cond_t cv) {
    if ( f != NULL ) {
        pthread_mutex_lock(f->w_help->local_mutex);
        if ( !list_empty(f->w_help->local_tasks) ) {
            struct list_elem * e = list_pop_back(f->w_help->local_tasks);
            struct future * fut = list_entry(e, struct future, elem);
            complete_task(fut, f->w_help->local_mutex);
            pthread_mutex_unlock(f->w_help->local_mutex);
        }
        else {
            pthread_mutex_unlock(f->w_help->local_mutex);
            pthread_mutex_lock(f->w->local_mutex);
            if ( f->status != COMPLETED ) {
                pthread_cond_wait(cv, f->w->local_mutex);
            }
            pthread_mutex_unlock(f->w->local_mutex);
        }
    } 
    else {
        struct worker * current_worker = pthread_tls_load();
        pthread_mutex_lock(current_worker->pool->global_mutex);
        if ( !list_empty(current_worker->pool->global_tasks) ) {
            struct list_elem * e = list_pop_back(current_worker->pool->global_tasks);
            struct future * fut = list_entry(e, struct future, elem);
            complete_task(fut, current_worker->pool->global_mutex);
            pthread_mutex_unlock(current_worker->pool->global_mutex);
            return;
        }
        pthread_mutex_unlock(current_worker->pool->global_mutex);

        if ( !attempt_work_steal() ) {
            pthread_mutex_lock(current_worker->pool->global_mutex);
            pthread_mutex_lock(current_worker->shutdown_mutex);
            if ( !current_worker->shutting_down &&
               list_empty(current_worker->pool->global_tasks) ) {
                pthread_mutex_unlock(current_worker->shutdown_mutex);
                pthread_cond_wait(cv, current_worker->pool->global_mutex);
                pthread_mutex_unlock(current_worker->pool->global_mutex);
                return;
               }
            pthread_mutex_unlock(current_worker->shutdown_mutex);
            pthread_mutex_unlock(current_worker->pool->global_mutex);
        }
    }
}

/* Starting routine for worker created by p_thread. */
static void worker_function(void * arg) {
    struct worker * current_worker = arg;

    pthread_tls_store(current_worker, sizeof(struct worker));

    pthread_mutex_lock(current_worker->shutdown_mutex);
    while ( !current_worker->shutting_down ) {
        pthread_mutex_unlock(current_worker->shutdown_mutex);
        find_tasks(NULL, current_worker->pool->cond);
        pthread_mutex_lock(current_worker->shutdown_mutex);
    }
    pthread_mutex_unlock(current_worker->shutdown_mutex);
}

/* Create a new thread pool with no more than n threads. */
struct thread_pool * thread_pool_new(int nthreads) {
    struct thread_pool * thread_pool = malloc(sizeof(struct thread_pool));

    thread_pool->global_mutex = pthread_mutex_init();
    thread_pool->cond = pthread_cond_init();
    struct list * g_tasks = malloc(sizeof(struct list));
    list_init(g_tasks);
    thread_pool->global_tasks = g_tasks;

    thread_pool->workers = malloc(nthreads * sizeof(struct worker));

    for ( int i = 0; i < nthreads; i++ ) {
        thread_pool->workers[i].pool = thread_pool;
        struct list * l_tasks = malloc(sizeof(struct list));
        list_init(l_tasks);
        thread_pool->workers[i].local_tasks = l_tasks;
        thread_pool->workers[i].shutting_down = false;
        thread_pool->workers[i].internal = true;
        thread_pool->workers[i].local_mutex = pthread_mutex_init();
        thread_pool->workers[i].shutdown_mutex = pthread_mutex_init();
    }

    for ( int i = 0; i < nthreads; i++ ) {
        thread_pool->workers[i].t = pthread_create(worker_function, &thread_pool->workers[i]);
    }

    return thread_pool;
}

/* 
 * Shutdown this thread pool in an orderly fashion.  
 * Tasks that have been submitted but not executed may or
 * may not be executed.
 *
 * Deallocate the thread pool object before returning. 
 */
void thread_pool_shutdown_and_destroy(struct thread_pool * pool) {
    int i;
    pthread_mutex_lock(pool->global_mutex);
    for ( i = 0; i < pool->nthreads; i++ ) {
        pthread_mutex_lock(pool->workers[i].shutdown_mutex);
        pool->workers[i].shutting_down = true;
        pthread_cond_signal(pool->cond);
        pthread_mutex_unlock(pool->workers[i].shutdown_mutex);
    }
    pthread_mutex_unlock(pool->global_mutex);
    
    for ( i = 0; i < pool->nthreads; i++ ) {
        pthread_join(pool->workers[i].t);
    }

    free(pool->global_tasks);

    for ( int i = 0; i < pool->nthreads; i++ ) {
        free(pool->workers[i].local_tasks);
    }
    free(pool->workers);
    free(pool);
}

/* 
 * Submit a fork join task to the thread pool and return a
 * future.  The returned future can be used in future_get()
 * to obtain the result.
 * 'pool' - the pool to which to submit
 * 'task' - the task to be submitted.
 * 'data' - data to be passed to the task's function
 *
 * Returns a future representing this computation.
 */
struct future * thread_pool_submit(struct thread_pool *pool, fork_join_task_t task, void * data) {
    struct future * future;
    if ( is_main_thread() ) {
        future = malloc(sizeof(struct future));
        future->f_cond = pthread_cond_init();
        future->task = task;
        future->args = data;
        future->status = NOT_STARTED;
        future->pool = pool;
        pthread_mutex_lock(pool->global_mutex);
        list_push_front(pool->global_tasks, &future->elem);
        pthread_mutex_unlock(pool->global_mutex);
    }
    else {
        struct worker * current_worker = pthread_tls_load();
        future = malloc(sizeof(struct future));
        future->f_cond = pthread_cond_init();
        future->task = task;
        future->args = data;
        future->status = NOT_STARTED;
        future->pool = pool;
        pthread_mutex_lock(current_worker->local_mutex);
        list_push_front(current_worker->local_tasks, &future->elem);
        pthread_mutex_unlock(current_worker->local_mutex);
    }
    pthread_cond_signal(pool->cond);

    return future;
}

/* Make sure that the thread pool has completed the execution
 * of the fork join task this future represents.
 *
 * Returns the value returned by this task.
 */
void * future_get(struct future * fut) {
    if ( is_main_thread() ) {
        pthread_mutex_lock(fut->pool->global_mutex);
        if ( fut->status != COMPLETED ) {
            pthread_cond_wait(fut->f_cond, fut->pool->global_mutex);
        }
        pthread_mutex_unlock(fut->pool->global_mutex);
        return fut->result;
    }
    else {
        pthread_mutex_lock(fut->w->local_mutex);
        if ( fut->status == COMPLETED ) {
            pthread_mutex_unlock(fut->w->local_mutex);
            return fut->result;
        }

        if ( fut->status == NOT_STARTED ) {
            list_remove(&fut->elem);
            pthread_mutex_unlock(fut->w->local_mutex);
            fut->result = fut->task(fut->pool, fut->args);
            return fut->result;
        }
        
        while ( fut->status != COMPLETED ) {
            pthread_mutex_unlock(fut->w->local_mutex);
            find_tasks(fut, fut->f_cond);
            pthread_mutex_lock(fut->w->local_mutex);
        }
        pthread_mutex_unlock(fut->w->local_mutex);
        return fut->result;
    }
}

/* Deallocate this future.  Must be called after future_get() */
void future_free(struct future * fut) {
    free(fut);
}
