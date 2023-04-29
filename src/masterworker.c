#define _GNU_SOURCE // getopt()

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/wait.h>
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

// Socket of Master process used to accept the connection request of the Collector process
static int sfd;

// Connection socket with the Collector process
static int cfd;

// Termination flag
static volatile sig_atomic_t sigexit = 0;

/*  Check if the string 's' is a number and store it in 'n'.
 *
 *  RETURN VALUE: 0 on success
 *                1 not a number
 *                2 on overflow/underflow  
 */
static int isNumber(const char *s, long *n);

// Print info message
static void info(char pathname[]);

// Print help message
static void usage();

// Function registered using atexit()
static void cleanup();

// Remove extra '/' characters from 'filename'
static void sanitize_filename(char filename[], int last);

// Search recursively inside 'dirname' directory, valid files, and put them in the 'requests' queue
static void read_dir(char dirname[], Queue_t *requests, char progname[]);

// Signal handler established for signal SIGHUP, SIGINT, SIGQUIT, SIGTERM
static void sigexit_handler(int signo);

// Signal handler thread, send print command to Collector process when signal SIGUSR1 is caught
static void *sigprint_handler_thread(void *arg);

int main(int argc, char *argv[]) {
    // Block all the signals that the program handles
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

    // Ignore SIGPIPE
    struct sigaction sa;
    
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = SIG_IGN;

    SYSCALL_EXIT(argv[0], "sigaction() 'SIGPIPE'", sigaction(SIGPIPE, &sa, NULL))

    // Install signal handler for signal SIGHUP, SIGINT, SIGQUIT, SIGTERM
    memset(&sa, 0, sizeof(sa));   
    sa.sa_handler = sigexit_handler;
        
    SYSCALL_EXIT(argv[0], "sigaction() 'SIGHUP'", sigaction(SIGHUP, &sa, NULL))
    SYSCALL_EXIT(argv[0], "sigaction() 'SIGINT'", sigaction(SIGINT, &sa, NULL))
    SYSCALL_EXIT(argv[0], "sigaction() 'SIGQUIT'", sigaction(SIGQUIT, &sa, NULL))
    SYSCALL_EXIT(argv[0], "sigaction() 'SIGTERM'", sigaction(SIGTERM, &sa, NULL))

    // Create a new UNIX socket
    SYSCALL_EXIT(argv[0], "socket()", sfd = socket(AF_UNIX, SOCK_STREAM, 0))

    // Assign the address specified by 'SOCK_PATHNAME' to the socket 'sfd'
    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, SOCK_PATHNAME, sizeof(addr.sun_path) - 1);
        
    if (bind(sfd, (struct sockaddr*) &addr, sizeof(addr)) == -1) {
        int errsv = errno;
        fprintf(stderr, "%s: \x1B[1;31merror:\x1B[0m bind(): %s\n", argv[0], strerror(errsv)); 
        close(sfd);
        exit(errsv);
    }

    // Accept incoming connection requests
    if (listen(sfd, SOMAXCONN) == -1) {
        int errsv = errno;
        fprintf(stderr, "%s: \x1B[1;31merror:\x1B[0m listen(): %s\n", argv[0], strerror(errsv));
        unlink(SOCK_PATHNAME);
        close(sfd);
        exit(errsv);
    }

    // Create Collector process
    pid_t cpid;

    if ((cpid = fork()) == -1) {
        int errsv = errno;
        fprintf(stderr, "%s: \x1B[1;31merror:\x1B[0m fork(): %s\n", argv[0], strerror(errsv));
        unlink(SOCK_PATHNAME); 
        close(sfd);
        exit(errsv);
    }

    if (cpid == 0) {
        // Collector process
        
        // Close socket of Master process
        close(sfd);

        // Exec main function
        exec_collector();
    } else {
        // Master process

        // Wait for connection with Collector process
        if ((cfd = accept(sfd, NULL, NULL)) == -1) {
            int errsv = errno;
            fprintf(stderr, "%s: \x1B[1;31merror:\x1B[0m accept(): %s\n", argv[0], strerror(errsv)); 
            close(sfd);
            exit(errsv);
        }

        // Register the cleanup function
        if (atexit(cleanup) != 0) {
            fprintf(stderr, "%s: \x1B[1;31merror:\x1B[0m atexit()\n", argv[0]);
            unlink(SOCK_PATHNAME);
            close(cfd);
            close(sfd);
            exit(EXIT_FAILURE);
        }

        // Init variables
        long pool_size = POOL_SIZE, queue_size = QUEUE_SIZE, delay = DELAY;
        char *dirname = NULL;

        // Check if there are no arguments
        if (argc == 1) {
            info(argv[0]);
            exit(EXIT_FAILURE);
        }

        // Parsing command-line option arguments
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
                    // Remove extra '/' characters
                    sanitize_filename(optarg, 1);
                    
                    // Get information about 'd' argument
                    memset(&statbuf, 0, sizeof(statbuf));
                    
                    if (stat(optarg, &statbuf) == -1) {
                        errsv = errno;
                        fprintf(stderr, "%s: \x1B[1;31merror:\x1B[0m cannot accesss '%s': %s\n", argv[0], optarg, strerror(errsv));
                        exit(errsv);
                    }

                    // Check if 'd' argument is a directory
                    if (!S_ISDIR(statbuf.st_mode)) {
                        fprintf(stderr, "%s: \x1B[1;31merror:\x1B[0m '%s': Not a directory\n", argv[0], optarg);
                        info(argv[0]);
                        exit(ENOTDIR);
                    }

                    // Check if pathname is too long
                    if (strlen(optarg) > PATHNAME_MAX)
                        fprintf(stderr, "%s: \x1B[1;33mwarning:\x1B[0m ignore '%s': File name too long (PATHNAME_MAX = %d)\n", argv[0], optarg, PATHNAME_MAX);
                    else
                        dirname = optarg;
                    break;
                case 'h':
                    // Print help message
                    usage();
                    exit(EXIT_FAILURE);
                case '?':
                    // Invalid option
                    fprintf(stderr, "%s: \x1B[1;31merror:\x1B[0m invalid option -- '%c'\n", argv[0], optopt);
                    info(argv[0]);
                    exit(EXIT_FAILURE);
                case ':':
                    // Missing option argument
                    fprintf(stderr, "%s: \x1B[1;31merror:\x1B[0m option requires an argument -- '%c'\n", argv[0], optopt);
                    info(argv[0]);
                    exit(EXIT_FAILURE);
            }
        }

        // Check if there are no files
        extern int optind;

        if (dirname == NULL && optind == argc) {
            info(argv[0]);
            exit(EXIT_FAILURE);
        }

        // Create queue where storing all filenames
        Queue_t *requests;

        if ((requests = initQueue()) == NULL) {
            errsv = errno;
            fprintf(stderr, "%s: \x1B[1;31merror:\x1B[0m initQueue(): %s\n", argv[0], strerror(errsv));
            exit(errsv);
        }

        // Parsing command-line filename arguments
        while (optind != argc) {
            // Remove extra '/' characters
            sanitize_filename(argv[optind], 0);

            // Get information about filename argument
            memset(&statbuf, 0, sizeof(statbuf));

            if (stat(argv[optind], &statbuf) == -1) {
                errsv = errno;
                fprintf(stderr, "%s: \x1B[1;31merror:\x1B[0m cannot accesss '%s': %s\n", argv[0], argv[optind], strerror(errsv));
                deleteQueue(requests);
                exit(errsv);
            }
            
            // Check if filename is a regular file
            if (!S_ISREG(statbuf.st_mode)) {
                fprintf(stderr, "%s: \x1B[1;33mwarning:\x1B[0m ignore '%s': Not a regular file\n", argv[0], argv[optind]);
                optind++;
                continue;
            }

            // Check if filename format is invalid 
            if((statbuf.st_size % sizeof(long)) != 0) {
                fprintf(stderr, "%s: \x1B[1;33mwarning:\x1B[0m ignore '%s': Invalid format\n", argv[0], argv[optind]);
                optind++;
                continue;
            }

            // Check if pathname is too long
            if (strlen(argv[optind]) > PATHNAME_MAX) {
                fprintf(stderr, "%s: \x1B[1;33mwarning:\x1B[0m ignore '%s': File name too long (PATHNAME_MAX = %d)\n", argv[0], argv[optind], PATHNAME_MAX);
                optind++;
                continue;
            }

            // Insert filename in the 'requests' queue
            if (pushQueue(requests, argv[optind]) == -1) {
                errsv = errno;
                fprintf(stderr, "%s: \x1B[1;31merror:\x1B[0m pushQueue() '%s': %s\n", argv[0], argv[optind], strerror(errsv)); 
                deleteQueue(requests);
                exit(errsv);
            }

            optind++;
        }

        // Search inside 'dirname' directory, valid files, and put them in the 'requests' queue
        if (dirname != NULL) read_dir(dirname, requests, argv[0]);

        // Check if there are no files
        int n_results;

        if ((n_results = lengthQueue(requests)) == 0) {
            info(argv[0]);
            deleteQueue(requests);
            exit(EXIT_FAILURE);
        }

        // Send the number of filenames to Collector process
        if (writen(cfd, &n_results, sizeof(int)) == -1) {
            errsv = errno;
            fprintf(stderr, "%s: \x1B[1;31merror:\x1B[0m writen() 'n_results': %s\n", argv[0], strerror(errsv)); 
            deleteQueue(requests);
            exit(errsv);
        }

        // Init 'collector_fd_mutex' to ensure thread safety on 'cfd' socket
        pthread_mutex_t collector_fd_mutex;

        if ((error_number = pthread_mutex_init(&collector_fd_mutex, NULL)) != 0) {
            fprintf(stderr, "%s: \x1B[1;31merror:\x1B[0m pthread_mutex_init(): %s\n", argv[0], strerror(error_number)); 
            deleteQueue(requests);
            exit(error_number);
        }

        // Create thread pool
        Threadpool_t *pool;
        
        if ((pool = initThreadPool(pool_size, queue_size, cfd, &collector_fd_mutex)) == NULL) {
            errsv = errno;
            fprintf(stderr, "%s: \x1B[1;31merror:\x1B[0m initThreadPool(): %s\n", argv[0], strerror(errsv)); 
            deleteQueue(requests);
            exit(errsv);
        }

        // Spawn signal handler thread
        pthread_t sigprint_handler_tid;

        if((error_number = pthread_create(&sigprint_handler_tid, NULL, sigprint_handler_thread, &collector_fd_mutex)) != 0) {
            errsv = errno;
            fprintf(stderr, "%s: \x1B[1;31merror:\x1B[0m pthread_create() 'sigprint_handler_thread': %s\n", argv[0], strerror(errsv)); 
            deleteQueue(requests);
            shutdownThreadPool(pool);
            exit(errsv);
        }

        // Unblock signal SIGHUP, SIGINT, SIGQUIT, SIGTERM
        // Signal SIGUSR1 remains blocked
        sigdelset(&mask, SIGUSR1);

        if ((error_number = pthread_sigmask(SIG_UNBLOCK , &mask, NULL)) != 0) {
            fprintf(stderr, "%s: \x1B[1;31merror:\x1B[0m pthread_sigmask(): %s\n", argv[0], strerror(error_number));
            deleteQueue(requests);
            shutdownThreadPool(pool);
            exit(error_number);
        }

        // Set timer of 'delay' ms 
        struct timespec base_time_request, time_request, time_remaining;

        if (delay) {
            base_time_request.tv_sec = delay / 1000;
            base_time_request.tv_nsec = (delay % 1000) * 1000000;
            time_request = base_time_request;
        }

        // Submit 'filename' to thread pool every 'delay' ms
        char *filename;

        while (!sigexit) {
            // Pop 'filename' from the queue 
            if ((errno = 0, filename = popQueue(requests)) != NULL) {
                // Submit 'filename' to thread pool
                if (submitThreadPool(pool, filename) == -1) {
                    errsv = errno;
                    fprintf(stderr, "%s: \x1B[1;31merror:\x1B[0m executeThreadPool(): %s\n", argv[0], strerror(errsv)); 
                    deleteQueue(requests);
                    shutdownThreadPool(pool);
                    exit(errsv);
                }
            } else if (errno == 0) {
                // Queue is empty
                break;
            } else {
                errsv = errno;
                fprintf(stderr, "%s: \x1B[1;31merror:\x1B[0m popQueue(): %s\n", argv[0], strerror(errsv)); 
                deleteQueue(requests);
                shutdownThreadPool(pool);
                exit(errsv);
            }

            // Suspend execution for 'delay' ms
            if (delay) {
                while (nanosleep(&time_request, &time_remaining) != 0) {
                    // Check if it has been interrupted by a signal handler
                    if (errno == EINTR) {
                        if (sigexit) {
                            // Exit to start termination protocol
                            break;
                        } else {
                            // Suspend execution for the remaining time
                            time_request = time_remaining;
                        } 
                    } else {
                        errsv = errno;
                        fprintf(stderr, "%s: \x1B[1;31merror:\x1B[0m nanosleep(): %s\n", argv[0], strerror(errsv)); 
                        deleteQueue(requests);
                        shutdownThreadPool(pool);
                        exit(errsv);
                    }
                }

                // Set correct 'time_request'
                time_request = base_time_request;
            }
        }

        // Delete 'requests' queue
        deleteQueue(requests);
        
        // Initiate an orderly shutdown of the thread pool in which previously submitted tasks are executed
        SYSCALL_EXIT(argv[0], "shutdownThreadPool()", shutdownThreadPool(pool))

        // Wait termination of signal handler thread
        sigexit = 1;

        if ((error_number = pthread_join(sigprint_handler_tid, NULL)) != 0) {
            fprintf(stderr, "%s: \x1B[1;31merror:\x1B[0m pthread_join() 'sigprint_handler_thread': %s\n", argv[0], strerror(error_number));
            exit(error_number);
        }

        // Destroy 'collector_fd_mutex'
        if ((error_number = pthread_mutex_destroy(&collector_fd_mutex)) != 0) {
            fprintf(stderr, "%s: \x1B[1;31merror:\x1B[0m pthread_mutex_destroy(): %s\n", argv[0], strerror(error_number));
            exit(error_number);
        }

        // Send 0 (exit) to Collector process
        int opcode = 0;
        
        SYSCALL_EXIT(argv[0], "writen() 'opcode: 0 (exit)'", writen(cfd, &opcode, sizeof(int)))

        // Wait termination of Collector process
        int wstatus;

        while (waitpid(cpid, &wstatus, 0) == -1) {
            // Check if it has been interrupted by a signal handler
            if (errno != EINTR) {
                errsv = errno;
                fprintf(stderr, "%s: \x1B[1;31merror:\x1B[0m waitpid(): %s\n", argv[0], strerror(errsv)); 
                exit(errsv);
            }
        }

        // Check the exit code
        if (WIFEXITED(wstatus)) {
            if (WEXITSTATUS(wstatus) != EXIT_SUCCESS) {
                fprintf(stderr, "%s: \x1B[1;31merror:\x1B[0m collector: Exited with code (%d)\n", argv[0], WEXITSTATUS(wstatus)); 
                exit(EXIT_FAILURE);
            }
        } else {
            // Check if Collector process was killed by a signal
            if (WIFSIGNALED(wstatus)) {
                fprintf(stderr, "%s: \x1B[1;31merror:\x1B[0m collector: Killed by signal (%d)\n", argv[0], WTERMSIG(wstatus)); 
                exit(EXIT_FAILURE);
            }
        }

        // Success
        exit(EXIT_SUCCESS);
    }
}

