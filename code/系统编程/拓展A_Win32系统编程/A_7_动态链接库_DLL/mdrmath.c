#include "mdrmath.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int mdr_add(int a, int b) {
    return a + b;
}

char *mdr_greet(const char *name) {
    static const char prefix[] = "hello, ";
    if (name == NULL) {
        name = "world";
    }
    size_t len = sizeof prefix - 1 + strlen(name) + 1;
    char *buf = malloc(len);
    if (buf == NULL) {
        return NULL;
    }
    snprintf(buf, len, "%s%s", prefix, name);
    return buf;
}

void mdr_buffer_free(void *block) {
    free(block);
}