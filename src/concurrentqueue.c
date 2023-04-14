#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <pthread.h>
#include <utils.h>
#include <concurrentqueue.h>

/* -------------------- Queue interface -------------------- */

ConcurrentQueue_t *initConcurrentQueue(size_t n) {
    if (n == 0) {
        errno = EINVAL;
        return NULL;
    }

    ConcurrentQueue_t *q = calloc(sizeof(ConcurrentQueue_t), 1);

    if (q == NULL) {
        int errsv = errno;
	    fprintf(stderr, "\x1B[1;31merror:\x1B[0m calloc()\n");
        errno = errsv;
        return NULL;
    }

    if ((q->buf = malloc(sizeof(char*) * n)) == NULL) {
        int errsv = errno;
	    fprintf(stderr, "\x1B[1;31merror:\x1B[0m malloc()\n");
        free(q);
        errno = errsv;
        return NULL;
    }

    int error_number;

    if ((error_number = pthread_mutex_init(&q->mutex, NULL)) != 0) {
	    fprintf(stderr, "\x1B[1;31merror:\x1B[0m pthread_mutex_init()\n");
	    goto error;
    }

    if ((error_number = pthread_cond_init(&q->cond_full, NULL)) != 0) {
        fprintf(stderr, "\x1B[1;31merror:\x1B[0m pthread_cond_init() 'cond_full'\n");
        goto error;
    }

    if ((error_number = pthread_cond_init(&q->cond_empty, NULL)) != 0) {
        fprintf(stderr, "\x1B[1;31merror:\x1B[0m pthread_cond_init() 'cond_empty'\n");
        goto error;
    }

    q->head = 0;
    q->tail = 0;
    q->qlen = 0;
    q->qsize = n;
    q->num_prod = 0;
    q->num_cons = 0;

    return q;

    error:

    if (&q->mutex != NULL) pthread_mutex_destroy(&q->mutex);
    if (&q->cond_full != NULL) pthread_cond_destroy(&q->cond_full);
    if (&q->cond_empty) pthread_cond_destroy(&q->cond_empty);
    if (q->buf != NULL) free(q->buf); 
    free(q);

    errno = error_number;
    return NULL;
}

void deleteConcurrentQueue(ConcurrentQueue_t *q) {
    if (q != NULL) {
	    while(q->head != q->tail) {
            free(q->buf[q->head]);
            q->head += ((q->head + 1) >= q->qsize) ? (1 - q->qsize) : 1;
        }

        if (&q->mutex != NULL) pthread_mutex_destroy(&q->mutex);
        if (&q->cond_full != NULL) pthread_cond_destroy(&q->cond_full);
        if (&q->cond_empty) pthread_cond_destroy(&q->cond_empty);
        if (q->buf != NULL) free(q->buf); 
        
        free(q);
    }
}

int pushConcurrentQueue(ConcurrentQueue_t *q, char filename[]) {
    if ((q == NULL)) {
        errno = EINVAL;
	    return -1;
    }

    int ret;

    LOCK(&q->mutex, ret, -1)

    q->num_prod++;

    while (q->qlen == q->qsize) {
        WAIT(&q->cond_full, &q->mutex, ret, -1)
    }

    q->num_prod--;

    q->buf[q->tail] = filename;

    q->tail += ((q->tail + 1) >= q->qsize) ? (1 - q->qsize) : 1;
    q->qlen += 1;

    if (q->num_cons > 0)
        SIGNAL(&q->cond_empty, ret, -1)  

    UNLOCK(&q->mutex, ret, -1)
    
    return 0;
}

char *popConcurrentQueue(ConcurrentQueue_t *q) {
    if (q == NULL) {
        errno = EINVAL;
	    return NULL;
    }

    int ret;

    LOCK(&q->mutex, ret, NULL)

    q->num_cons++;

    while (q->qlen == 0) {
        WAIT(&q->cond_empty, &q->mutex, ret, NULL)
    }

    q->num_cons--;

    char *filename = q->buf[q->head];
    q->buf[q->head] = NULL;

    q->head += ((q->head + 1) >= q->qsize) ? (1 - q->qsize) : 1;
    q->qlen -= 1;

    if (q->num_prod > 0)
        SIGNAL(&q->cond_full, ret, NULL)  
    
    UNLOCK(&q->mutex, ret, NULL)
    
    return filename;
} 

