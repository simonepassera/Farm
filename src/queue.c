#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <queue.h>

/*  Initialize an unbounded queue. 
 *
 *  RETURN VALUE: pointer to the new queue on success
 *                NULL on error (errno is set)
 */
Queue_t *initQueue() {
    // Allocate queue data structure
    Queue_t *q;

    if ((q = malloc(sizeof(Queue_t))) == NULL) {
        int errsv = errno;
        fprintf(stderr, "\x1B[1;31merror:\x1B[0m malloc()\n");
        errno = errsv;
        return NULL;
    }

    // Init variables
    q->head = NULL;
    q->tail = NULL;    
    q->qlen = 0;
    
    // Return pointer to queue
    return q;
}

// Delete a queue allocated with initQueue() pointed to by q
void deleteQueue(Queue_t *q) {
    if (q != NULL) {
        // Free all filename if present
        if (q->qlen != 0) {
            Node_t *n;

            while(q->head != NULL) {
                n = q->head;
                q->head = q->head->next;
                free(n->filename);
                free(n);
            }
        }
        
        // Free queue
        free(q);
    }
}

/*  Insert filename into the queue pointed to by q.
 * 
 *  RETURN VALUE: 0 on success
 *                -1 on error (errno is set)
 */
int pushQueue(Queue_t *q, char filename[]) {
    // Check parameters
    if ((q == NULL) || (filename == NULL)) {
        errno = EINVAL;
        return -1;
    }

    // Allocate new node
    Node_t *n;

    if ((n = malloc(sizeof(Node_t))) == NULL) {
        int errsv = errno;
        fprintf(stderr, "\x1B[1;31merror:\x1B[0m malloc()\n");
        errno = errsv;
        return -1;
    }

    n->next = NULL;

    // Insert filename in the node
    size_t filename_len = strlen(filename);
 
    if ((n->filename = malloc(filename_len + 1)) == NULL) {
        int errsv = errno;
        fprintf(stderr, "\x1B[1;31merror:\x1B[0m malloc()\n");
        errno = errsv;
        return -1;
    }

    memcpy(n->filename, filename, filename_len);
    n->filename[filename_len] = '\0';

    if (q->tail == NULL) {
        q->head = n;
        q->tail = n;
    } else {
        q->tail->next = n;
        q->tail = n;
    }

    q->qlen++;

    // Success
    return 0;
}

/* Pull filename from the queue.
 *
 *  RETURN VALUE: pointer to the filename on success
 *                NULL on empty queue or on error (errno is set)
 */
char *popQueue(Queue_t *q) {
    // Check queue pointer        
    if (q == NULL) {
        errno = EINVAL;
        return NULL;
    }
    
    // Check if queue is empty
    if (q->qlen == 0) {
        return NULL;
    } else {
        // Remove filename from the queue
        Node_t *n = q->head;
        char *filename = n->filename;
        if ((q->head = q->head->next) == NULL) q->tail = NULL;
        q->qlen--;
        free(n);

        return filename;
    }
} 

// Return the current length of the queue passed as a parameter
size_t lengthQueue(Queue_t *q) {
    // Check queue pointer
    if (q == NULL) {
        errno = EINVAL;
        return -1;
    }
    
    return q->qlen;
}

