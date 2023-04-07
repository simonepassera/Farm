#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <libqueue.h>

/* -------------------- Utility functions -------------------- */

static inline Node_t *allocNode() {
    return malloc(sizeof(Node_t));
}

static inline Queue_t *allocQueue() {
    return malloc(sizeof(Queue_t));
}

static inline void freeNode(Node_t *node) {
    free(node);
}

/* -------------------- Queue interface -------------------- */

Queue_t *initQueue() {
    Queue_t *q = allocQueue();
    if (q == NULL) return NULL;

    q->head = allocNode();
    if (q->head == NULL) return NULL;
    
    q->head->data = NULL; 
    q->head->next = NULL;
    q->tail = q->head;    
    q->qlen = 0;
    
    return q;
}

void deleteQueue(Queue_t *q, void (*fun)(void*)) {
    while(q->head != q->tail) {
        Node_t *p = (Node_t*) q->head;
	    q->head = q->head->next;
        if (fun != NULL) fun(p->data);
	    freeNode(p);
    }

    if (q->head != NULL) freeNode(q->head);
    free(q);
}

int push(Queue_t *q, void *data) {
    if ((q == NULL) || (data == NULL)) {
        errno = EINVAL;
        return -1;
    }

    Node_t *n = allocNode();
    if (n == NULL) return -1;

    n->data = data; 
    n->next = NULL;

    q->tail->next = n;
    q->tail = n;
    q->qlen += 1;

    return 0;
}

void *pop(Queue_t *q) {        
    if (q == NULL) {
        errno = EINVAL;
        return NULL;
    }
    
    if (q->head == q->tail) {
        return NULL;
    } else {
        Node_t *n = (Node_t*) q->head;
        q->head = q->head->next;
        q->qlen -= 1;
        freeNode(n);

        return q->head->data;
    }
} 
 
long length(Queue_t *q) {
    if (q == NULL) {
        errno = EINVAL;
        return -1;
    }

    return q->qlen;
}

