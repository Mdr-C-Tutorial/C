#define _POSIX_C_SOURCE 200809L

#include <setjmp.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <bpf/libbpf.h>

#include "usdt_setjmp.skel.h"

static volatile sig_atomic_t stop;
static jmp_buf env;

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
    struct usdt_setjmp_bpf *skel;
    struct sigaction sa;
    int err;

    libbpf_set_print(print_libbpf);

    skel = usdt_setjmp_bpf__open();
    if (skel == NULL) {
        fprintf(stderr, "无法打开骨架\n");
        return 1;
    }

    /* 全局对象在 open 之后、load 之前写入，验证器把它当作常量 */
    skel->bss->target_pid = getpid();

    err = usdt_setjmp_bpf__load(skel);
    if (err != 0) {
        fprintf(stderr, "加载失败：%s\n", strerror(-err));
        goto cleanup;
    }

    /* 只按名附加的那个程序，在这里指定 pid 与探针 */
    skel->links.usdt_manual_attach =
        bpf_program__attach_usdt(skel->progs.usdt_manual_attach, getpid(), "libc.so.6", "libc",
                                 "setjmp", NULL);
    if (skel->links.usdt_manual_attach == NULL) {
        err = -errno;
        fprintf(stderr, "手动附加失败：%s\n", strerror(-err));
        goto cleanup;
    }

    err = usdt_setjmp_bpf__attach(skel);
    if (err != 0) {
        fprintf(stderr, "附加失败：%s\n", strerror(-err));
        goto cleanup;
    }

    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handle_sigint;
    sigaction(SIGINT, &sa, NULL);

    printf("已启动，另开一个终端执行 sudo cat /sys/kernel/tracing/trace_pipe\n");
    while (!stop) {
        /* 每秒调用一次 setjmp，触发 libc 里预埋的 USDT 探针 */
        setjmp(env);
        sleep(1);
    }

cleanup:
    usdt_setjmp_bpf__destroy(skel);
    return err != 0;
}