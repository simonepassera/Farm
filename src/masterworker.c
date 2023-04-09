#define _POSIX_C_SOURCE 200809L

#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <dirent.h>
#include <libutil.h>
#include <libqueue.h>

void info(char pathname[]);
void usage();
void sanitize_filename(char filename[], int last);
void read_dir(char dirname[], Queue_t *requests, char progname[]);

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
    extern int optopt;
    int opt, errsv;

    while ((opt = getopt(argc, argv, ":n:q:t:d:h")) != -1) {
        switch (opt) {
            case 'n':
                if ((isNumber(optarg, &nthread) != 0) || (nthread < 1)) {
                    fprintf(stderr, "%s: \x1B[1;31merror:\x1B[0m invalid option argument -- 'n'\n", argv[0]);
                    info(argv[0]);
                    exit(EXIT_FAILURE);
                }
                break;
            case 'q':
                if ((isNumber(optarg, &qlen) != 0) || (qlen < 1)) {
                    fprintf(stderr, "%s: \x1B[1;31merror:\x1B[0m invalid option argument -- 'q'\n", argv[0]);
                    info(argv[0]);
                    exit(EXIT_FAILURE);
                }
                break;
            case 't':
                if ((isNumber(optarg, &delay) != 0) || (delay < 0)) {
                    fprintf(stderr, "%s: \x1B[1;31merror:\x1B[0m invalid option argument -- 't'\n", argv[0]);
                    info(argv[0]);
                    exit(EXIT_FAILURE);
                }
                break;
            case 'd':
                sanitize_filename(optarg, 1);
                memset(&statbuf, 0, sizeof(statbuf));
                
                if (stat(optarg, &statbuf) == -1) {
                    errsv = errno;
                    fprintf(stderr, "%s: \x1B[1;31merror:\x1B[0m cannot accesss '%s': %s\n", argv[0], optarg, strerror(errsv));
                    exit(errsv);
                }

                if (!S_ISDIR(statbuf.st_mode)) {
                    fprintf(stderr, "%s: \x1B[1;31merror:\x1B[0m invalid option argument -- 'd'\n", argv[0]);
                    info(argv[0]);
                    exit(EXIT_FAILURE);
                }

                if (strlen(optarg) > PATHNAME_MAX)
                    fprintf(stderr, "%s: \x1B[1;33mwarning:\x1B[0m ignore '%s': File name too long (FILENAME_MAX = %d)\n", argv[0], optarg, PATHNAME_MAX);
                else
                    dirname = optarg;
                break;
            case 'h':
                usage();
                exit(EXIT_SUCCESS);
            case '?':
                fprintf(stderr, "%s: \x1B[1;31merror:\x1B[0m invalid option -- '%c'\n", argv[0], optopt);
                info(argv[0]);
                exit(EXIT_FAILURE);
            case ':':
                fprintf(stderr, "%s: \x1B[1;31merror:\x1B[0m option requires an argument -- '%c'\n", argv[0], optopt);
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
    CHECK_EXIT(argv[0], "initQueue()", (requests = initQueue()) == NULL, 1)

    while (optind != argc) {
        sanitize_filename(argv[optind], 0);
        memset(&statbuf, 0, sizeof(statbuf));

        if (stat(argv[optind], &statbuf) == -1) {
            errsv = errno;
            fprintf(stderr, "%s: \x1B[1;31merror:\x1B[0m cannot accesss '%s': %s\n", argv[0], argv[optind], strerror(errsv));
            deleteQueue(requests);
            exit(errsv);
        }
        
        if (!S_ISREG(statbuf.st_mode)) {
            fprintf(stderr, "%s: \x1B[1;31merror:\x1B[0m '%s': Not a regular file\n", argv[0], argv[optind]);
            info(argv[0]);
            deleteQueue(requests);
            exit(EXIT_FAILURE);
        }

        if (strlen(argv[optind]) > PATHNAME_MAX) {
            fprintf(stderr, "%s: \x1B[1;33mwarning:\x1B[0m ignore '%s': File name too long (FILENAME_MAX = %d)\n", argv[0], argv[optind], PATHNAME_MAX);
        } else {
            if (push(requests, argv[optind]) == -1) {
                errsv = errno;
                fprintf(stderr, "%s: \x1B[1;31merror:\x1B[0m push() '%s': %s\n", argv[0], argv[optind], strerror(errsv)); 
                deleteQueue(requests);
                exit(errsv);
            }
        }

        optind++;
    }

    if (dirname != NULL) read_dir(dirname, requests, argv[0]);

    deleteQueue(requests);
    
    exit(EXIT_SUCCESS);
}

void info(char pathname[]) {
    fprintf(stderr, "Try '\x1B[1;36m%s -h\x1B[0m' for more information.\n", pathname);
}

void usage() {
    fprintf(stderr, "Usage: \x1B[1mfarm\x1B[0m [OPTION]... [FILE]...\n");
    fprintf(stderr, "Read binary FILEs and return for each of them a calculation based on the numbers present in the FILEs.\n");
    fprintf(stderr, "The printing of the results is sorted in ascending order.\n\n");
    fprintf(stderr, "  \x1B[1m-h\x1B[0m\x1B[21Gdisplay this help and exit\n");
    fprintf(stderr, "  \x1B[1m-n\x1B[0m nthread\x1B[21Gnumber of Worker threads of the MasterWorker process (default value \x1B[1m4\x1B[0m)\n");
    fprintf(stderr, "  \x1B[1m-q\x1B[0m qlen\x1B[21Glength of the concurrent queue between the Master thread and the Worker threads (default value \x1B[1m8\x1B[0m)\n");
    fprintf(stderr, "  \x1B[1m-d\x1B[0m dirname\x1B[21Gcalculate the result for each binary FILE in the directory and subdirectories\n");
    fprintf(stderr, "  \x1B[1m-t\x1B[0m delay\x1B[21Gtime in milliseconds between the sending of two successive requests to\n\x1B[21Gthe Worker threads by the Master thread (default value \x1B[1m0\x1B[0m)\n\n");
    fprintf(stderr, "  \x1B[1mSignals:\x1B[0m\n\n");
    fprintf(stderr, "  Signal\x1B[44GAction\n");
    fprintf(stderr, "  ──────────────────────────────────────────────────────────────────────────────────────────────────\n");
    fprintf(stderr, "  \x1B[1mSIGUSR1\x1B[0m\x1B[44Gprint the results calculated up to that moment\n");
    fprintf(stderr, "  \x1B[1mSIGINT - SIGQUIT - SIGTERM - SIGHUP\x1B[0m\x1B[44Gcomplete any tasks in the queue and terminate the process\n");
}

void sanitize_filename(char filename[], int remove_last) {
    int i, j, f;

    for (i = 0, j = 0, f = 0; filename[i] != '\0'; i++) {
        if ((filename[i] != '/')) {
            filename[j++] = filename[i];
            f = 0;
        } else if ((filename[i] == '/')) {
            if (!f) {
                filename[j++] = filename[i]; 
                f = 1;
            }
        }
    }

    if (remove_last && (filename[j-1] == '/')) filename[j-1] = '\0';
    
    filename[j] = '\0';
}

void read_dir(char dirname[], Queue_t *requests, char progname[]) {
    DIR *dirp;

    if((dirp = opendir(dirname)) == NULL) {
        int errsv = errno;
        fprintf(stderr, "%s: \x1B[1;31merror:\x1B[0m opendir() '%s': %s\n", progname, dirname, strerror(errsv)); 
        deleteQueue(requests);
        exit(errsv);
    }

	struct dirent *entry;
    struct stat statbuf;
    char filename[PATHNAME_MAX + 1];
    int len_dirname, len_filename;
    
	while((errno = 0, entry = readdir(dirp)) != NULL) {
        len_dirname = strlen(dirname);
        len_filename = strlen(entry->d_name);

	    if((len_dirname + len_filename + 1) > PATHNAME_MAX) {
            fprintf(stderr, "%s: \x1B[1;33mwarning:\x1B[0m ignore '%s/%s': File name too long (FILENAME_MAX = %d)\n", progname, dirname, entry->d_name, PATHNAME_MAX);
            continue;
	    }
	    		    
	    strncpy(filename, dirname, PATHNAME_MAX + 1);
	    strncat(filename, "/", PATHNAME_MAX - len_dirname);
	    strncat(filename, entry->d_name, PATHNAME_MAX - len_dirname - 1);

        memset(&statbuf, 0, sizeof(statbuf));
        
        if (stat(filename, &statbuf) == -1) {
            int errsv = errno;
            fprintf(stderr, "%s: \x1B[1;31merror:\x1B[0m stat() '%s': %s\n", progname, filename, strerror(errsv));
            deleteQueue(requests);
            exit(errsv);
        }

        if(S_ISDIR(statbuf.st_mode)) {
            if ((strcmp(".", entry->d_name) != 0) && (strcmp("..", entry->d_name) != 0)) {
                read_dir(filename, requests, progname);
            }
	    } else {
            if(!S_ISREG(statbuf.st_mode)) {
                fprintf(stderr, "%s: \x1B[1;33mwarning:\x1B[0m ignore '%s': Not a regular file\n", progname, filename);
            } else {
                if (push(requests, filename) == -1) {
                    int errsv = errno;
                    fprintf(stderr, "%s: \x1B[1;31merror:\x1B[0m push() '%s': %s\n", progname, filename, strerror(errsv)); 
                    deleteQueue(requests);
                    exit(errsv);
                }
            }
        }
    }
    
    if(errno != 0) {
        int errsv = errno;
        fprintf(stderr, "%s: \x1B[1;31merror:\x1B[0m readdir() '%s': %s\n", progname, dirname, strerror(errsv)); 
        deleteQueue(requests);
        exit(errsv);
    }

	if (closedir(dirp) == -1) {
        int errsv = errno;
        fprintf(stderr, "%s: \x1B[1;31merror:\x1B[0m closedir() '%s': %s\n", progname, dirname, strerror(errsv)); 
        deleteQueue(requests);
        exit(errsv);
    }
}