#define _POSIX_C_SOURCE 200809L

/* sleep(3) 被信号打断时返回剩余的秒数，而不是报错。 */
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

static void on_alarm(int signo) {
    (void)signo;
}

int main(void) {
    struct sigaction sa = {0};
    sa.sa_handler = on_alarm;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    if (sigaction(SIGALRM, &sa, NULL) == -1) {
        perror("sigaction");
        return 1;
    }

    alarm(1);
    unsigned int left = sleep(3);
    printf("sleep returned %u\n", left);
    return 0;
}