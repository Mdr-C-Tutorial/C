#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <bpf/libbpf.h>

#include "hello.skel.h"

static volatile sig_atomic_t stop;

static void on_signal(int signo)
{
    (void)signo;
    stop = 1;
}

static int print_libbpf(enum libbpf_print_level level, const char *format,
                        va_list args)
{
    if (level == LIBBPF_DEBUG) {
        return 0;
    }
    return vfprintf(stderr, format, args);
}

int main(void)
{
    struct hello_bpf *skel;
    struct sigaction sa;
    int err;

    libbpf_set_print(print_libbpf);

    skel = hello_bpf__open();
    if (skel == NULL) {
        fprintf(stderr, "打开骨架失败\n");
        return 1;
    }

    /* 先写入进程号再加载，第一次事件到达时过滤条件已经就位 */
    skel->bss->my_pid = getpid();

    err = hello_bpf__load(skel);
    if (err != 0) {
        fprintf(stderr, "加载失败：%s\n", strerror(-err));
        goto cleanup;
    }

    err = hello_bpf__attach(skel);
    if (err != 0) {
        fprintf(stderr, "附加失败：%s\n", strerror(-err));
        goto cleanup;
    }

    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = on_signal;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);

    printf("已启动，PID 为 %d。另开一个终端执行 sudo cat "
           "/sys/kernel/tracing/trace_pipe。\n", getpid());
    fflush(stdout);

    while (!stop) {
        write(STDERR_FILENO, ".", 1);
        sleep(1);
    }

    write(STDERR_FILENO, "\n", 1);

cleanup:
    hello_bpf__destroy(skel);
    return err != 0;
}
