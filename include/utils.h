#ifndef __UTILS_H__
#define __UTILS_H__

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <pthread.h>

#define PATHNAME_MAX 255
#define SOCK_PATH "farm.sck"

#define SYSCALL_EXIT(name, return_value, syscall) \
    if ((return_value = syscall) == -1) {         \
        perror(name);                             \
        exit(errno);			                  \
    }

#define SYSCALL_RETURN(name, return_value, syscall) \
    if ((return_value = syscall) == -1) {           \
        perror(name);                               \
        return -1;			                        \
    }

#define CHECK_EXIT(progname, message, condition, print_errno)           \
    if (condition) {                                                    \
        if (print_errno) {                                              \
            int errsv = errno;                                          \
            fprintf(stderr, "%s: \x1B[1;31merror:\x1B[0m ", progname);  \
            errno = errsv;                                              \
            perror(message);                                            \
            exit(errsv);                                                \
        } else {                                                        \
            exit(EXIT_FAILURE);                                         \
        }                                                               \
    }

#define CHECK_RETURN(progname, message, condition, print_errno)         \
    if (condition) {                                                    \
        if (print_errno) {                                              \
            int errsv = errno;                                          \
            fprintf(stderr, "%s: \x1B[1;31merror:\x1B[0m ", progname);  \
            errno = errsv;                                              \
            perror(message);                                            \
            return errsv;                                               \
        } else {                                                        \
            return EXIT_FAILURE;                                        \
        }                                                               \
    }

#define LOCK_EXIT(mutex, error_number, tid)                                                 \
    if ((error_number = pthread_mutex_lock(mutex)) != 0) {                                  \
        fprintf(stderr, "\x1B[1;31mthread[%d]: error:\x1B[0m pthread_mutex_lock(): ", tid); \
        errno = error_number;	                                                            \
        perror(NULL);                                                                       \
        exit(error_number);                                                                 \
    }

#define UNLOCK_EXIT(mutex, error_number, tid)                                                   \
    if ((error_number = pthread_mutex_unlock(mutex)) != 0) {                                      \
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

extern int readn(int fd, void *buf, size_t n);
extern int writen(int fd, void *buf, size_t n);

#endif /* __UTILS_H__ */
