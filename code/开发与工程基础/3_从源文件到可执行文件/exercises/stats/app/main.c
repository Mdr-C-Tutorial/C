#include <stdio.h>

#include "stats.h"

int main(void) {
    double values[] = {2.0, 4.0, 4.0, 4.0, 5.0, 5.0, 7.0, 9.0};
    printf("mean = %.2f\n", mean(values, 8));
    printf("spread = %.2f\n", spread(values, 8));
    return 0;
}
