/*
 * 生成可重复的输入：先造出 M 个人工单词，再按偏斜分布从里面抽取 N 个，
 * 写成一行一个单词的文本文件。
 *
 *   gen_words M N SEED > words.txt
 *
 * M 在 1 到 100000 之间，N 是单词个数，SEED 是 32 位无符号整数。
 * 单词由小写字母组成，长度 4 到 11，内容只由种子与下标决定，与 N 无关。
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define MAX_WORDS 100000
#define MIN_WORD_LEN 4
#define MAX_WORD_LEN 11
#define MAX_WORD_STORE (MAX_WORDS * (MAX_WORD_LEN + 1))

static uint32_t rng_state;

/* 线性同余生成器：参数取 Numerical Recipes 的一组常数 */
static uint32_t next_random(void) {
    rng_state = rng_state * 1664525u + 1013904223u;
    return rng_state;
}

static size_t bounded(size_t bound) {
    if (bound == 0) {
        return 0;
    }
    return (size_t)(next_random() % (uint32_t)bound);
}

int main(int argc, char *argv[]) {
    if (argc != 4) {
        fprintf(stderr, "usage: %s M N SEED\n", argv[0]);
        return 2;
    }
    long m = strtol(argv[1], NULL, 10);
    long n = strtol(argv[2], NULL, 10);
    unsigned long seed = strtoul(argv[3], NULL, 10);
    if (m < 1 || m > MAX_WORDS || n < 0) {
        fprintf(stderr, "gen_words: M must be 1..%d, N >= 0\n", MAX_WORDS);
        return 2;
    }

    /* 造词：种子只在这里用一次，抽词时状态已经确定 */
    char *store = malloc((size_t)m * (MAX_WORD_LEN + 1));
    if (store == NULL) {
        fprintf(stderr, "gen_words: out of memory\n");
        return 1;
    }
    rng_state = (uint32_t)seed;
    for (long i = 0; i < m; i++) {
        char *word = store + (size_t)i * (MAX_WORD_LEN + 1);
        size_t len = MIN_WORD_LEN + bounded(MAX_WORD_LEN - MIN_WORD_LEN + 1);
        for (size_t j = 0; j < len; j++) {
            word[j] = (char)('a' + (int)bounded(26));
        }
        word[len] = '\0';
    }

    /* 抽词：偏斜分布让少数单词占多数，接近自然语言里的高频词 */
    uint32_t pick_state = (uint32_t)seed ^ 0x9e3779b9u;
    for (long i = 0; i < n; i++) {
        pick_state = pick_state * 1664525u + 1013904223u;
        size_t pick = (size_t)(pick_state % (uint32_t)m);
        uint32_t skew_state = pick_state;
        /* 连续取两次随机数，较小的那个让低下标更容易被选中 */
        uint32_t other = skew_state * 1664525u + 1013904223u;
        size_t index = pick < (size_t)other % (uint32_t)m ? pick
                                                          : (size_t)other % (uint32_t)m;
        fputs(store + index * (MAX_WORD_LEN + 1), stdout);
        putchar(i + 1 == n ? '\n' : ' ');
    }

    free(store);
    return 0;
}
