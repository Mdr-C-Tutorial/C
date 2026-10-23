#define _POSIX_C_SOURCE 200809L

#include <semaphore.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <unistd.h>

#define SLOTS 8
#define ITEMS 100
#define MSG_LEN 32

struct slot {
    int seq;
    char text[MSG_LEN];
};

struct ring {
    struct slot slots[SLOTS];
    size_t head; /* 生产者写这里 */
    size_t tail; /* 消费者读这里 */
    sem_t empty; /* 空槽个数 */
    sem_t full;  /* 已填槽个数 */
    sem_t mutex; /* 保护 head/tail 与槽内容 */
};

static void die(const char *what) {
    perror(what);
    exit(EXIT_FAILURE);
}

static void producer(struct ring *r) {
    for (int i = 0; i < ITEMS; i++) {
        sem_wait(&r->empty);
        sem_wait(&r->mutex);
        struct slot *s = &r->slots[r->head];
        s->seq = i;
        snprintf(s->text, sizeof s->text, "第 %d 条消息", i);
        r->head = (r->head + 1) % SLOTS;
        sem_post(&r->mutex);
        sem_post(&r->full);
    }
}

static void consumer(struct ring *r) {
    for (int i = 0; i < ITEMS; i++) {
        sem_wait(&r->full);
        sem_wait(&r->mutex);
        struct slot *s = &r->slots[r->tail];
        if (s->seq != i) {
            fprintf(stderr, "顺序错了：期待 %d，实际 %d\n", i, s->seq);
        }
        if (i % 25 == 0) {
            printf("消费 %s\n", s->text);
        }
        r->tail = (r->tail + 1) % SLOTS;
        sem_post(&r->mutex);
        sem_post(&r->empty);
    }
    printf("消费者收到全部 %d 条，顺序正确\n", ITEMS);
}

int main(void) {
    struct ring *r = mmap(NULL, sizeof *r, PROT_READ | PROT_WRITE,
                          MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if (r == MAP_FAILED) {
        die("mmap");
    }
    r->head = 0;
    r->tail = 0;
    if (sem_init(&r->empty, 1, SLOTS) == -1 || sem_init(&r->full, 1, 0) == -1 ||
        sem_init(&r->mutex, 1, 1) == -1) {
        die("sem_init");
    }

    pid_t pid = fork();
    if (pid == -1) {
        die("fork");
    }
    if (pid == 0) {
        consumer(r);
        fflush(stdout);
        _exit(EXIT_SUCCESS);
    }

    producer(r);
    if (waitpid(pid, NULL, 0) == -1) {
        die("waitpid");
    }
    sem_destroy(&r->empty);
    sem_destroy(&r->full);
    sem_destroy(&r->mutex);
    munmap(r, sizeof *r);
    return EXIT_SUCCESS;
}
