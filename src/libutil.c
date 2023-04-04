#include <stdlib.h>
#include <string.h>
#include <libutil.h>

// Check if the string 's' is a number and store in 'n'
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
