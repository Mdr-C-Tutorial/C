/* 可预测的临时文件名 vs mkstemp。
 * 只在当前目录创建文件，用完删掉。 */
#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int main(void) {
    /* 错误做法：名字可猜，O_CREAT 不加 O_EXCL，会跟随已存在的符号链接 */
    int bad = open("mytool.tmp", O_WRONLY | O_CREAT | O_TRUNC, 0600);
    if (bad == -1) {
        perror("open predictable");
        return 1;
    }
    static const char payload[] = "written through mytool.tmp\n";
    if (write(bad, payload, sizeof payload - 1) == -1) {
        perror("write");
        return 1;
    }
    printf("predictable name -> fd %d\n", bad);
    close(bad);
    unlink("mytool.tmp");

    /* 正确做法：mkstemp 用 O_EXCL 创建，名字不可预测 */
    char tmpl[] = "mytool.XXXXXX";
    int good = mkstemp(tmpl);
    if (good == -1) {
        perror("mkstemp");
        return 1;
    }
    printf("mkstemp name      -> %s, fd %d\n", tmpl, good);
    close(good);
    unlink(tmpl);
    return 0;
}