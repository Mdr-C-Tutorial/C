#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <unistd.h>

int main(void) {
    char line[32];
    for (int i = 1; i <= 10000; i++) {
        int n = snprintf(line, sizeof line, "line %d\n", i);
        if (write(STDOUT_FILENO, line, (size_t)n) == -1) {
            perror("write");
            return 1;
        }
    }
    return 0;
}
