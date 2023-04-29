#ifndef __THREADPOOL_H__
#define __THREADPOOL_H__

#include <pthread.h>
#include <concurrentqueue.h>

// Thread pool data structure
typedef struct Threadpool {
    int pool_size;
    pthread_t *worker_threads;
    ConcurrentQueue_t *tasks;
    pthread_mutex_t *collector_fd_mutex;
} Threadpool_t;


/* -------------------- ThreadPool interface -------------------- */


/*  Create a new thread pool with 'pool_size' Worker threads and internal queue of size 'queue_size'. 
 *  Worker threads send results to file descriptor 'collector_fd'.
 *  The variable 'collector_fd_mutex' ensures correct synchronization between threads.
 *
 *  RETURN VALUE: pointer to the new thread pool on success
 *                NULL on error (errno is set)
 */
extern Threadpool_t *initThreadPool(size_t pool_size, size_t queue_size, int collector_fd, pthread_mutex_t *collector_fd_mutex);

/*  Initiate an orderly shutdown of the thread pool 'pool' in which previously submitted tasks are executed.
 *
 *  RETURN VALUE: 0 on success
 *                -1 on error (errno is set)
 */
extern int shutdownThreadPool(Threadpool_t *pool);

/*  Submit a new 'filename' for execution to the thread pool 'pool'. 
 *
 *  RETURN VALUE: 0 on success
 *                -1 on error (errno is set)
 */
extern int submitThreadPool(Threadpool_t *pool, char filename[]);

#endif /* __THREADPOOL_H__ */