// Print info message
static void info(char pathname[]) {
    fprintf(stderr, "%s: Try '\x1B[1;36m%s -h\x1B[0m' for more information\n", pathname, pathname);
}

// Print help message
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

/*  Check if the string 's' is a number and store it in 'n'.
 *
 *  RETURN VALUE: 0 on success
 *                1 not a number
 *                2 on overflow/underflow  
 */
static int isNumber(const char *s, long *n) {
    // Not a number
    if (s == NULL) return 1;
    if (strlen(s) == 0) return 1;

    char *e = NULL;
    errno = 0;
    long val = strtol(s, &e, 10);

    if (errno != 0) {
        // Overflow/underflow
        if (errno == ERANGE) return 2;

        // Not a number
        return 1;
    }
    
    if (e != NULL && *e == '\0') {
        *n = val;

        // Success
        return 0;
    }

    // Not a number
    return 1;
}

// Remove extra '/' characters from 'filename'
static void sanitize_filename(char filename[], int remove_last) {
    // Find multiple '/' characters and raplace them with only one  
    int final_index = 0;

    for (int i = 0, flag = 0; filename[i] != '\0'; i++) {
        if ((filename[i] != '/')) {
            filename[final_index++] = filename[i];
            flag = 0;
        } else if ((filename[i] == '/')) {
            if (!flag) {
                filename[final_index++] = filename[i]; 
                flag = 1;
            }
        }
    }

    // If 'remove_last' is set remove the final '/' character from 'filename'
    if (remove_last && (filename[final_index-1] == '/'))
        filename[final_index-1] = '\0';
    else 
        filename[final_index] = '\0';
}

