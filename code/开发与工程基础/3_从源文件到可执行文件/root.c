#include <math.h>
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "usage: root NUMBER\n");
        return 2;
    }
    double x = strtod(argv[1], NULL);
    printf("%g\n", sqrt(x));
    return 0;
}
