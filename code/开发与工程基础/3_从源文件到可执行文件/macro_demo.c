#include <stdio.h>

/* 摄氏温度换算为华氏温度 */
#define FAHRENHEIT(c) ((c) * 9.0 / 5.0 + 32.0)
#define STEP 10
#define LIMIT (3 * STEP)

int main(void) {
    for (int c = 0; c <= LIMIT; c += STEP) {
#ifdef DEBUG
        fprintf(stderr, "debug: c = %d\n", c);
#endif
        printf("%3d C = %5.1f F\n", c, FAHRENHEIT(c));
    }
    return 0;
}
