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

#define LOCK(mutex, error_number, return_value)                             \
    if ((error_number = pthread_mutex_lock(mutex)) != 0) {                  \
        fprintf(stderr, "\x1B[1;31merror:\x1B[0m pthread_mutex_lock(): ");  \
        errno = error_number;	                                            \
        perror(NULL);                                                       \
        return return_value;                                                \
    }

#define UNLOCK(mutex, error_number, return_value)                               \
    if ((error_number = pthread_mutex_unlock(mutex)) != 0) {                    \
        fprintf(stderr, "\x1B[1;31merror:\x1B[0m pthread_mutex_unlock(): ");    \
        errno = error_number;	                                                \
        perror(NULL);                                                           \
        return return_value;                                                    \
    }

#define WAIT(cond, mutex, error_number, return_value)                       \
    if ((error_number = pthread_cond_wait(cond ,mutex)) != 0) {             \
        fprintf(stderr, "\x1B[1;31merror:\x1B[0m pthread_cond_wait(): ");   \
        errno = error_number;	                                            \
        perror(NULL);                                                       \
        return return_value;                                                \
    }

#define SIGNAL(cond, error_number, return_value)                            \
    if ((error_number = pthread_cond_signal(cond)) != 0) {                  \
        fprintf(stderr, "\x1B[1;31merror:\x1B[0m pthread_cond_signal(): "); \
        errno = error_number;	                                            \
        perror(NULL);                                                       \
        return return_value;                                                \
    }

#endif /* __UTILS_H__ */
