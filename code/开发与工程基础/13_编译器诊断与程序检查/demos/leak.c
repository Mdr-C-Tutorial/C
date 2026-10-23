#include <stdio.h>
#include <stdlib.h>

int main(void) {
    char *buf = malloc(64);
    if (buf == NULL) {
        return 1;
    }
    snprintf(buf, 64, "wordfreq");
    printf("%s\n", buf);
    /* 忘记 free(buf) */
    return 0;
}
