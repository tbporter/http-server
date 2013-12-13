#ifndef _THREADPOOL_H
#define _THREADPOOL_H
/* Create a new thread pool with n threads. */
struct thread_pool* thread_pool_new(int nthreads);

/* Shutdown this thread pool.  May or may not execute already queued tasks. */
void thread_pool_shutdown(struct thread_pool *);

/* A function pointer representing a 'callable' */
typedef void * (* thread_pool_callable_func_t) (void * data);

/* Submit a callable to thread pool */
void thread_pool_submit(
        struct thread_pool *, 
        thread_pool_callable_func_t callable, 
        void * callable_data);

/* Don't need futures for this, events generate new events in themselves and
 * epoll will read when events should be spawned */
#endif