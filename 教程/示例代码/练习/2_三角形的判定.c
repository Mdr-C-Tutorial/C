/*
 * Powered by Mdr-C-Tutorial. No rights reserved.
 */

#include <stdio.h>

int main(void)
{
    double a, b, c;
    bool right = false;
    bool isosceles = false;
    scanf("%lf %lf %lf", &a, &b, &c);
    if (a == b && b == c) {
        printf("Equilateral Triangle");
        return 0;
    }
    if (a == b || b == c || c == a) {
        isosceles = true;
    }
    if (a * a + b * b == c * c || b * b + c * c == a * a || a * a + c * c == b * b) {
        right = true;
    }
    if (right && isosceles) {
        printf("Isosceles Right Triangle");
    }
    if (right) {
        printf("Right Triangle");
    }
    if (isosceles) {
        printf("Isosceles Triangle");
    }
    return 0;
}
