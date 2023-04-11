#ifndef __UTILS_H__
#define __UTILS_H__

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>

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

#endif /* __UTILS_H__ */
