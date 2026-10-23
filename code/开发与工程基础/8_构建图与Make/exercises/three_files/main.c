#include <stdio.h>
#include <stdlib.h>

#include "calc.h"

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "usage: calc A B\n");
        return 2;
    }
    int a = atoi(argv[1]);
    int b = atoi(argv[2]);
    printf("sum: %d\n", calc_sum(a, b));
    printf("product: %d\n", calc_product(a, b));
    return 0;
}
