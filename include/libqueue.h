#ifndef __LIBQUEUE_H__
#define __LIBQUEUE_H__

#define PATHNAME_MAX 255

// Queue element
typedef struct Node {
    char *filename;
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

// Delete a queue allocated with initQueue() pointed to by q.
void deleteQueue(Queue_t *q);

/*  Insert filename into the queue pointed to by q.
 * 
 *  RETURN VALUE: 0 on success
 *                -1 on error (errno is set)
 */
int push(Queue_t *q, char *filename);

/* Pull filename from the queue.
 *
 *  RETURN VALUE: pointer to the filename on success
 *                NULL on empty queue or on error (errno is set)
 */
void *pop(Queue_t *q);

// Return the current length of the queue passed as a parameter.
long length(Queue_t *q);

#endif /* __LIBQUEUE_H__ */