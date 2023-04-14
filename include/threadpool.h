#ifndef __THREADPOOL_H__
#define __THREADPOOL_H__

#include <pthread.h>
#include <concurrentqueue.h>

typedef struct Threadpool {
    pthread_t *worker_threads;
    int pool_size;
    ConcurrentQueue_t *tasks;
    pthread_mutex_t *collector_fd_mutex;
} Threadpool_t;

/*

 */
extern Threadpool_t *initThreadPool(size_t pool_size, size_t queue_size, int socket_fd);

/*

 */
extern int shutdownThreadPool(Threadpool_t *pool);

/*

 */
extern int executeThreadPool(Threadpool_t *pool, char filename[]);

#endif /* __THREADPOOL_H__ */