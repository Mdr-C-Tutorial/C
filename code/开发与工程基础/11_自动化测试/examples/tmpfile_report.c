/*
 * 用 tmpfile 捕获 wf_report_print 的输出，再读回比较。
 * 在 wordfreq/ 目录下编译：
 *   gcc -std=c17 -Wall -Wextra -Iinclude -Itests \
 *       -o build/test_report ../examples/tmpfile_report.c tests/check.c src/report.c
 */
#include <stdio.h>
#include <string.h>

#include "check.h"
#include "wf/counter.h"
#include "wf/report.h"

/* 读一行并去掉行尾的换行，比较时不把换行也算进期望值；读不到时把 buf 置为空串 */
static void read_line(FILE *in, char *buf, size_t cap) {
    buf[0] = '\0';
    if (fgets(buf, (int)cap, in) == NULL) {
        return;
    }
    buf[strcspn(buf, "\n")] = '\0';
}

static void test_print_format(void) {
    wf_entry entries[2] = {{"the", 4}, {"on", 2}};
    FILE *out = tmpfile();
    CHECK(out != NULL);
    if (out == NULL) {
        return;
    }

    wf_report_print(out, entries, 2);

    rewind(out);
    char line[64];
    read_line(out, line, sizeof line);
    CHECK_STR_EQ(line, "      4 the");
    read_line(out, line, sizeof line);
    CHECK_STR_EQ(line, "      2 on");
    fclose(out);
}

int main(void) {
    test_print_format();
    return check_report("test_report");
}
