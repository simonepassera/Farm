#define _POSIX_C_SOURCE 200809L

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <dirent.h>
#include <utils.h>
#include <queue.h>
#include <threadpool.h>

void info(char pathname[]);
void usage();

/*  Check if the string 's' is a number and store in 'n'.
 *
 *  RETURN VALUE: 0 on success
 *                1 is not a number
 *                2 on overflow/underflow  
 */
int isNumber(const char *s, long *n);

void sanitize_filename(char filename[], int last);
void read_dir(char dirname[], Queue_t *requests, char progname[]);
void cleanup();

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
            fprintf(stderr, "%s: \x1B[1;33mwarning:\x1B[0m ignore '%s': Not a regular file\n", argv[0], argv[optind]);
            optind++;
            continue;
        }

        if((statbuf.st_size % sizeof(long)) != 0) {
            fprintf(stderr, "%s: \x1B[1;33mwarning:\x1B[0m ignore '%s': Invalid format\n", argv[0], argv[optind]);
            optind++;
            continue;
        }

        if (strlen(argv[optind]) > PATHNAME_MAX) {
            fprintf(stderr, "%s: \x1B[1;33mwarning:\x1B[0m ignore '%s': File name too long (FILENAME_MAX = %d)\n", argv[0], argv[optind], PATHNAME_MAX);
            optind++;
            continue;
        }

        if (pushQueue(requests, argv[optind]) == -1) {
            errsv = errno;
            fprintf(stderr, "%s: \x1B[1;31merror:\x1B[0m pushQueue() '%s': %s\n", argv[0], argv[optind], strerror(errsv)); 
            deleteQueue(requests);
            exit(errsv);
        }

        optind++;
    }

    if (dirname != NULL) read_dir(dirname, requests, argv[0]);

    if (length(requests) == 0) {
        info(argv[0]);
        deleteQueue(requests);
        exit(EXIT_SUCCESS);
    }

    int sfd;

    if ((sfd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
        errsv = errno;
        fprintf(stderr, "%s: \x1B[1;31merror:\x1B[0m socket(): %s\n", argv[0], strerror(errsv)); 
        deleteQueue(requests);
        exit(errsv);
    }
        
    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, SOCK_PATH, sizeof(addr.sun_path) - 1);
        
    if (bind(sfd, (struct sockaddr*) &addr, sizeof(addr)) == -1) {
        errsv = errno;
        fprintf(stderr, "%s: \x1B[1;31merror:\x1B[0m bind(): %s\n", argv[0], strerror(errsv)); 
        deleteQueue(requests);
        close(sfd);
        exit(errsv);
    }

    if (atexit(cleanup) != 0) {
        fprintf(stderr, "%s: \x1B[1;31merror:\x1B[0m atexit()\n", argv[0]); 
        deleteQueue(requests);
        close(sfd);
        exit(EXIT_FAILURE);
    }

    if (listen(sfd, SOMAXCONN) == -1) {
        errsv = errno;
        fprintf(stderr, "%s: \x1B[1;31merror:\x1B[0m listen(): %s\n", argv[0], strerror(errsv)); 
        deleteQueue(requests);
        close(sfd);
        exit(errsv);
    }

    pid_t cpid;

    if ((cpid = fork()) == -1) {
        errsv = errno;
        fprintf(stderr, "%s: \x1B[1;31merror:\x1B[0m fork(): %s\n", argv[0], strerror(errsv)); 
        deleteQueue(requests);
        close(sfd);
        exit(errsv);
    }

    if (cpid == 0) {
        execl("./collector", "./collector", (char*) NULL);

        errsv = errno;
        fprintf(stderr, "%s: \x1B[1;31merror:\x1B[0m exec() './collector': %s \x1B[1;36m(press Ctrl-C)\x1B[0m\n", argv[0], strerror(errsv)); 
        deleteQueue(requests);
        close(sfd);
        _exit(errno);
    }

    int cfd;

    if ((cfd = accept(sfd, NULL, NULL)) == -1) {
        errsv = errno;
        fprintf(stderr, "%s: \x1B[1;31merror:\x1B[0m accept(): %s\n", argv[0], strerror(errsv)); 
        deleteQueue(requests);
        close(sfd);
        exit(errsv);
    } 

    Threadpool_t *pool = initThreadPool(nthread, qlen, cfd);

    char *filename;
    
    while ((filename = popQueue(requests)) != NULL) {
        executeThreadPool(pool, filename);
    }

    shutdownThreadPool(pool);

    exit(EXIT_SUCCESS);
}

void info(char pathname[]) {
    fprintf(stderr, "%s: Try '\x1B[1;36m%s -h\x1B[0m' for more information\n", pathname, pathname);
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

int isNumber(const char *s, long *n) {
    if (s == NULL) return 1;
    if (strlen(s) == 0) return 1;

    char *e = NULL;
    errno = 0;

    long val = strtol(s, &e, 10);

    if (errno == ERANGE) return 2; // overflow/underflow

    if (e != NULL && *e == '\0') {
        *n = val;
        return 0; // success
    }

    return 1; // not a number
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
            } else if ((statbuf.st_size % sizeof(long)) != 0) {
                fprintf(stderr, "%s: \x1B[1;33mwarning:\x1B[0m ignore '%s': Invalid format\n", progname, filename);
            } else {
                if (pushQueue(requests, filename) == -1) {
                    int errsv = errno;
                    fprintf(stderr, "%s: \x1B[1;31merror:\x1B[0m pushQueue() '%s': %s\n", progname, filename, strerror(errsv)); 
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

void cleanup() {
    unlink(SOCK_PATH);
}