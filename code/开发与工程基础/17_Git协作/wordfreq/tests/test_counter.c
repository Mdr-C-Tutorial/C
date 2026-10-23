#include "check.h"

#include "wf/counter.h"

static void test_add_and_get(void) {
    wf_counter *c = wf_counter_create();
    CHECK(c != NULL);

    CHECK_EQ_SIZE(wf_counter_size(c), 0);
    CHECK_EQ_SIZE(wf_counter_get(c, "the"), 0);

    CHECK(wf_counter_add(c, "the") == 0);
    CHECK(wf_counter_add(c, "cat") == 0);
    CHECK(wf_counter_add(c, "the") == 0);

    CHECK_EQ_SIZE(wf_counter_size(c), 2);
    CHECK_EQ_SIZE(wf_counter_get(c, "the"), 2);
    CHECK_EQ_SIZE(wf_counter_get(c, "cat"), 1);
    CHECK_EQ_SIZE(wf_counter_get(c, "dog"), 0);

    wf_counter_destroy(c);
}

static void test_top_sorting(void) {
    wf_counter *c = wf_counter_create();
    CHECK(c != NULL);

    const char *words[] = {"the", "cat", "sat", "on", "the", "mat",
                           "the", "dog", "sat", "on", "the", "log"};
    for (size_t i = 0; i < sizeof words / sizeof words[0]; i++) {
        CHECK(wf_counter_add(c, words[i]) == 0);
    }

    wf_entry top[3];
    size_t n = wf_counter_top(c, top, 3);
    CHECK_EQ_SIZE(n, 3);
    CHECK_STR_EQ(top[0].word, "the");
    CHECK_EQ_SIZE(top[0].count, 4);
    CHECK_STR_EQ(top[1].word, "on");
    CHECK_EQ_SIZE(top[1].count, 2);
    CHECK_STR_EQ(top[2].word, "sat");
    CHECK_EQ_SIZE(top[2].count, 2);

    wf_counter_destroy(c);
}

static void test_top_more_than_size(void) {
    wf_counter *c = wf_counter_create();
    CHECK(c != NULL);

    CHECK(wf_counter_add(c, "a") == 0);
    CHECK(wf_counter_add(c, "b") == 0);

    wf_entry top[10];
    size_t n = wf_counter_top(c, top, 10);
    CHECK_EQ_SIZE(n, 2);
    CHECK_STR_EQ(top[0].word, "a");
    CHECK_STR_EQ(top[1].word, "b");

    wf_counter_destroy(c);
}

int main(void) {
    test_add_and_get();
    test_top_sorting();
    test_top_more_than_size();
    return check_report("test_counter");
}
