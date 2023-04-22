#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <utils.h>
#include <threadpool.h>

// Worker thread arguments
typedef struct Args {
    int tid;
    ConcurrentQueue_t *tasks;
    int collector_fd;
    pthread_mutex_t *collector_fd_mutex;
} Args_t;

// Code exec by worker thread
static void *worker_fun(void *arg) {
    // Check arg
    if (arg == NULL) {
        fprintf(stderr, "worker_thread: \x1B[1;31merror:\x1B[0m Invalid argument\n");
        exit(EINVAL);
    }

    // Store arguments and free 'arg' structure
    Args_t *args = (Args_t*) arg; 

    int tid = args->tid;
    int collector_fd = args-> collector_fd;
    ConcurrentQueue_t *tasks = args->tasks;
    pthread_mutex_t *collector_fd_mutex = args->collector_fd_mutex;
    
    free(args);

    FILE *stream;
    char *filename;
    long result, number, i;
    int filename_size, error_number;

    // Loop
    while(1) {
        // Pop 'filename' from the queue
        if ((errno = 0, filename = popConcurrentQueue(tasks)) != NULL) {
            // Open binary file 'filename'
            if ((stream = fopen(filename, "rb")) != NULL) {
                // Calculate the result
                result = 0; i = 0;

                while (fread(&number, sizeof(long), 1, stream) != 0) {
                    result += i * number;
                    i++;
                }

                // Check if an error occurs in fread()
                if (feof(stream) != 0) {
                    // Length of 'filename' + '\0'
                    filename_size = strlen(filename) + 1;

                    LOCK_EXIT(collector_fd_mutex, error_number, tid)

                    // Send length of 'filename' to collector
                    if (writen(collector_fd, &filename_size, sizeof(int)) == -1) {
                        int errsv = errno;
                        fprintf(stderr, "thread[%d]: \x1B[1;31merror:\x1B[0m writen() 'filename_size': ", tid);
                        errno = errsv;
                        perror(NULL);
                        exit(errsv);
                    }

                    // Send 'filename' to collector
                    if (writen(collector_fd, filename, filename_size) == -1) {
                        int errsv = errno;
                        fprintf(stderr, "thread[%d]: \x1B[1;31merror:\x1B[0m writen() 'filename': ", tid);
                        errno = errsv;
                        perror(NULL);
                        exit(errsv);
                    }
                    
                    // Send result of 'filename' to collector
                    if (writen(collector_fd, &result, sizeof(long)) == -1) {
                        int errsv = errno;
                        fprintf(stderr, "thread[%d]: \x1B[1;31merror:\x1B[0m writen() 'result': ", tid);
                        errno = errsv;
                        perror(NULL);
                        exit(errsv);
                    }

                    UNLOCK_EXIT(collector_fd_mutex, error_number, tid)
                } else {
                    fprintf(stderr, "thread[%d]: \x1B[1;31merror:\x1B[0m fread() '%s'\n", tid, filename);
                }

                // Close 'filename'
                if (fclose(stream) == EOF) {
                    int errsv = errno;
                    fprintf(stderr, "thread[%d]: \x1B[1;31merror:\x1B[0m fclose() '%s': ", tid, filename);
                    errno = errsv;
                    perror(NULL);
                }
            } else {
                int errsv = errno;
                fprintf(stderr, "thread[%d]: \x1B[1;31merror:\x1B[0m fopen() '%s': ", tid, filename);
                errno = errsv;
                perror(NULL);
            }

            free(filename);
        } else {
            // 'filename' == NULL
            if (errno == 0) {
                // Success
                pthread_exit(NULL);
            } else {
                int errsv = errno;
                fprintf(stderr, "thread[%d]: \x1B[1;31merror:\x1B[0m popConcurrentQueue(): ", tid);
                errno = errsv;
                perror(NULL);
                exit(errsv);
            }
        }
    }
}

/*  Create a new thread pool with 'pool_size' worker threads and internal queue of size 'queue_size'. 
 *  Worker threads send results to file descriptor 'collector_fd'.
 *  The variable 'collector_fd_mutex' ensure correct synchronization between threads.
 *
 *  RETURN VALUE: pointer to the new thread pool on success
 *                NULL on error (errno is set)
 */
