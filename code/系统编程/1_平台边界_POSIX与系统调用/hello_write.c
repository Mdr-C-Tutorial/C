#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <unistd.h>

int main(void) {
    static const char msg[] = "hello, world\n";
    if (write(STDOUT_FILENO, msg, sizeof msg - 1) == -1) {
        perror("write");
        return 1;
    }
    return 0;
}
