/* SPDX-License-Identifier: MIT */
#include "check.h"

#include <string.h>

#include "wf/token.h"

static void test_plain_words(void) {
    const char *text = "cat dog";
    wf_tokenizer tz;
    wf_tokenizer_init(&tz, text, strlen(text));
    char word[WF_WORD_MAX];

    CHECK_EQ_SIZE(wf_tokenizer_next(&tz, word, sizeof word), 3);
    CHECK_STR_EQ(word, "cat");
    CHECK_EQ_SIZE(wf_tokenizer_next(&tz, word, sizeof word), 3);
    CHECK_STR_EQ(word, "dog");
    CHECK_EQ_SIZE(wf_tokenizer_next(&tz, word, sizeof word), 0);
}

static void test_punctuation_and_digits(void) {
    const char *text = "hello, world! 123 foo_bar";
    wf_tokenizer tz;
    wf_tokenizer_init(&tz, text, strlen(text));
    char word[WF_WORD_MAX];

    CHECK_EQ_SIZE(wf_tokenizer_next(&tz, word, sizeof word), 5);
    CHECK_STR_EQ(word, "hello");
    CHECK_EQ_SIZE(wf_tokenizer_next(&tz, word, sizeof word), 5);
    CHECK_STR_EQ(word, "world");
    CHECK_EQ_SIZE(wf_tokenizer_next(&tz, word, sizeof word), 3);
    CHECK_STR_EQ(word, "foo");
    CHECK_EQ_SIZE(wf_tokenizer_next(&tz, word, sizeof word), 3);
    CHECK_STR_EQ(word, "bar");
    CHECK_EQ_SIZE(wf_tokenizer_next(&tz, word, sizeof word), 0);
}

static void test_lowercase_folding(void) {
    const char *text = "The THE tHe";
    wf_tokenizer tz;
    wf_tokenizer_init(&tz, text, strlen(text));
    char word[WF_WORD_MAX];

    CHECK_EQ_SIZE(wf_tokenizer_next(&tz, word, sizeof word), 3);
    CHECK_STR_EQ(word, "the");
    CHECK_EQ_SIZE(wf_tokenizer_next(&tz, word, sizeof word), 3);
    CHECK_STR_EQ(word, "the");
    CHECK_EQ_SIZE(wf_tokenizer_next(&tz, word, sizeof word), 3);
    CHECK_STR_EQ(word, "the");
    CHECK_EQ_SIZE(wf_tokenizer_next(&tz, word, sizeof word), 0);
}

static void test_no_words(void) {
    const char *text = "12345 !@#$%";
    wf_tokenizer tz;
    wf_tokenizer_init(&tz, text, strlen(text));
    char word[WF_WORD_MAX];

    CHECK_EQ_SIZE(wf_tokenizer_next(&tz, word, sizeof word), 0);
}

static void test_truncation(void) {
    const char *text = "abcdef next";
    wf_tokenizer tz;
    wf_tokenizer_init(&tz, text, strlen(text));
    char word[4];

    CHECK_EQ_SIZE(wf_tokenizer_next(&tz, word, sizeof word), 3);
    CHECK_STR_EQ(word, "abc");
    CHECK_EQ_SIZE(wf_tokenizer_next(&tz, word, sizeof word), 3);
    CHECK_STR_EQ(word, "nex");
    CHECK_EQ_SIZE(wf_tokenizer_next(&tz, word, sizeof word), 0);
}

int main(void) {
    test_plain_words();
    test_punctuation_and_digits();
    test_lowercase_folding();
    test_no_words();
    test_truncation();
    return check_report("test_token");
}
