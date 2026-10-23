#define _POSIX_C_SOURCE 200809L

#include <fcntl.h>
#include <mqueue.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define MQ_NAME "/p087_mq"

static void die(const char *what) {
    perror(what);
    exit(EXIT_FAILURE);
}

int main(void) {
    struct mq_attr attr = {0};
    attr.mq_maxmsg = 10;
    attr.mq_msgsize = 64;

    mqd_t mq = mq_open(MQ_NAME, O_CREAT | O_RDWR, 0600, &attr);
    if (mq == (mqd_t)-1) {
        die("mq_open");
    }

    /* 优先级高的先被取出 */
    if (mq_send(mq, "普通消息", strlen("普通消息"), 1) == -1) {
        die("mq_send");
    }
    if (mq_send(mq, "紧急消息", strlen("紧急消息"), 9) == -1) {
        die("mq_send");
    }

    char buf[64];
    for (int i = 0; i < 2; i++) {
        unsigned prio = 0;
        ssize_t n = mq_receive(mq, buf, sizeof buf, &prio);
        if (n == -1) {
            die("mq_receive");
        }
        buf[n] = '\0';
        printf("取出优先级 %u：%s\n", prio, buf);
    }

    mq_close(mq);
    mq_unlink(MQ_NAME);
    return EXIT_SUCCESS;
}
