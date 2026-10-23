#include "reader.h"

#include <stdio.h>
#include <stdlib.h>

char *reader_load(const char *path, size_t *len) {
    FILE *in = fopen(path, "rb");
    if (in == NULL) {
        return NULL;
    }
    if (fseek(in, 0, SEEK_END) != 0) {
        fclose(in);
        return NULL;
    }
    long size = ftell(in);
    if (size < 0 || fseek(in, 0, SEEK_SET) != 0) {
        fclose(in);
        return NULL;
    }
    char *buf = malloc((size_t)size + 1);
    if (buf == NULL) {
        fclose(in);
        return NULL;
    }
    size_t n = fread(buf, 1, (size_t)size, in);
    fclose(in);
    buf[n] = '\0';
    *len = n;
    return buf;
}
