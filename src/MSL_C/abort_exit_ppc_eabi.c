#include <types.h>

int raise(int sig);
void exit(int status);

int __aborting = 0;
void (*__stdio_exit)(void) = 0;

void abort(void) {
    raise(1);
    __aborting = 1;
    exit(1);
}
