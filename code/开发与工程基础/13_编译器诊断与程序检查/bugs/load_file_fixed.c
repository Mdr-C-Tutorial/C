#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

static int load_file(const char *path, char **out, size_t *len) {
    FILE *in = fopen(path, "rb");
    if (in == NULL) {
        return -1;
    }
    char *buf = NULL;
    int status = -1;
    if (fseek(in, 0, SEEK_END) != 0) {
        goto done;
    }
    long size = ftell(in);
    if (size < 0) {
        goto done;
    }
    rewind(in);
    buf = malloc((size_t)size + 1);
    if (buf == NULL) {
        goto done;
    }
    size_t got = fread(buf, 1, (size_t)size, in);
    buf[got] = '\0';
    *out = buf;
    *len = got;
    buf = NULL;
    status = 0;

done:
    free(buf);
    fclose(in);
    return status;
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        return 2;
    }
    char *text = NULL;
    size_t len = 0;
    if (load_file(argv[1], &text, &len) != 0) {
        return 1;
    }
    printf("%zu\n", len);
    free(text);
    return 0;
}
