#define _POSIX_C_SOURCE 200809L

#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <libutil.h>
#include <libqueue.h>

#define PATHNAME_MAX 255

void info(char *pathname);
void usage();

int main(int argc, char *argv[]) {
    // Default options
    long nthread = 4, qlen = 8, delay = 0;
    char *dirname = NULL;

    if (argc == 1) {
        info(argv[0]);
        exit(EXIT_FAILURE);
    }

    struct stat statbuf;
    extern char *optarg;
    int opt, errsv;

    while ((opt = getopt(argc, argv, "n:q:t:d:h")) != -1) {
        switch (opt) {
            case 'n':
                if ((isNumber(optarg, &nthread) != 0) || (nthread < 1)) {
                    fprintf(stderr, "%s: invalid option argument -- 'n'\n", argv[0]);
                    info(argv[0]);
                    exit(EXIT_FAILURE);
                }
                break;
            case 'q':
                if ((isNumber(optarg, &qlen) != 0) || (qlen < 1)) {
                    fprintf(stderr, "%s: invalid option argument -- 'q'\n", argv[0]);
                    info(argv[0]);
                    exit(EXIT_FAILURE);
                }
                break;
            case 't':
                if ((isNumber(optarg, &delay) != 0) || (delay < 0)) {
                    fprintf(stderr, "%s: invalid option argument -- 't'\n", argv[0]);
                    info(argv[0]);
                    exit(EXIT_FAILURE);
                }
                break;
            case 'd':
                memset(&statbuf, 0, sizeof(statbuf));
                
                if (stat(optarg, &statbuf) == -1) {
                    errsv = errno;
                    fprintf(stderr, "%s: cannot accesss '%s': %s\n", argv[0], optarg, strerror(errsv));
                    exit(errsv);
                }

                if (!S_ISDIR(statbuf.st_mode)) {
                    fprintf(stderr, "%s: invalid option argument -- 'd'\n", argv[0]);
                    info(argv[0]);
                    exit(EXIT_FAILURE);
                }

                dirname = optarg;
                break;
            case 'h':
                usage();
                exit(EXIT_SUCCESS);
            case '?':
                info(argv[0]);
                exit(EXIT_FAILURE);
        }
    }

    extern int optind;

    if (dirname == NULL && optind == argc) {
        info(argv[0]);
        exit(EXIT_FAILURE);
    }

    Queue_t *requests;
    CHECK_EXIT("initQueue", (requests = initQueue()) == NULL, 1)

    while (optind != argc) {
        memset(&statbuf, 0, sizeof(statbuf));

        if (stat(argv[optind], &statbuf) == -1) {
            errsv = errno;
            fprintf(stderr, "%s: cannot accesss '%s': %s\n", argv[0], argv[optind], strerror(errsv));
            deleteQueue(requests, free);
            exit(errsv);
        }
        
        if (!S_ISREG(statbuf.st_mode)) {
            fprintf(stderr, "%s: '%s': Not a regular file\n", argv[0], argv[optind]);
            info(argv[0]);
            deleteQueue(requests, free);
            exit(EXIT_FAILURE);
        }
        
        CHECK_EXIT("push", push(requests, argv[optind]) == -1, 1)

        optind++;
    }

    if (dirname != NULL) {
        // TODO
    }
    
    exit(EXIT_SUCCESS);
}

void info(char *pathname) {
    fprintf(stderr, "Try '%s -h' for more information.\n", pathname);
}

void usage() {
    fprintf(stderr, "Usage: \x1B[1mfarm\x1B[22m [OPTION]... [FILE]...\n");
    fprintf(stderr, "Read binary FILEs and return for each of them a calculation based on the numbers present in the FILEs.\n");
    fprintf(stderr, "The printing of the results is sorted in ascending order.\n\n");
    fprintf(stderr, "  \x1B[1m-h\x1B[22m\x1B[21Gdisplay this help and exit\n");
    fprintf(stderr, "  \x1B[1m-n\x1B[22m nthread\x1B[21Gnumber of Worker threads of the MasterWorker process (default value \x1B[1m4\x1B[22m)\n");
    fprintf(stderr, "  \x1B[1m-q\x1B[22m qlen\x1B[21Glength of the concurrent queue between the Master thread and the Worker threads (default value \x1B[1m8\x1B[22m)\n");
    fprintf(stderr, "  \x1B[1m-d\x1B[22m dirname\x1B[21Gcalculate the result for each binary FILE in the directory and subdirectories\n");
    fprintf(stderr, "  \x1B[1m-t\x1B[22m delay\x1B[21Gtime in milliseconds between the sending of two successive requests to\n\x1B[21Gthe Worker threads by the Master thread (default value \x1B[1m0\x1B[22m)\n\n");
    fprintf(stderr, "  \x1B[1mSignals:\x1B[22m\n\n");
    fprintf(stderr, "  Signal\x1B[44GAction\x1B[22m\n");
    fprintf(stderr, "  ──────────────────────────────────────────────────────────────────────────────────────────────────\n");
    fprintf(stderr, "  \x1B[1mSIGUSR1\x1B[22m\x1B[44Gprint the results calculated up to that moment\n");
    fprintf(stderr, "  \x1B[1mSIGINT - SIGQUIT - SIGTERM - SIGHUP\x1B[22m\x1B[44Gcomplete any tasks in the queue and terminate the process\n");
}