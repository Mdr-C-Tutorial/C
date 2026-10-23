#include "lines.h"

#include <ctype.h>
#include <stdlib.h>

#include "reader.h"

size_t count_words_in_file(const char *path) {
    size_t len = 0;
    char *text = reader_load(path, &len);
    if (text == NULL) {
        return 0;
    }

    size_t words = 0;
    int in_word = 0;
    for (size_t i = 0; i < len; i++) {
        if (isspace((unsigned char)text[i])) {
            in_word = 0;
        } else if (!in_word) {
            in_word = 1;
            words++;
        }
    }
    free(text);
    return words;
}
