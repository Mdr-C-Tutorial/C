#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <bpf/libbpf.h>

#include "tp_openat.skel.h"

static volatile sig_atomic_t stop;

static void handle_sigint(int signo)
{
    (void)signo;
    stop = 1;
}

static int print_libbpf(enum libbpf_print_level level, const char *format, va_list args)
{
    (void)level;
    return vfprintf(stderr, format, args);
}

int main(void)
{
    struct tp_openat_bpf *skel;
    struct sigaction sa;
    int err;

    libbpf_set_print(print_libbpf);

    skel = tp_openat_bpf__open_and_load();
    if (skel == NULL) {
        fprintf(stderr, "无法打开并加载骨架\n");
        return 1;
    }

    /* SEC 名里已经写全了类别与事件名，骨架可以自动附加 */
    err = tp_openat_bpf__attach(skel);
    if (err != 0) {
        fprintf(stderr, "附加失败：%s\n", strerror(-err));
        goto cleanup;
    }

    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handle_sigint;
    sigaction(SIGINT, &sa, NULL);

    printf("已启动，另开一个终端执行 sudo cat /sys/kernel/tracing/trace_pipe\n");
    while (!stop) {
        sleep(1);
    }

cleanup:
    tp_openat_bpf__destroy(skel);
    return err != 0;
}