#include <utils.h>
#include <unistd.h>

int readn(int fd, void *buf, size_t n) {
    char *buf_char = (char*) buf;
    size_t n_left = n;
    ssize_t n_read;

    while(n_left > 0) {
        if ((n_read = read(fd, buf_char, n_left)) == -1) {
            if (errno == EINTR)
                continue;
	        else
                return -1;
	    }

	    if (n_read == 0) return 0; // EOF
        
        n_left -= n_read;
	    buf_char += n_read;
    }

    return n;
}

int writen(int fd, void *buf, size_t n) {
    char *buf_char = (char*) buf;
    size_t n_left = n;
    ssize_t n_write;

    while(n_left > 0) {
	    if ((n_write = write(fd, buf_char, n_left)) == -1) {
	        if (errno == EINTR)
                continue;
	        else
                return -1;
	    }
        
        n_left -= n_write;
	    buf_char += n_write;
    }

    return n;
}