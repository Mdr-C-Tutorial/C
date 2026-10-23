#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <bpf/libbpf.h>

#include "calc_uprobe.skel.h"

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
    struct calc_uprobe_bpf *skel;
    LIBBPF_OPTS(bpf_uprobe_opts, opts);
    struct sigaction sa;
    int err = 0;

    libbpf_set_print(print_libbpf);

    skel = calc_uprobe_bpf__open_and_load();
    if (skel == NULL) {
        fprintf(stderr, "无法打开并加载骨架\n");
        return 1;
    }

    /* 节名只有 "uprobe"/"uretprobe" 的两个程序在这里指定路径与函数名；
     * 节名写全了的 uprobed_sub 两个程序交给 __attach 自动附加。 */
    opts.func_name = "uprobed_add";
    opts.retprobe = false;
    skel->links.uprobe_add = bpf_program__attach_uprobe_opts(
        skel->progs.uprobe_add, -1, "./calc", 0, &opts);
    if (skel->links.uprobe_add == NULL) {
        err = -errno;
        fprintf(stderr, "附加 uprobe_add 失败：%s\n", strerror(-err));
        goto cleanup;
    }

    opts.func_name = "uprobed_add";
    opts.retprobe = true;
    skel->links.uretprobe_add = bpf_program__attach_uprobe_opts(
        skel->progs.uretprobe_add, -1, "./calc", 0, &opts);
    if (skel->links.uretprobe_add == NULL) {
        err = -errno;
        fprintf(stderr, "附加 uretprobe_add 失败：%s\n", strerror(-err));
        goto cleanup;
    }

    err = calc_uprobe_bpf__attach(skel);
    if (err != 0) {
        fprintf(stderr, "附加失败：%s\n", strerror(-err));
        goto cleanup;
    }

    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handle_sigint;
    sigaction(SIGINT, &sa, NULL);

    printf("已启动，另开一个终端执行 ./calc\n");
    while (!stop) {
        sleep(1);
    }

cleanup:
    calc_uprobe_bpf__destroy(skel);
    return err != 0;
}