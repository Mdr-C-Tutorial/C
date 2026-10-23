#include "check.h"
#include "lines.h"

int main(void) {
    /* 假实现固定返回 6 个单词，与文件系统无关 */
    CHECK_EQ_SIZE(count_words_in_file("not/a/real/path"), 6);
    return check_report("test_lines");
}
