#include <stdio.h>

#include "globals.h"
#include "report.c"
#include "words.c"

int main(void) {
    count_letters("Hello, world!");
    count_letters("abc");
    g_word_count = 2;
    print_report();
    printf("words: %d\n", g_word_count);
    return 0;
}
