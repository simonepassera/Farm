#define _POSIX_C_SOURCE 200809L

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <libutil.h>

#define PATHNAME_MAX 255

void info(char *pathname);
void usage();
int parser(int argc, char *argv[], long *nthread, long *qlen, long *delay, char **dirname);

int main(int argc, char *argv[]) {
    // Default options
    long nthread = 4, qlen = 8, delay = 0;
    char *dirname = NULL;

    if (parser(argc, argv, &nthread, &qlen, &delay, &dirname) != 0) {
        if (dirname != NULL) free(dirname);
        exit(EXIT_FAILURE);
    }

    exit(EXIT_SUCCESS);
}

void info(char *pathname) {
    fprintf(stderr, "Try '%s -h' for more information.\n", pathname);
}

void usage() {
    fprintf(stderr, "Usage: \x1B[1mfarm\x1B[22m [OPTION]... [FILE]...\n");
    fprintf(stderr, "Read binary files and returns for each of theme a calculation based on the numbers present in files.\n\n");
    fprintf(stderr, "  \x1B[1m-h\x1B[22m\x1B[21Gdisplay this help and exit\n");
    fprintf(stderr, "  \x1B[1m-n\x1B[22m nthread\x1B[21Gnumber of Worker threads of the MasterWorker process (default value \x1B[1m4\x1B[22m)\n");
    fprintf(stderr, "  \x1B[1m-q\x1B[22m qlen\x1B[21Glength of the concurrent queue between the Master thread and the Worker threads (default value \x1B[1m8\x1B[22m)\n");
    fprintf(stderr, "  \x1B[1m-d\x1B[22m dirname\x1B[21Gcalculates the result for each binary file in the directory and subdirectories\n");
    fprintf(stderr, "  \x1B[1m-t\x1B[22m delay\x1B[21Gtime in milliseconds between the sending of two successive requests to\n\x1B[21Gthe Worker threads by the Master thread (default value \x1B[1m0\x1B[22m)\n\n");
    fprintf(stderr, "  \x1B[1mSignals:\x1B[22m\n\n");
    fprintf(stderr, "  Signal\x1B[44GAction\x1B[22m\n");
    fprintf(stderr, "  ──────────────────────────────────────────────────────────────────────────────────────────────────\n");
    fprintf(stderr, "  \x1B[1mSIGUSR1\x1B[22m\x1B[44Gprints the results calculated up to that moment\n");
    fprintf(stderr, "  \x1B[1mSIGINT - SIGQUIT - SIGTERM - SIGHUP\x1B[22m\x1B[44Gcomplete any tasks in the queue and terminate the process\n");
}

int parser(int argc, char *argv[], long *nthread, long *qlen, long *delay, char **dirname) {
    if (argc == 1) {
        info(argv[0]);
        return 1;
    }

    extern char *optarg;
    int opt;

    while ((opt = getopt(argc, argv, "hn:q:d:t:")) != -1) {
        switch (opt) {
            case 'h':
                usage();
                return 1;
                break;
            case 'n':
                if ((isNumber(optarg, nthread) != 0) || (*nthread < 1)) {
                    fprintf(stderr, "%s: invalid option argument -- 'n'\n", argv[0]);
                    info(argv[0]);
                    return 1;
                }
                break;
            case 'q':
                if ((isNumber(optarg, qlen) != 0) || (*qlen < 1)) {
                    fprintf(stderr, "%s: invalid option argument -- 'q'\n", argv[0]);
                    info(argv[0]);
                    return 1;
                }
                break;
            case 'd':
                CHECK_EXIT("strndup", (*dirname = strndup(optarg, PATHNAME_MAX)) == NULL, 1)
                break;
            case 't':
                if ((isNumber(optarg, delay) != 0) || (*delay < 0)) {
                    fprintf(stderr, "%s: invalid option argument -- 't'\n", argv[0]);
                    info(argv[0]);
                    return 1;
                }
                break;
            case '?':
                info(argv[0]);
                return 1;
        }
    }

    extern int optind;

    if (*dirname == NULL && argv[optind] == NULL) {
        info(argv[0]);
        return 1;
    }

    return 0;
}