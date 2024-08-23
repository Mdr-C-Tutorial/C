/*
 * Powered by Mdr-C-Tutorial. No rights reserved.
 */

#include <math.h>
#include <stdio.h>

int sqrt_coefficient(int num)
{
    if (num < 0)
    {
        num = -num;
    }
    for (int i = (int)ceil(sqrt(num)); i > 0; i--)
    {
        if (num % (i * i) == 0)
        {
            return i;
        }
    }
    return 1;
}

void cancelling_2(int *denominator, int *n1)
{
    const int minimal = *denominator < *n1 ? *denominator : *n1;
    for (int i = minimal; i > 1; i--)
    {
        if (*denominator / i * i == *denominator && *n1 / i * i == *n1)
        {
            *denominator /= i;
            *n1 /= i;
            break;
        }
    }
}

void cancelling(int *denominator, int *n1, int *n2, const int delta)
{
    if (delta == 0)
    {
        cancelling_2(denominator, n1);
    }
    else
    {
        const int tmp1 = *n1 < *n2 ? *n1 : *n2;
        const int minimal = *denominator < tmp1 ? *denominator : tmp1;

        for (int i = minimal; i > 1; i--)
        {
            if (*denominator / i * i == *denominator && *n1 / i * i == *n1 && *n2 / i * i == *n2)
            {
                *denominator /= i;
                *n1 /= i;
                *n2 /= i;
                break;
            }
        }
    }
}

void print_number(const int a, const int b, const int delta)
{
    if (delta < 0)
    {
        printf("Two imaginary roots: x1 = %lg+%lgi, x2 = %lg-%lgi\n", -b / 2.0 / a, sqrt(-delta) / 2.0 / a,
               -b / 2.0 / a,
               sqrt(-delta) / 2.0 / a);
    }
    else if (delta == 0)
    {
        printf("Two real roots: x1 = x2 = %lg\n", -b / 2.0 / a);
    }
    else
    {
        printf("Two real roots: x1 = %lg, x2 = %lg\n", (-b + sqrt(delta)) / 2.0 / a, (-b - sqrt(delta)) / 2.0 / a);
    }
}

void latex_numerator(int b, const int coefficient_part, int root_part)
{
    printf("%d \\pm ", -b);
    bool imaginary = false;
    if (root_part <= 0)
    {
        root_part = -root_part;
        imaginary = true;
    }
    if (coefficient_part != 1)
    {
        printf("%d", coefficient_part);
    }
    if (root_part != 1)
    {
        printf("\\sqrt{%d}", root_part);
    }
    if (imaginary)
    {
        fputs("i", stdout);
    }
}

void latex(const int a, int b, const int delta)
{
    int coefficient_part = sqrt_coefficient(delta);
    int root_part = delta;
    if (coefficient_part > 1)
    {
        root_part = delta / coefficient_part / coefficient_part;
    }
    int denominator = 2 * a;
    cancelling(&denominator, &b, &coefficient_part, delta);

    putchar('$');
    if (delta == 0)
    {
        if (b > 0)
        {
            putchar('-');
        }
        if (denominator > 1)
        {
            printf("\\frac{%d}{%d}", b, denominator);
        }
        else
        {
            printf("%d", denominator);
        }
    }
    else
    {
        if (denominator > 1)
        {
            fputs("\\frac{", stdout);
            latex_numerator(b, coefficient_part, root_part);
            printf("}{%d}", denominator);
        }
        else
        {
            latex_numerator(b, coefficient_part, root_part);
        }
    }
    putchar('$');
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
