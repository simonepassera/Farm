#ifndef __CONCURRENTQUEUE_H__
#define __CONCURRENTQUEUE_H__

// Queue data structure
typedef struct ConcurrentQueue {
    char **buf;
    size_t head;
    size_t tail;
    size_t qsize;
    size_t qlen;
    pthread_mutex_t mutex;
    pthread_cond_t cond_full;
    pthread_cond_t cond_empty;
} ConcurrentQueue_t;


/*  Initialize a concurrent queue of size 'n'. 
 *
 *  RETURN VALUE: pointer to the new queue on success
 *                NULL on error (print errno message)
 */
extern ConcurrentQueue_t *initConcurrentQueue(size_t n);

// Delete a queue allocated with initConcurrentQueue() pointed to by q.
extern void deleteConcurrentQueue(ConcurrentQueue_t *q);

/*  Insert filename into the queue pointed to by q.
 * 
 *  RETURN VALUE: 0 on success
 *                -1 on error (errno is set)
 */
extern int pushConcurrentQueue(ConcurrentQueue_t *q, char filename[]);

/* Pull filename from the queue.
 *
 *  RETURN VALUE: pointer to the filename on success
 *                NULL on error (errno is set)
 */
extern char *popConcurrentQueue(ConcurrentQueue_t *q);

#endif /* __CONCURRENTQUEUE_H__ */
