/* SPDX-License-Identifier: MIT */
/*
 * wordfreq：统计文本中出现次数最多的单词。
 *
 * 本文件负责命令行参数和读文件；分词、计数、输出格式分别由
 * token、counter、report 三个模块完成。
 *
 * 读文件有两套实现：配置时检测到 getline 就用它按行读入，
 * 否则用 fgets 分块读入。两条路径的行为相同，都要测。
 */
#if !defined(_WIN32)
#define _POSIX_C_SOURCE 200809L /* getline 需要 POSIX 2008 */
#endif

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "wf/config.h"
#include "wf/counter.h"
#include "wf/report.h"
#include "wf/token.h"
#include "wf/version.h"

#define DEFAULT_N 10  /* 没有 -n 时输出的行数 */
#define MAX_N 1000000 /* -n 允许的最大值 */

static int usage(void) {
    fputs("usage: wordfreq [-n N] [FILE...]\n", stderr);
    return 2;
}

static int out_of_memory(void) {
    fputs("wordfreq: out of memory\n", stderr);
    return 1;
}

/* 把 -n 的实参转换成 1 到 MAX_N 之间的整数；成功返回 0，否则返回 -1 */
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

/* 把一段文本中的单词加入 counter；0 成功，-1 内存不足 */
static int count_text(wf_counter *counter, const char *text, size_t len) {
    wf_tokenizer tz;
    wf_tokenizer_init(&tz, text, len);
    char word[WF_WORD_MAX];
    while (wf_tokenizer_next(&tz, word, sizeof word) > 0) {
        if (wf_counter_add(counter, word) != 0) {
            return -1;
        }
    }
    return 0;
}

#ifdef WF_CONFIG_HAVE_GETLINE
/*
 * 按行读入：getline 按需增长缓冲区。换行是分隔符，单词不会跨行，
 * 所以逐行分词与整文件分词的结果相同。
 */
static int count_stream(wf_counter *counter, FILE *in, const char *name) {
    char *line = NULL;
    size_t cap = 0;
    int status = 0;
    ssize_t got;
    while ((got = getline(&line, &cap, in)) >= 0) {
        if (count_text(counter, line, (size_t)got) != 0) {
            status = out_of_memory();
            break;
        }
    }
    if (status == 0 && ferror(in)) {
        fprintf(stderr, "wordfreq: %s: %s\n", name, strerror(errno));
        status = 1;
    }
    free(line);
    return status;
}
#else
/* 用 fgets 分块读入：缓冲区大小固定，长行会分成多段，结果仍然相同 */
static int count_stream(wf_counter *counter, FILE *in, const char *name) {
    char chunk[4096];
    int status = 0;
    while (fgets(chunk, sizeof chunk, in) != NULL) {
        size_t len = strlen(chunk);
        if (count_text(counter, chunk, len) != 0) {
            status = out_of_memory();
            break;
        }
    }
    if (status == 0 && ferror(in)) {
        fprintf(stderr, "wordfreq: %s: %s\n", name, strerror(errno));
        status = 1;
    }
    return status;
}
#endif

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

/* 输出次数最多的 n 个单词 */
static int print_top(const wf_counter *counter, size_t n) {
    wf_entry *entries = calloc(n, sizeof *entries);
    if (entries == NULL) {
        return out_of_memory();
    }
    size_t written = wf_counter_top(counter, entries, n);
    int status = 0;
    if (written == 0 && wf_counter_size(counter) > 0) {
        status = out_of_memory(); /* 有单词却一项也取不出来 */
    } else {
        wf_report_print(stdout, entries, written);
    }
    free(entries);
    return status;
}

int main(int argc, char *argv[]) {
    size_t n = DEFAULT_N;
    int argi = 1;
    while (argi < argc && argv[argi][0] == '-') {
        if (strcmp(argv[argi], "--version") == 0) {
            printf("wordfreq %s\n", wf_version());
            return 0;
        }
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
