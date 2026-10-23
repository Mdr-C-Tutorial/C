#include <math.h>
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv)
{
    if (argc != 2) {
        fprintf(stderr, "usage: calc NUMBER\n");
        return 1;
    }
    double x = strtod(argv[1], NULL);
    printf("sqrt(%g) = %.6f\n", x, sqrt(x));
    return 0;
}