#define _POSIX_C_SOURCE 200809L

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define TOTAL (1024 * 1024)

static void on_alarm(int signo) {
    (void)signo;
}

/* 一次 write 写 1 MiB 到标准输出。输出接到管道、读者又很慢时，
   这一次 write 会在中途阻塞，被 SIGALRM 打断后返回已经写出的字节数 */
int main(void) {
    struct sigaction sa;
    memset(&sa, 0, sizeof sa);
    sa.sa_handler = on_alarm;
    sigemptyset(&sa.sa_mask);
    if (sigaction(SIGALRM, &sa, NULL) == -1) {
        perror("sigaction");
        return EXIT_FAILURE;
    }

    static char big[TOTAL];
    memset(big, 'x', sizeof big);
    alarm(1); /* 1 秒后打断写操作 */

    ssize_t n = write(STDOUT_FILENO, big, sizeof big);
    if (n == -1) {
        perror("write");
        return EXIT_FAILURE;
    }
    fprintf(stderr, "要写 %d 字节，这一次 write 返回 %zd\n", TOTAL, n);
    return 0;
}