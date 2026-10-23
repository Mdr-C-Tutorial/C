#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

/* 请求读 4096 字节，打印这一次 read 实际拿到多少 */
int main(void) {
    char buf[4096];
    ssize_t n = read(STDIN_FILENO, buf, sizeof buf);
    if (n == -1) {
        perror("read");
        return EXIT_FAILURE;
    }
    printf("请求 4096 字节，read 返回 %zd，内容是：", n);
    fwrite(buf, 1, (size_t)n, stdout);
    return 0;
}