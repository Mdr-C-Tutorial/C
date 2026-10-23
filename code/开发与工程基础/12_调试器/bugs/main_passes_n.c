/*
 * 第 12 章缺陷 (a) 的后一半：main.c 把请求的 n 直接传给 wf_report_print，
 * 而不是 wf_counter_top 的返回值。calloc 得到的 entries[n] 中，实际没有写入的
 * 条目 word 为空指针，report 的对齐循环解引用它。
 *
 * 只替换 main.c；report.c 用 report_alignment.c。
 */
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "wf/counter.h"
#include "wf/report.h"
#include "wf/token.h"

#define DEFAULT_N 10
#define MAX_N 1000000

static int usage(void) {
    fputs("usage: wordfreq [-n N] [FILE...]\n", stderr);
    return 2;
}

static int out_of_memory(void) {
    fputs("wordfreq: out of memory\n", stderr);
    return 1;
}

static int parse_count(const char *arg, size_t *n) {
    char *end;
    errno = 0;
    long value = strtol(arg, &end, 10);
    if (end == arg || *end != '\0' || errno == ERANGE || value < 1 ||
        value > MAX_N) {
        return -1;
    }
    *n = (size_t)value;
    return 0;
}

static char *read_stream(FILE *in, size_t *len) {
    size_t cap = 4096;
    size_t used = 0;
    char *buf = malloc(cap);
    if (buf == NULL) {
        return NULL;
    }
    for (;;) {
        used += fread(buf + used, 1, cap - used, in);
        if (used < cap) {
            break;
        }
        char *bigger = NULL;
        if (cap <= SIZE_MAX / 2) {
            bigger = realloc(buf, cap * 2);
        }
        if (bigger == NULL) {
            free(buf);
            return NULL;
        }
        buf = bigger;
        cap *= 2;
    }
    if (ferror(in)) {
        free(buf);
        return NULL;
    }
    *len = used;
    return buf;
}

static int count_stream(wf_counter *counter, FILE *in, const char *name) {
    size_t len;
    char *text = read_stream(in, &len);
    if (text == NULL) {
        if (!ferror(in)) {
            return out_of_memory();
        }
        fprintf(stderr, "wordfreq: %s: %s\n", name, strerror(errno));
        return 1;
    }

    wf_tokenizer tz;
    wf_tokenizer_init(&tz, text, len);
    char word[WF_WORD_MAX];
    int status = 0;
    while (wf_tokenizer_next(&tz, word, sizeof word) > 0) {
        if (wf_counter_add(counter, word) != 0) {
            status = out_of_memory();
            break;
        }
    }
    free(text);
    return status;
}

static int count_file(wf_counter *counter, const char *path) {
    FILE *in = fopen(path, "rb");
    if (in == NULL) {
        fprintf(stderr, "wordfreq: %s: %s\n", path, strerror(errno));
        return 1;
    }
    int status = count_stream(counter, in, path);
    fclose(in);
    return status;
}

static int print_top(const wf_counter *counter, size_t n) {
    wf_entry *entries = calloc(n, sizeof *entries);
    if (entries == NULL) {
        return out_of_memory();
    }
    size_t written = wf_counter_top(counter, entries, n);
    int status = 0;
    if (written == 0 && wf_counter_size(counter) > 0) {
        status = out_of_memory();
    } else {
        /* 缺陷：传的是请求的 n，而不是实际写入的 written */
        wf_report_print(stdout, entries, n);
    }
    free(entries);
    return status;
}

int main(int argc, char *argv[]) {
    size_t n = DEFAULT_N;
    int argi = 1;
    while (argi < argc && argv[argi][0] == '-') {
        if (strcmp(argv[argi], "-n") == 0 && argi + 1 < argc &&
            parse_count(argv[argi + 1], &n) == 0) {
            argi += 2;
        } else {
            return usage();
        }
    }

    wf_counter *counter = wf_counter_create();
    if (counter == NULL) {
        return out_of_memory();
    }

    int status = 0;
    if (argi == argc) {
        status = count_stream(counter, stdin, "(standard input)");
    }
    for (; argi < argc && status == 0; argi++) {
        status = count_file(counter, argv[argi]);
    }
    if (status == 0) {
        status = print_top(counter, n);
    }
    wf_counter_destroy(counter);
    return status;
}
