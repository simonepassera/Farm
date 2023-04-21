#define _GNU_SOURCE

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
#include <signal.h>
#include <time.h>
#include <utils.h>
#include <queue.h>
#include <threadpool.h>
#include <collector.h>

// Default options
#define POOL_SIZE 4
#define QUEUE_SIZE 8
#define DELAY 0
#define PATHNAME_MAX 255

static int sfd;
static int cfd;

static volatile sig_atomic_t sigexit = 0;

static void info(char pathname[]);
static void usage();

/*  Check if the string 's' is a number and store in 'n'.
 *
 *  RETURN VALUE: 0 on success
 *                1 is not a number
 *                2 on overflow/underflow  
 */
static int isNumber(const char *s, long *n);

static void sanitize_filename(char filename[], int last);
static void read_dir(char dirname[], Queue_t *requests, char progname[]);
static void cleanup();
static void sigexit_handler(int signo);
static void *sigprint_handler_thread(void *arg);

int main(int argc, char *argv[]) {
    sigset_t mask;
    
    sigemptyset(&mask);
    sigaddset(&mask, SIGHUP);
    sigaddset(&mask, SIGINT); 
    sigaddset(&mask, SIGQUIT);
    sigaddset(&mask, SIGTERM);
    sigaddset(&mask, SIGUSR1);

    int error_number;
    
    if ((error_number = pthread_sigmask(SIG_BLOCK, &mask, NULL)) != 0) {
        fprintf(stderr, "%s: \x1B[1;31merror:\x1B[0m pthread_sigmask(): %s\n", argv[0], strerror(error_number));
        exit(error_number);
    }

    struct sigaction sa;

    memset(&sa, 0, sizeof(sa));   
    sa.sa_handler = SIG_IGN;

    SYSCALL_EXIT(argv[0], "sigaction 'SIGPIPE'", sigaction(SIGPIPE, &sa, NULL))

    if ((sfd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
        int errsv = errno;
        fprintf(stderr, "%s: \x1B[1;31merror:\x1B[0m socket(): %s\n", argv[0], strerror(errsv)); 
        exit(errsv);
    }

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, SOCK_PATH, sizeof(addr.sun_path) - 1);
        
    if (bind(sfd, (struct sockaddr*) &addr, sizeof(addr)) == -1) {
        int errsv = errno;
        fprintf(stderr, "%s: \x1B[1;31merror:\x1B[0m bind(): %s\n", argv[0], strerror(errsv)); 
        close(sfd);
        exit(errsv);
    }

    if (listen(sfd, SOMAXCONN) == -1) {
        int errsv = errno;
        fprintf(stderr, "%s: \x1B[1;31merror:\x1B[0m listen(): %s\n", argv[0], strerror(errsv));
        unlink(SOCK_PATH);
        close(sfd);
        exit(errsv);
    }

    pid_t cpid;

    if ((cpid = fork()) == -1) {
        int errsv = errno;
        fprintf(stderr, "%s: \x1B[1;31merror:\x1B[0m fork(): %s\n", argv[0], strerror(errsv));
        unlink(SOCK_PATH); 
        close(sfd);
        exit(errsv);
    }

    if (cpid == 0) {
        close(sfd);
        exec_collector();
    } else {
        if ((cfd = accept(sfd, NULL, NULL)) == -1) {
            int errsv = errno;
            fprintf(stderr, "%s: \x1B[1;31merror:\x1B[0m accept(): %s\n", argv[0], strerror(errsv)); 
            close(sfd);
            exit(errsv);
        }

        if (atexit(cleanup) != 0) {
            fprintf(stderr, "%s: \x1B[1;31merror:\x1B[0m atexit()\n", argv[0]);
            unlink(SOCK_PATH);
            close(cfd);
            close(sfd);
            exit(EXIT_FAILURE);
        }

        long pool_size = POOL_SIZE, queue_size = QUEUE_SIZE, delay = DELAY;
        char *dirname = NULL;

        if (argc == 1) {
            info(argv[0]);
            exit(EXIT_FAILURE);
        }

        extern char *optarg;
        extern int optopt;
        
        int opt, errsv;
        struct stat statbuf;

        while ((opt = getopt(argc, argv, ":n:q:t:d:h")) != -1) {
            switch (opt) {
                case 'n':
                    if ((isNumber(optarg, &pool_size) != 0) || (pool_size < 1)) {
                        fprintf(stderr, "%s: \x1B[1;31merror:\x1B[0m invalid option argument -- 'n'\n", argv[0]);
                        info(argv[0]);
                        exit(EXIT_FAILURE);
                    }
                    break;
                case 'q':
                    if ((isNumber(optarg, &queue_size) != 0) || (queue_size < 1)) {
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
                        fprintf(stderr, "%s: \x1B[1;31merror:\x1B[0m invalid option argument -- 'd': Not a directory\n", argv[0]);
                        info(argv[0]);
                        exit(ENOTDIR);
                    }

                    if (strlen(optarg) > PATHNAME_MAX)
                        fprintf(stderr, "%s: \x1B[1;33mwarning:\x1B[0m ignore '%s': File name too long (FILENAME_MAX = %d)\n", argv[0], optarg, PATHNAME_MAX);
                    else
                        dirname = optarg;
                    break;
                case 'h':
                    usage();
                    exit(EXIT_FAILURE);
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

        if ((requests = initQueue()) == NULL) {
            errsv = errno;
            fprintf(stderr, "%s: \x1B[1;31merror:\x1B[0m initQueue(): %s\n", argv[0], strerror(errsv));
            exit(errsv);
        }

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

        int opcode;

        if ((opcode = lengthQueue(requests)) == 0) {
            info(argv[0]);
            deleteQueue(requests);
            exit(EXIT_FAILURE);
        }

        if (writen(cfd, &opcode, sizeof(int)) == -1) {
            errsv = errno;
            fprintf(stderr, "%s: \x1B[1;31merror:\x1B[0m writen() 'opcode: queue_length': %s\n", argv[0], strerror(errsv)); 
            deleteQueue(requests);
            exit(errsv);
        }

        pthread_mutex_t collector_fd_mutex;

        if ((error_number = pthread_mutex_init(&collector_fd_mutex, NULL)) != 0) {
            fprintf(stderr, "%s: \x1B[1;31merror:\x1B[0m pthread_mutex_init(): %s\n", argv[0], strerror(error_number)); 
            deleteQueue(requests);
            exit(error_number);
        }

        Threadpool_t *pool;
        
        if ((pool = initThreadPool(pool_size, queue_size, cfd, &collector_fd_mutex)) == NULL) {
            errsv = errno;
            fprintf(stderr, "%s: \x1B[1;31merror:\x1B[0m initThreadPool(): %s\n", argv[0], strerror(errsv)); 
            deleteQueue(requests);
            exit(errsv);
        }

        memset(&sa, 0, sizeof(sa));   
        sa.sa_handler = sigexit_handler;
        
        SYSCALL_EXIT(argv[0], "sigaction 'SIGHUP'", sigaction(SIGHUP, &sa, NULL))
        SYSCALL_EXIT(argv[0], "sigaction 'SIGINT'", sigaction(SIGINT, &sa, NULL))
        SYSCALL_EXIT(argv[0], "sigaction 'SIGQUIT'", sigaction(SIGQUIT, &sa, NULL))
        SYSCALL_EXIT(argv[0], "sigaction 'SIGTERM'", sigaction(SIGTERM, &sa, NULL))

        pthread_t sigprint_handler_tid;

        if((error_number = pthread_create(&sigprint_handler_tid, NULL, sigprint_handler_thread, &collector_fd_mutex)) != 0) {
            errsv = errno;
            fprintf(stderr, "%s: \x1B[1;31merror:\x1B[0m pthread_create() 'sigprint_handler_thread': %s\n", argv[0], strerror(errsv)); 
            deleteQueue(requests);
            shutdownThreadPool(pool);
            exit(errsv);
        }

        sigdelset(&mask, SIGUSR1);

        if ((error_number = pthread_sigmask(SIG_UNBLOCK , &mask, NULL)) != 0) {
            fprintf(stderr, "%s: \x1B[1;31merror:\x1B[0m pthread_sigmask(): %s\n", argv[0], strerror(error_number));
            deleteQueue(requests);
            shutdownThreadPool(pool);
            exit(error_number);
        }

        struct timespec base_request, request, remaining;

        if (delay) {
            base_request.tv_sec = delay / 1000;
            base_request.tv_nsec = (delay % 1000) * 1000000;
            request = base_request;
        }

        char *filename;

        while (!sigexit) {
            if ((errno = 0, filename = popQueue(requests)) != NULL) {
                if (executeThreadPool(pool, filename) == -1) {
                    errsv = errno;
                    fprintf(stderr, "%s: \x1B[1;31merror:\x1B[0m executeThreadPool(): %s\n", argv[0], strerror(errsv)); 
                    deleteQueue(requests);
                    shutdownThreadPool(pool);
                    exit(errsv);
                }
            } else if (errno == 0) {
                break;
            } else {
                errsv = errno;
                fprintf(stderr, "%s: \x1B[1;31merror:\x1B[0m popQueue(): %s\n", argv[0], strerror(errsv)); 
                deleteQueue(requests);
                shutdownThreadPool(pool);
                exit(errsv);
            }

            if (delay) {
                while (nanosleep(&request, &remaining) != 0) {
                    if (errno == EINTR) {
                        if (sigexit) {
                            break;
                        } else {
                            request = remaining;
                        } 
                    } else {
                        errsv = errno;
                        fprintf(stderr, "%s: \x1B[1;31merror:\x1B[0m nanosleep(): %s\n", argv[0], strerror(errsv)); 
                        deleteQueue(requests);
                        shutdownThreadPool(pool);
                        exit(errsv);
                    }
                }

                request = base_request;
            }
        }

        deleteQueue(requests);
        
        if (shutdownThreadPool(pool) ==  -1) {
            errsv = errno;
            fprintf(stderr, "%s: \x1B[1;31merror:\x1B[0m shutdownThreadPool(): %s\n", argv[0], strerror(errsv));
            exit(errsv);
        }

        sigexit = 1;

        if ((error_number = pthread_join(sigprint_handler_tid, NULL)) != 0) {
            fprintf(stderr, "%s: \x1B[1;31merror:\x1B[0m pthread_join() 'sigprint_handler_thread': %s\n", argv[0], strerror(error_number));
            exit(error_number);
        }

        if ((error_number = pthread_mutex_destroy(&collector_fd_mutex)) != 0) {
            fprintf(stderr, "%s: \x1B[1;31merror:\x1B[0m pthread_mutex_destroy(): %s\n", argv[0], strerror(error_number));
            exit(error_number);
        }

        opcode = -1;

        if (writen(cfd, &opcode, sizeof(int)) == -1) {
            errsv = errno;
            fprintf(stderr, "%s: \x1B[1;31merror:\x1B[0m writen() 'opcode: exit': %s\n", argv[0], strerror(errsv)); 
            exit(errsv);
        }

        exit(EXIT_SUCCESS);
    }
}

static void info(char pathname[]) {
    fprintf(stderr, "%s: Try '\x1B[1;36m%s -h\x1B[0m' for more information\n", pathname, pathname);
}

static void usage() {
    fprintf(stderr, "Usage: \x1B[1mfarm\x1B[0m [OPTION]... [FILE]...\n");
    fprintf(stderr, "Read binary FILEs and return for each of them a calculation based on the numbers present in the FILEs.\n");
    fprintf(stderr, "The printing of the results is sorted in ascending order.\n\n");
    fprintf(stderr, "  \x1B[1m-h\x1B[0m\x1B[21Gdisplay this help and exit\n");
    fprintf(stderr, "  \x1B[1m-n\x1B[0m \x1B[4mthreads\x1B[0m\x1B[21Gnumber of Worker threads (default value \x1B[1m4\x1B[0m)\n");
    fprintf(stderr, "  \x1B[1m-q\x1B[0m \x1B[4msize\x1B[0m\x1B[21Glength of the concurrent queue between the Master thread and the Worker threads (default value \x1B[1m8\x1B[0m)\n");
    fprintf(stderr, "  \x1B[1m-d\x1B[0m \x1B[4mdirname\x1B[0m\x1B[21Gcalculate the result for each binary FILE in the \x1B[4mdirname\x1B[0m directory and subdirectories\n");
    fprintf(stderr, "  \x1B[1m-t\x1B[0m \x1B[4mdelay\x1B[0m\x1B[21Gtime in milliseconds between the sending of two successive requests to\n\x1B[21Gthe Worker threads by the Master thread (default value \x1B[1m0\x1B[0m)\n\n");
    fprintf(stderr, "  \x1B[1mSignals:\x1B[0m\n\n");
    fprintf(stderr, "  Signal\x1B[44GAction\n");
    fprintf(stderr, "  ──────────────────────────────────────────────────────────────────────────────────────────────────\n");
    fprintf(stderr, "  \x1B[1mSIGUSR1\x1B[0m\x1B[44Gprint the results calculated up to that moment\n");
    fprintf(stderr, "  \x1B[1mSIGHUP - SIGINT - SIGQUIT - SIGTERM\x1B[0m\x1B[44Gcomplete any tasks in the queue and terminate the process\n");
}

static int isNumber(const char *s, long *n) {
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

static void sanitize_filename(char filename[], int remove_last) {
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

static void read_dir(char dirname[], Queue_t *requests, char progname[]) {
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

static void cleanup() {
    unlink(SOCK_PATH);
    close(cfd);
    close(sfd);
}

static void sigexit_handler(int signo) {
    switch (signo) {
        case SIGHUP:
        case SIGINT:
        case SIGQUIT:
        case SIGTERM:
            sigexit = 1;
    }
}

static void *sigprint_handler_thread(void *arg) {
    if (arg == NULL) {
        fprintf(stderr, "sigprint_handler_thread: \x1B[1;31merror:\x1B[0m Invalid argument\n");
        exit(EINVAL);
    }

    pthread_mutex_t *collector_fd_mutex = (pthread_mutex_t*) arg;

    sigset_t set;

    sigemptyset(&set);
    sigaddset(&set, SIGUSR1);

    struct timespec timeout;
    timeout.tv_sec = 0;
    timeout.tv_nsec = 500000000;

    int opcode = 0, tid = 0, error_number;

    while(!sigexit) {
        if (sigtimedwait(&set, NULL, &timeout) == -1) {
            if ((errno == EAGAIN) || (errno == EINTR)) {
                continue;
            } else {
                int errsv = errno;
                fprintf(stderr, "thread[%d]: \x1B[1;31merror:\x1B[0m sigtimedwait(): ", tid);
                errno = errsv;
                perror(NULL);
                exit(errsv);
            }
        } else {
            LOCK_EXIT(collector_fd_mutex, error_number, tid)

            if (writen(cfd, &opcode, sizeof(int)) == -1) {
                int errsv = errno;
                fprintf(stderr, "thread[%d]: \x1B[1;31merror:\x1B[0m writen() 'opcode: print': ", tid); 
                errno = errsv;
                perror(NULL);
                exit(errsv);
            }

            UNLOCK_EXIT(collector_fd_mutex, error_number, tid)
        }
    }

    pthread_exit(NULL);
}