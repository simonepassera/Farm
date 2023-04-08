#ifndef __LIBUTIL_H__
#define __LIBUTIL_H__

#include <errno.h>

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

// Check if the string 's' is a number and store in 'n'
extern int isNumber(const char *s, long *n);
/*
RETURN: 0 -> success
        1 -> not a number
        2 -> overflow/underflow
*/

#endif /* __LIBUTIL_H__ */