// Search recursively inside 'dirname' directory, valid files, and put them in the 'requests' queue
static void read_dir(char dirname[], Queue_t *requests, char progname[]) {
    // Open 'dirname' directory
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
    
    // Read file in 'dirname'
	while((errno = 0, entry = readdir(dirp)) != NULL) {
        // Resolve relative path
        len_dirname = strlen(dirname);
        len_filename = strlen(entry->d_name);

	    if((len_dirname + len_filename + 1) > PATHNAME_MAX) {
            fprintf(stderr, "%s: \x1B[1;33mwarning:\x1B[0m ignore '%s/%s': File name too long (PATHNAME_MAX = %d)\n", progname, dirname, entry->d_name, PATHNAME_MAX);
            continue;
	    }
	    		    
	    strncpy(filename, dirname, PATHNAME_MAX + 1);
	    strncat(filename, "/", PATHNAME_MAX - len_dirname);
	    strncat(filename, entry->d_name, PATHNAME_MAX - len_dirname - 1);

        // Get information about 'filename'
        memset(&statbuf, 0, sizeof(statbuf));
        
        if (stat(filename, &statbuf) == -1) {
            int errsv = errno;
            fprintf(stderr, "%s: \x1B[1;31merror:\x1B[0m stat() '%s': %s\n", progname, filename, strerror(errsv));
            deleteQueue(requests);
            exit(errsv);
        }

        // If 'filename' is a directory, call recursively read_dir() 
        if(S_ISDIR(statbuf.st_mode)) {
            if ((strcmp(".", entry->d_name) != 0) && (strcmp("..", entry->d_name) != 0)) {
                read_dir(filename, requests, progname);
            }
	    } else {
            // Skip the file if it is not regular or if the format is invalid
            if(!S_ISREG(statbuf.st_mode)) {
                fprintf(stderr, "%s: \x1B[1;33mwarning:\x1B[0m ignore '%s': Not a regular file\n", progname, filename);
            } else if ((statbuf.st_size % sizeof(long)) != 0) {
                fprintf(stderr, "%s: \x1B[1;33mwarning:\x1B[0m ignore '%s': Invalid format\n", progname, filename);
            } else {
                // Insert 'filename' in the 'requests' queue
                if (pushQueue(requests, filename) == -1) {
                    int errsv = errno;
                    fprintf(stderr, "%s: \x1B[1;31merror:\x1B[0m pushQueue() '%s': %s\n", progname, filename, strerror(errsv)); 
                    deleteQueue(requests);
                    exit(errsv);
                }
            }
        }
    }
    
    // Check if an error occurs in readdir()
    if(errno != 0) {
        int errsv = errno;
        fprintf(stderr, "%s: \x1B[1;31merror:\x1B[0m readdir() '%s': %s\n", progname, dirname, strerror(errsv)); 
        deleteQueue(requests);
        exit(errsv);
    }

    // Close 'dirname' directory
	if (closedir(dirp) == -1) {
        int errsv = errno;
        fprintf(stderr, "%s: \x1B[1;31merror:\x1B[0m closedir() '%s': %s\n", progname, dirname, strerror(errsv)); 
        deleteQueue(requests);
        exit(errsv);
    }
}

