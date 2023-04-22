#include <utils.h>
#include <unistd.h>

/*  Read 'n' byte from file descriptor 'fd' into the buffer starting at 'buf'.
 *
 *  RETURN VALUE: n on success
 *                0 indicates end of file
 *                -1 on error (errno is set)
 */
int readn(int fd, void *buf, size_t n) {
    char *buf_char = (char*) buf;
    size_t n_left = n;
    ssize_t n_read;

    while(n_left > 0) {
        if ((n_read = read(fd, buf_char, n_left)) == -1) {
            if (errno == EINTR)
                continue;
	        else
                // Error
                return -1;
	    }

        // EOF
	    if (n_read == 0) return 0;
        
        n_left -= n_read;
	    buf_char += n_read;
    }

    // Success
    return n;
}

/*  Write 'n' byte from the buffer starting at 'buf' to file descriptor 'fd'.
 *
 *  RETURN VALUE: n on success
 *                -1 on error (errno is set)
 */
int writen(int fd, void *buf, size_t n) {
    char *buf_char = (char*) buf;
    size_t n_left = n;
    ssize_t n_write;

    while(n_left > 0) {
	    if ((n_write = write(fd, buf_char, n_left)) == -1) {
	        if (errno == EINTR)
                continue;
	        else
                // Error
                return -1;
	    }
        
        n_left -= n_write;
	    buf_char += n_write;
    }

    // Success
    return n;
}