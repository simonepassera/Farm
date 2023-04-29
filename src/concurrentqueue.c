#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <pthread.h>
#include <utils.h>
#include <concurrentqueue.h>

/*  Initialize a concurrent queue of size 'n'. 
 *
 *  RETURN VALUE: pointer to the new queue on success
 *                NULL on error (errno is set)
 */
ConcurrentQueue_t *initConcurrentQueue(size_t n) {
    // Check size
    if (n == 0) {
        errno = EINVAL;
        return NULL;
    }

    // Allocate concurrent queue data structure
    ConcurrentQueue_t *q;
    
    if ((q = malloc(sizeof(ConcurrentQueue_t))) == NULL) {
        int errsv = errno;
	    fprintf(stderr, "\x1B[1;31merror:\x1B[0m malloc()\n");
        errno = errsv;
        return NULL;
    }

    // Allocate filename array of size 'n'
    if ((q->buf = malloc(sizeof(char*) * n)) == NULL) {
        int errsv = errno;
	    fprintf(stderr, "\x1B[1;31merror:\x1B[0m malloc()\n");
        free(q);
        errno = errsv;
        return NULL;
    }

    int error_number;
    
    // Init queue mutex
    if ((error_number = pthread_mutex_init(&q->mutex, NULL)) != 0) {
	    fprintf(stderr, "\x1B[1;31merror:\x1B[0m pthread_mutex_init()\n");
        free(q->buf);
        free(q);
        errno = error_number;
        return NULL;
    }

    // Init condition variable 'full' where the producers will be suspended
        if ((error_number = pthread_cond_init(&q->cond_full, NULL)) != 0) {
        fprintf(stderr, "\x1B[1;31merror:\x1B[0m pthread_cond_init() 'cond_full'\n");
        pthread_mutex_destroy(&q->mutex);
        free(q->buf);
        free(q);
        errno = error_number;
        return NULL;
    }

    // Init condition variable 'empty' where the consumers will be suspended
    if ((error_number = pthread_cond_init(&q->cond_empty, NULL)) != 0) {
        fprintf(stderr, "\x1B[1;31merror:\x1B[0m pthread_cond_init() 'cond_empty'\n");
        pthread_mutex_destroy(&q->mutex);
        pthread_cond_destroy(&q->cond_full);
        free(q->buf);
        free(q);
        errno = error_number;
        return NULL;
    }

    // Init variables
    q->head = 0;
    q->tail = 0;
    q->qlen = 0;
    q->qsize = n;
    q->num_prod = 0;
    q->num_cons = 0;

    // Return pointer to the concurrent queue
    return q;
}

// Delete a queue allocated with initConcurrentQueue() pointed to by q
void deleteConcurrentQueue(ConcurrentQueue_t *q) {
    if (q != NULL) {
        // Free all filenames if present
	    while(q->head != q->tail) {
            free(q->buf[q->head]);
            q->head += ((q->head + 1) >= q->qsize) ? (1 - q->qsize) : 1;
        }

        // Free resources in the queue data structure
        pthread_mutex_destroy(&q->mutex);
        pthread_cond_destroy(&q->cond_full);
        pthread_cond_destroy(&q->cond_empty);
        free(q->buf); 
        
        // Free queue
        free(q);
    }
}

/*  Insert filename into the queue pointed to by q.
 * 
 *  RETURN VALUE: 0 on success
 *                -1 on error (errno is set)
 */
int pushConcurrentQueue(ConcurrentQueue_t *q, char filename[]) {
    // Check arguments, filename 'NULL' allowed for the Worker thread termination protocol
    if ((q == NULL)) {
        errno = EINVAL;
	    return -1;
    }

    int error_number;

    LOCK_RETURN(&q->mutex, error_number, -1)

    // Increase the number of waiting producers
    q->num_prod++;

    // Wait if queue is full
    while (q->qlen == q->qsize) {
        WAIT_RETURN(&q->cond_full, &q->mutex, error_number, -1)
    }

    // Producer no longer waiting
    q->num_prod--;

    // Insert filename in the queue
    q->buf[q->tail] = filename;
    q->tail += ((q->tail + 1) >= q->qsize) ? (1 - q->qsize) : 1;
    q->qlen += 1;

    // Signal any consumers
    if (q->num_cons > 0)
        SIGNAL_RETURN(&q->cond_empty, error_number, -1)  

    UNLOCK_RETURN(&q->mutex, error_number, -1)

    // Success
    return 0;
}

/* Pull filename from the queue.
 *
 *  RETURN VALUE: pointer to the filename on success
 *                NULL on error (errno is set)
 */
char *popConcurrentQueue(ConcurrentQueue_t *q) {
    // Check queue pointer
    if (q == NULL) {
        errno = EINVAL;
	    return NULL;
    }

    int error_number;

    LOCK_RETURN(&q->mutex, error_number, NULL)

    // Increase the number of waiting consumers
    q->num_cons++;

    // Wait if queue is empty
    while (q->qlen == 0) {
        WAIT_RETURN(&q->cond_empty, &q->mutex, error_number, NULL)
    }

    // Consumer no longer waiting
    q->num_cons--;

    // Remove filename from the queue
    char *filename = q->buf[q->head];
    q->buf[q->head] = NULL;
    q->head += ((q->head + 1) >= q->qsize) ? (1 - q->qsize) : 1;
    q->qlen -= 1;

    // Signal any producers
    if (q->num_prod > 0)
        SIGNAL_RETURN(&q->cond_full, error_number, NULL)  
    
    UNLOCK_RETURN(&q->mutex, error_number, NULL)
    
    return filename;
} 

