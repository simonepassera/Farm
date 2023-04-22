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

// Result of a filename
typedef struct Result {
    char *filename;
    long result;
} Result_t;

// Cleanup array of results
static void deleteResults(Result_t **results, size_t size) {
    if (size != 0) {
        for (size_t i = 0; i < size; i++) {
            free(results[i]->filename);
            free(results[i]);
        }
    }

    free(results);
}

// Quicksort compare function
static int comparResults(const void *a, const void *b) {
    Result_t *res_a = *(Result_t**)a;
    Result_t *res_b = *(Result_t**)b;

    if (res_a->result < res_b->result) {
        return -1;
    } else if (res_a->result > res_b->result) {
        return 1;
    } else {
        return 0;
    }
}

// Print results sorted with quicksort algorithm
static void printResults(Result_t **results, size_t size, int *ordered) {
    if (size != 0) {
        if (!*ordered) {
            // Sort array in ascending order
            qsort(results, size, sizeof(Result_t*), comparResults);
            *ordered = 1;
        }

        static int first = 1;
        if (!first) printf("\n");
        first = 0;

        // Print results
        for (size_t i = 0; i < size; i++) {
            printf("%ld %s\n", results[i]->result, results[i]->filename);
        }
    }
}

// Code exec by Collector process
void exec_collector() {
    // Create a new UNIX socket
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

    // Connect to the Master process
    if (connect(sfd, (struct sockaddr*) &addr, sizeof(addr)) == -1) {
        int errsv = errno;
        fprintf(stderr, "collector: \x1B[1;31merror:\x1B[0m connect(): %s\n", strerror(errsv)); 
        exit(errsv);
    }

    int n_results, read_return;

    // Read the total number of results
    // If read EOF terminate the process, error in the Master process (Commonly caused by CLI parsing)
    if ((read_return = readn(sfd, &n_results, sizeof(int))) == -1) {
        int errsv = errno;
        fprintf(stderr, "collector: \x1B[1;31merror:\x1B[0m readn() 'n_results': %s\n", strerror(errsv)); 
        close(sfd);
        exit(errsv);
    } else if (read_return == 0) {
        close(sfd);
        exit(EXIT_FAILURE);      
    }
    
    // Check the read value
    if (n_results <= 0){
        fprintf(stderr, "collector: \x1B[1;31merror:\x1B[0m 'n_results': Invalid value\n"); 
        close(sfd);
        exit(EXIT_FAILURE);
    }

    // Allocate results array of size 'n_results'
    Result_t **results;
    size_t results_index = 0;

    if ((results = malloc(sizeof(Result_t*) * n_results)) == NULL) {
        int errsv = errno;
        fprintf(stderr, "collector: \x1B[1;31merror:\x1B[0m malloc(): %s\n", strerror(errsv));
        close(sfd);
        exit(errsv);
    }

    int opcode, ordered = 0;

    // Loop
    while (1) {
        // Read opcode
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

        switch (opcode) {
            // Successful termination of the process
            case 0:
                printResults(results, results_index, &ordered);
                deleteResults(results, results_index);
                close(sfd);
                exit(EXIT_SUCCESS);
            // Print partial results
            case -1:
                printResults(results, results_index, &ordered);
                break;
            // Read new result of filename
            default:
                // Check the read value
                if (opcode < 0) {
                    fprintf(stderr, "collector: \x1B[1;31merror:\x1B[0m 'opcode': Invalid value\n\n");
                    deleteResults(results, results_index);
                    close(sfd);
                    exit(EXIT_FAILURE);
                }

                // Allocate result structure
                Result_t *new_result;

                if ((new_result = malloc(sizeof(Result_t))) == NULL) {
                    int errsv = errno;
                    fprintf(stderr, "collector: \x1B[1;31merror:\x1B[0m malloc(): %s\n", strerror(errsv));
                    deleteResults(results, results_index);
                    close(sfd);
                    exit(errsv);
                }

                // Allocate memory for filename of size 'opcode' 
                if ((new_result->filename = malloc(opcode * sizeof(char))) == NULL) {
                    int errsv = errno;
                    fprintf(stderr, "collector: \x1B[1;31merror:\x1B[0m malloc(): %s\n", strerror(errsv));
                    deleteResults(results, results_index);
                    close(sfd);
                    exit(errsv);
                }

                // Read filename
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

                // Read result
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

                // Check if master process send more results than 'n_results'
                if (results_index >= ((size_t) n_results)) {
                    fprintf(stderr, "collector: \x1B[1;31merror:\x1B[0m Too many results\n");
                    deleteResults(results, results_index);
                    close(sfd);
                    exit(EXIT_FAILURE);
                }

                // Add new result
                results[results_index] = new_result;
                results_index++;
                ordered = 0;
        }
    }
}