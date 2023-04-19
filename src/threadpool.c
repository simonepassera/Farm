#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <utils.h>
#include <threadpool.h>

typedef struct Args {
    int tid;
    ConcurrentQueue_t *tasks;
    int collector_fd;
    pthread_mutex_t *collector_fd_mutex;
} Args_t;

static void *worker_fun(void *arg) {
    if (arg == NULL) {
        fprintf(stderr, "thread[%lu]: \x1B[1;31merror:\x1B[0m Invalid argument\n", pthread_self());
        exit(EINVAL);
    }

    Args_t *args = (Args_t*) arg;  
    int tid = args->tid;
    ConcurrentQueue_t *tasks = args->tasks;
    int collector_fd = args-> collector_fd;
    pthread_mutex_t *collector_fd_mutex = args->collector_fd_mutex;
    free(args);

    char *filename;
    FILE *stream;
    long result, number, i;
    int filename_size, error_number;

    while(1) {
        if ((errno = 0, filename = popConcurrentQueue(tasks)) != NULL) {
            if ((stream = fopen(filename, "rb")) != NULL) {
                result = 0; i = 0;

                while (fread(&number, sizeof(long), 1, stream) != 0) {
                    result += i * number;
                    i++;
                }

                if (feof(stream) != 0) {
                    filename_size = strlen(filename) + 1;

                    LOCK_EXIT(collector_fd_mutex, error_number, tid)

                    if (writen(collector_fd, &filename_size, sizeof(int)) == -1) {
                        int errsv = errno;
                        fprintf(stderr, "thread[%d]: \x1B[1;31merror:\x1B[0m writen() 'filename_size': ", tid);
                        errno = errsv;
                        perror(NULL);
                        exit(errsv);
                    }

                    if (writen(collector_fd, filename, filename_size) == -1) {
                        int errsv = errno;
                        fprintf(stderr, "thread[%d]: \x1B[1;31merror:\x1B[0m writen() 'filename': ", tid);
                        errno = errsv;
                        perror(NULL);
                        exit(errsv);
                    }
                    
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
            if (errno == 0) {
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

Threadpool_t *initThreadPool(size_t pool_size, size_t queue_size, int socket_fd) {
    if((pool_size == 0) || (queue_size == 0) || (socket_fd < 0)) {
	    errno = EINVAL;
        return NULL;
    }
    
    Threadpool_t *pool;
    if ((pool = malloc(sizeof(Threadpool_t))) == NULL) {
        int errsv = errno;
        fprintf(stderr, "\x1B[1;31merror:\x1B[0m malloc()\n");
        errno = errsv;
        return NULL;
    }

    if ((pool->worker_threads = malloc(sizeof(pthread_t) * pool_size)) == NULL) {
        int errsv = errno;
        fprintf(stderr, "\x1B[1;31merror:\x1B[0m malloc()\n");
        free(pool);
        errno = errsv;
	    return NULL;
    }

    if ((pool->tasks = initConcurrentQueue(queue_size)) == NULL) {
        free(pool->worker_threads);
        free(pool);

        int errsv = errno;
        fprintf(stderr, "\x1B[1;31merror:\x1B[0m initConcurrentQueue()\n");
        errno = errsv;
        return NULL;
    }

    if ((pool->collector_fd_mutex = malloc(sizeof(pthread_mutex_t))) == NULL) {
        int errsv = errno;

        deleteConcurrentQueue(pool->tasks);
        free(pool->worker_threads);
        free(pool);
        
        fprintf(stderr, "\x1B[1;31merror:\x1B[0m malloc()\n");
        errno = errsv;
        return NULL;
    }

    int error_number;

    if ((error_number = pthread_mutex_init(pool->collector_fd_mutex, NULL)) != 0) {
        deleteConcurrentQueue(pool->tasks);
        free(pool->worker_threads);
        free(pool->collector_fd_mutex);
        free(pool);
        
        fprintf(stderr, "\x1B[1;31merror:\x1B[0m pthread_mutex_init()\n");
        errno = error_number;
        return NULL;
    }

    pool->pool_size = 0;

    for(size_t i = 0; i < pool_size; i++) {
        Args_t *args;
        
        if ((args = malloc(sizeof(Args_t))) == NULL) {
            int errsv = errno;
            shutdownThreadPool(pool);
            fprintf(stderr, "\x1B[1;31merror:\x1B[0m malloc()\n");
            errno = errsv;
            return NULL;
        }

        args->tid = i + 1;
        args->tasks = pool->tasks;
        args->collector_fd = socket_fd;
        args->collector_fd_mutex = pool->collector_fd_mutex;

        if((error_number = pthread_create(&pool->worker_threads[i], NULL, worker_fun, args)) != 0) {
            fprintf(stderr, "\x1B[1;31merror:\x1B[0m pthread_create()\n");
            free(args);
            shutdownThreadPool(pool);
            errno = error_number;
            return NULL;
        }

        pool->pool_size++;
    }

    return pool;
}

int shutdownThreadPool(Threadpool_t *pool) {  
    if(pool == NULL) {
	    errno = EINVAL;
	    return -1;
    }

    int i;

    for(i = 0; i < pool->pool_size; i++) {
        if (pushConcurrentQueue(pool->tasks, NULL) != 0) {
            int errsv = errno;
            fprintf(stderr, "\x1B[1;31merror:\x1B[0m pushConcurrentQueue()\n");
            errno = errsv;       
            return -1;
        }
    }

    int error_number;

    for(i = 0; i < pool->pool_size; i++) {
	    if ((error_number = pthread_join(pool->worker_threads[i], NULL)) != 0) {
            fprintf(stderr, "\x1B[1;31merror:\x1B[0m pthread_join()\n");
	        errno = error_number;
	        return -1;
        }
    }

    deleteConcurrentQueue(pool->tasks);

    if ((error_number = pthread_mutex_destroy(pool->collector_fd_mutex)) != 0) {
        fprintf(stderr, "\x1B[1;31merror:\x1B[0m pthread_mutex_destroy()\n");
	    errno = error_number;
	    return -1;
    }

    free(pool->collector_fd_mutex);
    free(pool->worker_threads);
    free(pool);

    return 0;
}

int executeThreadPool(Threadpool_t *pool, char filename[]) {
    if(pool == NULL || filename == NULL) {
	    errno = EINVAL;
	    return -1;
    }

    if (pushConcurrentQueue(pool->tasks, filename) != 0) {
        int errsv = errno;
        fprintf(stderr, "\x1B[1;31merror:\x1B[0m pushConcurrentQueue()\n");
        errno = errsv;
        return -1;
    }

    return 0;
}

