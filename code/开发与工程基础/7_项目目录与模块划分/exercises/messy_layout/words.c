#include <string.h>

#include "util.c"
#include "words.h"

int count_letters(const char *s) {
    int n = 0;
    for (size_t i = 0; i < strlen(s); i++) {
        if (is_letter(s[i])) {
            n++;
        }
    }
    g_letter_count += n;
    return n;
}
