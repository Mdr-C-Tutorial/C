/*
 * Powered by Mdr-C-Tutorial. No rights reserved.
 */

#include <stdio.h>
#include <stdlib.h>
#include <time.h>

int main(void) {
    int secret = rand() % 100 + 1;
    for (;;) {
        int guess;
        printf("Enter your guess: ");
        scanf("%d", &guess);
        if (guess == secret) {
            printf("You win!\n");
            break;
        } else if (guess > secret) {
            printf("Too high!\n");
        } else {
            printf("Too low!\n");
        }
    }
    return 0;
}
