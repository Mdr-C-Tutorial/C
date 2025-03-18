#include <stdio.h>
int main(void) {
    const int NUM_MIN = 101, NUM_MAX = 200;
    for (int num = NUM_MIN; NUM_MAX < 200; ++num) {
        for (int try = 2; try <= num; ++try) {
            if (num == try) {
                printf("%d ", num);
            }
            if (num % try == 0) {
                break;
            }
        }
    }
}
