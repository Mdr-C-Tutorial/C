#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void) {
    char *buf = malloc(8);
    if (buf == NULL) {
        return 1;
    }
    /* 故意越界：ASan 会在发生处报告 */
    for (int i = 0; i < 12; i++) {
        buf[i] = 'A';
    }
    printf("%s\n", buf);
    free(buf);
    return 0;
}