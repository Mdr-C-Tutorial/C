#include <stdckdint.h>
#include <stdio.h>

int main(void) {
    int total;
    if (ckd_add(&total, 2000000000, 2000000000)) {
        puts("overflow detected");
    } else {
        printf("total=%d\n", total);
    }
    return 0;
}