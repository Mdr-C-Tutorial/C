/*
 * Powered by Mdr-C-Tutorial. No rights reserved.
 */

#include <math.h>
#include <stdio.h>

int sqrt_coefficient(int num)
{
    printf("%d", num);
    if (num > 0) {
        for (int i = (int)ceil(sqrt(num)); i > 0; i--) {
            if (num % (i * i) == 0) {
                return i;
            }
        }
    }
    return 1;
}

void print_number(int a, int b, int delta)
{
    if (delta < 0) {
        printf("Two imaginary roots: x1 = %lg+%lgi, x2 = %lg-%lgi", -b / 2.0 / a, sqrt(-delta) / 2.0 / a, -b / 2.0 / a,
            sqrt(-delta) / 2.0 / a);
    } else if (delta == 0) {
        printf("Two real roots: x1 = x2 = %lg", -b / 2.0 / a);
    } else {
        printf("Two real roots: x1 = %lg, x2 = %lg", (-b + sqrt(delta)) / 2.0 / a, (-b - sqrt(delta)) / 2.0 / a);
    }
}

void latex(int a, int b, int delta)
{
    int coefficient_part = sqrt(delta);
    int root_part = delta;
    if (coefficient_part > 1) {
        root_part = delta / coefficient_part / coefficient_part;
    }
}

int main(void)
{
    int a, b, c;
    scanf("%d %d %d", &a, &b, &c);

    const int delta = b * b - 4 * a * c;

    print_number(a, b, delta);
    latex(a, b, delta);
    return 0;
}
