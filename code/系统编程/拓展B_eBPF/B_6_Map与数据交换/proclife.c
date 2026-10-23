#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <getopt.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <bpf/bpf.h>
#include <bpf/libbpf.h>

#include "proclife.h"
#include "proclife.skel.h"

static volatile sig_atomic_t stop;

static void on_signal(int signo)
{
    (void)signo;
    stop = 1;
}

static int print_libbpf(enum libbpf_print_level level, const char *format, va_list args)
{
    (void)level;
    return vfprintf(stderr, format, args);
}

static int handle_event(void *ctx, void *data, size_t len)
{
    const struct event *e = data;
    struct tm tm;
    char ts[16];
    time_t now;

    (void)ctx;
    (void)len;

    now = time(NULL);
    localtime_r(&now, &tm);
    strftime(ts, sizeof(ts), "%H:%M:%S", &tm);

    if (e->exit_event != 0) {
        printf("%-8s %-5s %-16s %-7u %-7u [%u]", ts, "EXIT", e->comm, e->pid,
               e->ppid, e->exit_code);
        if (e->duration_ns != 0) {
            printf(" (%llums)", e->duration_ns / 1000000);
        }
        putchar('\n');
    } else {
        printf("%-8s %-5s %-16s %-7u %-7u %s\n", ts, "EXEC", e->comm, e->pid,
               e->ppid, e->filename);
    }
    fflush(stdout);
    return 0;
}

static void usage(const char *prog)
{
    fprintf(stderr, "用法: %s [-d 毫秒] [-h]\n\n"
                    "  -d 毫秒  只报告存活时间不短于该值的进程\n"
                    "  -h       显示这段帮助\n",
            prog);
}

int main(int argc, char **argv)
{
    struct proclife_bpf *skel;
    struct ring_buffer *rb;
    struct sigaction sa;
    unsigned long long min_ms = 0;
    char *end;
    int opt;
    int err;

    while ((opt = getopt(argc, argv, "d:h")) != -1) {
        switch (opt) {
        case 'd':
            min_ms = strtoull(optarg, &end, 10);
            if (*end != '\0') {
                usage(argv[0]);
                return 1;
            }
            break;
        default:
            usage(argv[0]);
            return opt == 'h' ? 0 : 1;
        }
    }

    libbpf_set_print(print_libbpf);

    skel = proclife_bpf__open();
    if (skel == NULL) {
        fprintf(stderr, "打开骨架失败\n");
        return 1;
    }

    skel->rodata->min_duration_ns = min_ms * 1000000ULL;

    err = proclife_bpf__load(skel);
    if (err != 0) {
        fprintf(stderr, "加载失败：%s\n", strerror(-err));
        goto cleanup;
    }

    err = proclife_bpf__attach(skel);
    if (err != 0) {
        fprintf(stderr, "附加失败：%s\n", strerror(-err));
        goto cleanup;
    }

    rb = ring_buffer__new(bpf_map__fd(skel->maps.events), handle_event, NULL, NULL);
    if (rb == NULL) {
        fprintf(stderr, "创建环形缓冲区消费者失败\n");
        err = -errno;
        goto cleanup;
    }

    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = on_signal;
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);

    printf("%-8s %-5s %-16s %-7s %-7s %s\n", "TIME", "EVENT", "COMM", "PID",
           "PPID", "FILENAME/EXIT CODE");
    fflush(stdout);

    while (!stop) {
        err = ring_buffer__poll(rb, 100);
        if (err == -EINTR) {
            err = 0;
            break;
        }
        if (err < 0) {
            fprintf(stderr, "轮询环形缓冲区失败：%s\n", strerror(-err));
            break;
        }
    }

    ring_buffer__free(rb);

cleanup:
    proclife_bpf__destroy(skel);
    return err != 0;
}