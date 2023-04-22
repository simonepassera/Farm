#ifndef __UTILS_H__
#define __UTILS_H__

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <pthread.h>

// Pathname to UNIX socket 
#define SOCK_PATH "farm.sck"

#define SYSCALL_EXIT(progname, message, syscall)                    \
    if (syscall == -1) {                                            \
        int errsv = errno;                                          \
        fprintf(stderr, "%s: \x1B[1;31merror:\x1B[0m ", progname);  \
        errno = errsv;                                              \
        perror(message);                                            \
        exit(errsv);                                                \
    }

#define LOCK_EXIT(mutex, error_number, tid)                                                 \
    if ((error_number = pthread_mutex_lock(mutex)) != 0) {                                  \
        fprintf(stderr, "\x1B[1;31mthread[%d]: error:\x1B[0m pthread_mutex_lock(): ", tid); \
        errno = error_number;	                                                            \
        perror(NULL);                                                                       \
        exit(error_number);                                                                 \
    }

#define UNLOCK_EXIT(mutex, error_number, tid)                                                   \
    if ((error_number = pthread_mutex_unlock(mutex)) != 0) {                                    \
        fprintf(stderr, "\x1B[1;31mthread[%d]: error:\x1B[0m pthread_mutex_unlock(): ", tid);   \
        errno = error_number;	                                                                \
        perror(NULL);                                                                           \
        exit(error_number);                                                                     \
    }

#define LOCK_RETURN(mutex, error_number, return_value)                      \
    if ((error_number = pthread_mutex_lock(mutex)) != 0) {                  \
        fprintf(stderr, "\x1B[1;31merror:\x1B[0m pthread_mutex_lock()\n");  \
        errno = error_number;	                                            \
        return return_value;                                                \
    }

#define UNLOCK_RETURN(mutex, error_number, return_value)                        \
    if ((error_number = pthread_mutex_unlock(mutex)) != 0) {                    \
        fprintf(stderr, "\x1B[1;31merror:\x1B[0m pthread_mutex_unlock()\n");    \
        errno = error_number;	                                                \
        return return_value;                                                    \
    }

#define WAIT_RETURN(cond, mutex, error_number, return_value)                \
    if ((error_number = pthread_cond_wait(cond ,mutex)) != 0) {             \
        fprintf(stderr, "\x1B[1;31merror:\x1B[0m pthread_cond_wait()\n");   \
        errno = error_number;	                                            \
        return return_value;                                                \
    }

#define SIGNAL_RETURN(cond, error_number, return_value)                     \
    if ((error_number = pthread_cond_signal(cond)) != 0) {                  \
        fprintf(stderr, "\x1B[1;31merror:\x1B[0m pthread_cond_signal()\n"); \
        errno = error_number;	                                            \
        return return_value;                                                \
    }

/*  Read 'n' byte from file descriptor 'fd' into the buffer starting at 'buf'. 
 *
 *  RETURN VALUE: n on success
 *                0 indicates end of file
 *                -1 on error (errno is set)
 */
extern int readn(int fd, void *buf, size_t n);

/*  Write 'n' byte from the buffer starting at 'buf' to file descriptor 'fd'.
 *
 *  RETURN VALUE: n on success
 *                -1 on error (errno is set)
 */
extern int writen(int fd, void *buf, size_t n);

#endif /* __UTILS_H__ */