Threadpool_t *initThreadPool(size_t pool_size, size_t queue_size, int collector_fd, pthread_mutex_t *collector_fd_mutex) {
    // Check parameters
    if((pool_size == 0) || (queue_size == 0) || (collector_fd < 0) || (collector_fd_mutex == NULL)) {
	    errno = EINVAL;
        return NULL;
    }
    
    // Allocate thread pool data structure
    Threadpool_t *pool;

    if ((pool = malloc(sizeof(Threadpool_t))) == NULL) {
        int errsv = errno;
        fprintf(stderr, "\x1B[1;31merror:\x1B[0m malloc()\n");
        errno = errsv;
        return NULL;
    }

    // Allocate array for worker threads ID
    if ((pool->worker_threads = malloc(sizeof(pthread_t) * pool_size)) == NULL) {
        int errsv = errno;
        fprintf(stderr, "\x1B[1;31merror:\x1B[0m malloc()\n");
        free(pool);
        errno = errsv;
	    return NULL;
    }

    // Create thread pool concurrent queue of size 'queue_size'
    if ((pool->tasks = initConcurrentQueue(queue_size)) == NULL) {
        free(pool->worker_threads);
        free(pool);

        int errsv = errno;
        fprintf(stderr, "\x1B[1;31merror:\x1B[0m initConcurrentQueue()\n");
        errno = errsv;
        return NULL;
    }

    // Init pool variables
    pool->collector_fd_mutex = collector_fd_mutex;
    pool->pool_size = 0;

    // Create worker threads
    Args_t *args;
    int error_number;

    for(size_t i = 0; i < pool_size; i++) {
        // Allocate space for worker thread parameters
        if ((args = malloc(sizeof(Args_t))) == NULL) {
            int errsv = errno;
            shutdownThreadPool(pool);
            fprintf(stderr, "\x1B[1;31merror:\x1B[0m malloc()\n");
            errno = errsv;
            return NULL;
        }

        // Init arguments
        args->tid = i + 1;
        args->tasks = pool->tasks;
        args->collector_fd = collector_fd;
        args->collector_fd_mutex = collector_fd_mutex;

        // Spawn worker thread, exec 'worker_fun' function
        if((error_number = pthread_create(&pool->worker_threads[i], NULL, worker_fun, args)) != 0) {
            fprintf(stderr, "\x1B[1;31merror:\x1B[0m pthread_create()\n");
            free(args);
            shutdownThreadPool(pool);
            errno = error_number;
            return NULL;
        }

        // Increase the size of the thread pool
        pool->pool_size++;
    }

    return pool;
}

/*  Initiate an orderly shutdown of the thread pool 'pool' in which previously submitted tasks are executed.
 *
 *  RETURN VALUE: 0 on success
 *                -1 on error (errno is set)
 */
int shutdownThreadPool(Threadpool_t *pool) {  
    // Check pool
    if(pool == NULL) {
	    errno = EINVAL;
	    return -1;
    }

    // Insert a null pointer for each worker thread in the thread pool queue
    for(int i = 0; i < pool->pool_size; i++) {
        if (pushConcurrentQueue(pool->tasks, NULL) != 0) {
            int errsv = errno;
            fprintf(stderr, "\x1B[1;31merror:\x1B[0m pushConcurrentQueue()\n");
            errno = errsv;       
            return -1;
        }
    }

    // Wait for all worker threads to terminate
    for(int i = 0, error_number; i < pool->pool_size; i++) {
	    if ((error_number = pthread_join(pool->worker_threads[i], NULL)) != 0) {
            fprintf(stderr, "\x1B[1;31merror:\x1B[0m pthread_join()\n");
	        errno = error_number;
	        return -1;
        }
    }

    // Free thread pool resources
    deleteConcurrentQueue(pool->tasks);
    free(pool->worker_threads);
    free(pool);

    // Success
    return 0;
}

/*  Submit a new 'filename' for execution to the thread pool 'pool'. 
 *
 *  RETURN VALUE: 0 on success
 *                -1 on error (errno is set)
 */
int submitThreadPool(Threadpool_t *pool, char filename[]) {
    // Check parameters
    if(pool == NULL || filename == NULL) {
	    errno = EINVAL;
	    return -1;
    }

    // Insert 'filename' in the thread pool queue
    if (pushConcurrentQueue(pool->tasks, filename) != 0) {
        int errsv = errno;
        fprintf(stderr, "\x1B[1;31merror:\x1B[0m pushConcurrentQueue()\n");
        errno = errsv;
        return -1;
    }

    // Success
    return 0;
}

