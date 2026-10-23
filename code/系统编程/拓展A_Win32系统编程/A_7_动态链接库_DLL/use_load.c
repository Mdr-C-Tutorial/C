#include "mdrmath.h"

#include <stdio.h>

int main(void) {
    printf("mdr_add(2, 3) = %d\n", mdr_add(2, 3));

    char *msg = mdr_greet("load-time linking");
    if (msg == NULL) {
        return 1;
    }
    printf("%s\n", msg);
    mdr_buffer_free(msg);
    return 0;
}