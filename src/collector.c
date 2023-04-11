#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <utils.h>

int main() {
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

    int pfd;

    if ((pfd = connect(sfd, (struct sockaddr*) &addr, sizeof(addr))) == -1) {
        int errsv = errno;
        fprintf(stderr, "collector: \x1B[1;31merror:\x1B[0m connect(): %s\n", strerror(errsv)); 
        exit(errsv);
    }
}