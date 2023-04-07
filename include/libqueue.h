#ifndef __LIBQUEUE_H__
#define __LIBQUEUE_H__

// Queue element
typedef struct Node {
    void *data;
    struct Node *next;
} Node_t;

// Queue data structure
typedef struct Queue {
    Node_t *head;
    Node_t *tail;
    long qlen; 
} Queue_t;

/*  Initialize a queue. 
 *
 *  RETURN VALUE: pointer to the new queue on success
 *                NULL on error (errno is set)
 */
Queue_t *initQueue();

/*  Delete a queue allocated with initQueue() pointed to by q.
 *  If fun is not NULL, fun is apply to all element in the queue.
 */
void deleteQueue(Queue_t *q, void (*fun)(void*));

/*  Insert data into the queue pointed to by q.
 * 
 *  RETURN VALUE: 0 on success
 *                -1 on error (errno is set)
 */
int push(Queue_t *q, void *data);

/* Pull data from the queue.
 *
 *  RETURN VALUE: pointer to the data on success
 *                NULL on empty queue or on error (errno is set)
 */
void *pop(Queue_t *q);

// Return the current length of the queue passed as a parameter.
long length(Queue_t *q);

#endif /* __LIBQUEUE_H__ */