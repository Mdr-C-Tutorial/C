#define _XOPEN_SOURCE 700

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/types.h>

/* System V 消息队列：认识就够，新代码不要用 */
struct msgbuf_text {
    long mtype;
    char mtext[64];
};

int main(void) {
    key_t key = ftok(".", 'q');
    if (key == (key_t)-1) {
        perror("ftok");
        return EXIT_FAILURE;
    }
    int qid = msgget(key, IPC_CREAT | 0600);
    if (qid == -1) {
        perror("msgget");
        return EXIT_FAILURE;
    }

    struct msgbuf_text msg;
    msg.mtype = 1;
    strcpy(msg.mtext, "hello sysv");
    if (msgsnd(qid, &msg, strlen(msg.mtext) + 1, 0) == -1) {
        perror("msgsnd");
        return EXIT_FAILURE;
    }
    if (msgrcv(qid, &msg, sizeof msg.mtext, 1, 0) == -1) {
        perror("msgrcv");
        return EXIT_FAILURE;
    }
    printf("收到：%s\n", msg.mtext);

    if (msgctl(qid, IPC_RMID, NULL) == -1) {
        perror("msgctl");
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
