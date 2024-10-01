#include <stdio.h>
int main(void) {
    int num;
    int t;
    for (num = 101; num < 200; num++) {
        for (t = 2; t <= num; t++) {
            if (num == t) {
                printf("%d", num);
            }
            if (num % t == 0) {
                break;
            }
        }
    }
}
