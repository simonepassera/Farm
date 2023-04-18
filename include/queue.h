#ifndef __QUEUE_H__
#define __QUEUE_H__

// Queue element
typedef struct Node {
    char *filename;
    struct Node *next;
} Node_t;

// Queue data structure
typedef struct Queue {
    Node_t *head;
    Node_t *tail;
    size_t qlen; 
} Queue_t;

/*  Initialize a queue. 
 *
 *  RETURN VALUE: pointer to the new queue on success
 *                NULL on error (errno is set)
 */
extern Queue_t *initQueue();

// Delete a queue allocated with initQueue() pointed to by q.
extern void deleteQueue(Queue_t *q);

/*  Insert filename into the queue pointed to by q.
 * 
 *  RETURN VALUE: 0 on success
 *                -1 on error (errno is set)
 */
extern int pushQueue(Queue_t *q, char filename[]);

/* Pull filename from the queue.
 *
 *  RETURN VALUE: pointer to the filename on success
 *                NULL on empty queue or on error (errno is set)
 */
extern char *popQueue(Queue_t *q);

// Return the current length of the queue passed as a parameter.
extern size_t lengthQueue(Queue_t *q);

#endif /* __QUEUE_H__ */