// Function registered using atexit()
static void cleanup() {
    unlink(SOCK_PATHNAME);
    close(cfd);
    close(sfd);
}

// Signal handler established for signal SIGHUP, SIGINT, SIGQUIT, SIGTERM
static void sigexit_handler(int signo) {
    switch (signo) {
        case SIGHUP:
        case SIGINT:
        case SIGQUIT:
        case SIGTERM:
            // Set flag to start the termination process
            sigexit = 1;
    }
}

// Signal handler thread, send print command to Collector process when signal SIGUSR1 is caught
static void *sigprint_handler_thread(void *arg) {
    // Check arg
    if (arg == NULL) {
        fprintf(stderr, "sigprint_handler_thread: \x1B[1;31merror:\x1B[0m Invalid argument\n");
        exit(EINVAL);
    }

    pthread_mutex_t *collector_fd_mutex = (pthread_mutex_t*) arg;

    // Set signal SIGUSR1 
    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, SIGUSR1);

    // Set a timeout of 1 ms
    struct timespec timeout;
    timeout.tv_sec = 0;
    timeout.tv_nsec = 1000000;

    int opcode = -1, tid = 0, error_number;

    while(!sigexit) {
        // Suspend execution until the signal in set is pending
        if (sigtimedwait(&set, NULL, &timeout) == -1) {
            // Check if timeout has expired or if it has been interrupted by a signal handler
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

            // Send -1 (print) to Collector process
            if (writen(cfd, &opcode, sizeof(int)) == -1) {
                int errsv = errno;
                fprintf(stderr, "thread[%d]: \x1B[1;31merror:\x1B[0m writen() 'opcode: -1 (print)': ", tid); 
                errno = errsv;
                perror(NULL);
                exit(errsv);
            }

            UNLOCK_EXIT(collector_fd_mutex, error_number, tid)
        }
    }

    // Success
    pthread_exit(NULL);
}