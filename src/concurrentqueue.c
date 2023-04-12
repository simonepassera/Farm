#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <pthread.h>
#include <utils.h>
#include <concurrentqueue.h>

/* -------------------- Queue interface -------------------- */

ConcurrentQueue_t *initConcurrentQueue(size_t n) {
    if (n == 0) {
        fprintf(stderr, "\x1B[1;31merror:\x1B[0m initConcurrentQueue() 'n': ");
        errno = EINVAL;
        perror(NULL);
        return NULL;
    }

    int errsv;

    ConcurrentQueue_t *q = calloc(sizeof(ConcurrentQueue_t), 1);

    if (q == NULL) {
        errsv = errno;
	    fprintf(stderr, "\x1B[1;31merror:\x1B[0m calloc(): ");
        errno = errsv;
        perror(NULL);
        return NULL;
    }

    if (pthread_mutex_init(&q->mutex, NULL) != 0) {
        errsv = errno;
	    fprintf(stderr, "\x1B[1;31merror:\x1B[0m pthread_mutex_init(): ");
        errno = errsv;
        perror(NULL);
	    goto error;
    }

    if (pthread_cond_init(&q->cond_full, NULL) != 0) {
        errsv = errno;
        fprintf(stderr, "\x1B[1;31merror:\x1B[0m pthread_cond_init() 'cond_full': ");
        errno = errsv;
        perror(NULL);
        goto error;
    }

    if (pthread_cond_init(&q->cond_empty, NULL) != 0) {
        errsv = errno;
        fprintf(stderr, "\x1B[1;31merror:\x1B[0m pthread_cond_init() 'cond_empty': ");
        errno = errsv;
        perror(NULL);
        goto error;
    }

    if ((q->buf = malloc(sizeof(char*) * n)) == NULL) {
        errsv = errno;
	    fprintf(stderr, "\x1B[1;31merror:\x1B[0m malloc(): ");
        errno = errsv;
        perror(NULL);
        goto error;
    }

    q->head = 0;
    q->tail = 0;
    q->qlen = 0;
    q->qsize = n;

    return q;

    error:

    errsv = errno;

    if (&q->mutex != NULL) pthread_mutex_destroy(&q->mutex);
    if (&q->cond_full != NULL) pthread_cond_destroy(&q->cond_full);
    if (&q->cond_empty) pthread_cond_destroy(&q->cond_empty);
    if (q->buf != NULL) free(q->buf); 
    free(q);

    errno = errsv;
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
    if ((q == NULL) || (filename == NULL)) {
        errno = EINVAL;
	    return -1;
    }

    int ret;

    LOCK(&q->mutex, ret, -1)

    while (q->qlen == q->qsize) {
        WAIT(&q->cond_full, &q->mutex, ret, -1)
    }

    q->buf[q->tail] = filename;

    q->tail += ((q->tail + 1) >= q->qsize) ? (1 - q->qsize) : 1;
    q->qlen += 1;

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

    while (q->qlen == 0) {
        WAIT(&q->cond_empty, &q->mutex, ret, NULL)
    }

    char *filename = q->buf[q->head];
    q->buf[q->head] = NULL;

    q->head += ((q->head + 1) >= q->qsize) ? (1 - q->qsize) : 1;
    q->qlen -= 1;

    SIGNAL(&q->cond_full, ret, NULL)  
    UNLOCK(&q->mutex, ret, NULL)
    
    return filename;
} 

