#include <stdio.h>

#include "platform.h"

int main(void) {
    if (wf_stdout_is_terminal()) {
        printf("-- wordfreq 结果 --\n");
    }
    printf("the 4\n");
    return 0;
}
