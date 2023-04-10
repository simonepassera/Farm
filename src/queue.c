#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <utils.h>
#include <queue.h>

/* -------------------- Utility functions -------------------- */

static inline Queue_t *allocQueue() {
    return malloc(sizeof(Queue_t));
}

static inline Node_t *allocNode() {
    return malloc(sizeof(Node_t));
}

/* -------------------- Queue interface -------------------- */

Queue_t *initQueue() {
    Queue_t *q = allocQueue();
    if (q == NULL) return NULL;

    q->head = NULL;
    q->tail = NULL;    
    q->qlen = 0;
    
    return q;
}

void deleteQueue(Queue_t *q) {
    if (q != NULL) {
        if (q->qlen != 0) {
            Node_t *n;

            while(q->head != NULL) {
                n = q->head;
                q->head = q->head->next;
                free(n->filename);
                free(n);
            }
        }
        
        free(q);
    }
}

int push(Queue_t *q, char filename[]) {
    if ((q == NULL) || (filename == NULL)) {
        errno = EINVAL;
        return -1;
    }

    Node_t *n = allocNode();
    if (n == NULL) return -1;

    n->filename = strndup(filename, PATHNAME_MAX); 
    if (n->filename == NULL) return -1;
    n->next = NULL;

    q->qlen += 1;

    if (q->tail == NULL) {
        q->head = n;
        q->tail = n;
    } else {
        q->tail->next = n;
        q->tail = n;
    }

    return 0;
}

void *pop(Queue_t *q) {        
    if (q == NULL) {
        errno = EINVAL;
        return NULL;
    }
    
    if (q->qlen == 0) {
        return NULL;
    } else {
        Node_t *n = q->head;
        if ((q->head = q->head->next) == NULL) q->tail = NULL;
        q->qlen -= 1;
        char *filename = n->filename;
        free(n);

        return filename;
    }
} 
 
long length(Queue_t *q) {
    if (q == NULL) {
        errno = EINVAL;
        return -1;
    }

    return q->qlen;
}

