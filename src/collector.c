#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <utils.h>
#include <collector.h>

typedef struct Results {
    char *filename;
    long result;
} Results_t;

static void deleteResults(Results_t **results, size_t size) {
    if (size != 0) {
        for (size_t i = 0; i < size; i++) {
            free(results[i]->filename);
            free(results[i]);
        }
    }

    free(results);
}

static int comparResults(const void *a, const void *b) {
    Results_t *res_a = *(Results_t**)a;
    Results_t *res_b = *(Results_t**)b;

    if (res_a->result < res_b->result) {
        return -1;
    } else if (res_a->result > res_b->result) {
        return 1;
    } else {
        return 0;
    }
}

static void printResults(Results_t **results, size_t size, int *ordered) {
    if (size != 0) {
        if (!*ordered) {
            qsort(results, size, sizeof(Results_t*), comparResults);
            *ordered = 1;
        }

        static int first = 1;
        if (!first) printf("\n");
        first = 0;

        for (size_t i = 0; i < size; i++) {
            printf("%ld %s\n", results[i]->result, results[i]->filename);
        }
    }
}

void exec_collector() {
    int sfd;

    if ((sfd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
        int errsv = errno;
        fprintf(stderr, "collector: \x1B[1;31merror:\x1B[0m socket(): %s\n", strerror(errsv)); 
        exit(errsv);
    }

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, SOCK_PATH, sizeof(addr.sun_path) - 1);

    if (connect(sfd, (struct sockaddr*) &addr, sizeof(addr)) == -1) {
        int errsv = errno;
        fprintf(stderr, "collector: \x1B[1;31merror:\x1B[0m connect(): %s\n", strerror(errsv)); 
        exit(errsv);
    }

    int n_results, read_return;

    if ((read_return = readn(sfd, &n_results, sizeof(int))) == -1) {
        int errsv = errno;
        fprintf(stderr, "collector: \x1B[1;31merror:\x1B[0m readn() 'n_results': %s\n", strerror(errsv)); 
        close(sfd);
        exit(errsv);
    } else if (read_return == 0) {
        close(sfd);
        exit(EXIT_FAILURE);      
    }
    
    if (n_results <= 0){
        fprintf(stderr, "collector: \x1B[1;31merror:\x1B[0m 'n_results': Invalid value\n"); 
        close(sfd);
        exit(EXIT_FAILURE);
    }

    Results_t **results;

    if ((results = calloc(n_results, sizeof(Results_t*))) == NULL) {
        int errsv = errno;
        fprintf(stderr, "collector: \x1B[1;31merror:\x1B[0m calloc(): %s\n", strerror(errsv));
        close(sfd);
        exit(errsv);
    }

    int opcode, ordered = 0;
    size_t results_index = 0;

    while (1) {
        if ((read_return = readn(sfd, &opcode, sizeof(int))) == -1) {
            int errsv = errno;
            fprintf(stderr, "collector: \x1B[1;31merror:\x1B[0m readn() 'opcode': %s\n", strerror(errsv));
            deleteResults(results, results_index);
            close(sfd);
            exit(errsv);
        } else if (read_return == 0) {
            fprintf(stderr, "collector: \x1B[1;31merror:\x1B[0m readn() 'opcode': EOF\n");
            deleteResults(results, results_index);
            close(sfd);
            exit(EXIT_FAILURE);
        }

        if (opcode == 0) {
            printResults(results, results_index, &ordered);
        } else if (opcode > 0) {
            Results_t *new_result;

            if ((new_result = malloc(sizeof(Results_t))) == NULL) {
                int errsv = errno;
                fprintf(stderr, "collector: \x1B[1;31merror:\x1B[0m malloc(): %s\n", strerror(errsv));
                deleteResults(results, results_index);
                close(sfd);
                exit(errsv);
            }

            if ((new_result->filename = malloc(opcode * sizeof(char))) == NULL) {
                int errsv = errno;
                fprintf(stderr, "collector: \x1B[1;31merror:\x1B[0m malloc(): %s\n", strerror(errsv));
                deleteResults(results, results_index);
                close(sfd);
                exit(errsv);
            }

            if ((read_return = readn(sfd, new_result->filename, opcode * sizeof(char))) == -1) {
                int errsv = errno;
                fprintf(stderr, "collector: \x1B[1;31merror:\x1B[0m readn() 'filename': %s\n", strerror(errsv));
                deleteResults(results, results_index);
                close(sfd);
                exit(errsv);
            } else if (read_return == 0) {
                fprintf(stderr, "collector: \x1B[1;31merror:\x1B[0m readn() 'filename': EOF\n");
                deleteResults(results, results_index);
                close(sfd);
                exit(EXIT_FAILURE);
            }

            if ((read_return = readn(sfd, &new_result->result, sizeof(long))) == -1) {
                int errsv = errno;
                fprintf(stderr, "collector: \x1B[1;31merror:\x1B[0m readn() 'result': %s\n", strerror(errsv));
                deleteResults(results, results_index);
                close(sfd);
                exit(errsv);
            } else if (read_return == 0) {
                fprintf(stderr, "collector: \x1B[1;31merror:\x1B[0m readn() 'result': EOF\n");
                deleteResults(results, results_index);
                close(sfd);
                exit(EXIT_FAILURE);
            }

            if (results_index >= ((size_t) n_results)) {
                fprintf(stderr, "collector: \x1B[1;31merror:\x1B[0m Too many results\n");
                deleteResults(results, results_index);
                close(sfd);
                exit(EXIT_FAILURE);
            }

            results[results_index] = new_result;
            results_index++;
            ordered = 0;
        } else {
            printResults(results, results_index, &ordered);
            deleteResults(results, results_index);
            close(sfd);
            exit(EXIT_SUCCESS);
        }
    }
}