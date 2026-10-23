/*
 * wordfreq：统计文本中出现次数最多的单词。
 *
 * 本文件负责命令行参数和读文件；分词、计数、输出格式分别由
 * token、counter、report 三个模块完成。
 */
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "wf/counter.h"
#include "wf/report.h"
#include "wf/token.h"

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

/*
 * 读入 in 的全部内容。成功时返回缓冲区（由调用方释放），字节数写入 *len；
 * 失败时返回空指针：ferror(in) 为真表示读取出错，否则是内存不足。
 */
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
            break; /* 没有读满：到达文件末尾或出错 */
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

/* 读入 in 的全部文本，把其中的单词加入 counter；name 只用于错误信息 */
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
