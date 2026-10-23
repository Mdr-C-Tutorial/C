#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <getopt.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <bpf/bpf.h>
#include <bpf/libbpf.h>

#include "openwatch.h"
#include "openwatch.skel.h"

static volatile sig_atomic_t exiting;
static bool verbose_log;

static bool show_time;
static const char *name_filter;
static __u64 first_ts;

static void usage(const char *prog) {
    fprintf(stderr,
            "用法: %s [-p PID] [-n 名称] [-x] [-T] [-d 秒数] [-v]\n"
            "\n"
            "  -p PID   只跟踪这个进程\n"
            "  -n 名称  只显示进程名含该子串的事件\n"
            "  -x       只显示失败的打开\n"
            "  -T       每行前加时间戳\n"
            "  -d 秒数  运行指定时间后退出\n"
            "  -v       输出 libbpf 的调试日志\n",
            prog);
}

static void on_signal(int sig) {
    (void)sig;
    exiting = 1;
}

static int libbpf_log(enum libbpf_print_level level, const char *fmt,
                      va_list args) {
    if (level == LIBBPF_DEBUG && !verbose_log) {
        return 0;
    }
    return vfprintf(stderr, fmt, args);
}

/* 每 CPU 数组 map：一次取回每个 CPU 一份计数，再在用户态求和 */
static void print_dropped(struct openwatch_bpf *skel) {
    int ncpus = libbpf_num_possible_cpus();
    int fd = bpf_map__fd(skel->maps.dropped);
    __u64 *values;
    __u32 key = 0;
    __u64 total = 0;

    if (ncpus <= 0) {
        return;
    }
    values = calloc((size_t)ncpus, sizeof(*values));
    if (values == NULL) {
        return;
    }
    if (bpf_map_lookup_elem(fd, &key, values) == 0) {
        for (int i = 0; i < ncpus; i++) {
            total += values[i];
        }
    }
    if (total != 0) {
        fprintf(stderr, "环形缓冲区满，丢弃了 %llu 个事件\n",
                (unsigned long long)total);
    }
    free(values);
}

static int handle_event(void *ctx, void *data, size_t size) {
    const struct event *e = data;
    int fd;
    int err;

    (void)ctx;
    if (size < sizeof(*e)) {
        return 0;
    }
    if (name_filter != NULL && strstr(e->comm, name_filter) == NULL) {
        return 0;
    }

    if (e->ret >= 0) {
        fd = e->ret;
        err = 0;
    } else {
        fd = -1;
        err = -e->ret;
    }

    if (show_time) {
        if (first_ts == 0) {
            first_ts = e->ts;
        }
        printf("%-14.9f", (double)(e->ts - first_ts) / 1e9);
    }
    printf("%-6u %-16s %4d %3d %s\n", e->pid, e->comm, fd, err, e->path);
    return 0;
}

int main(int argc, char *argv[]) {
    struct openwatch_bpf *skel;
    struct ring_buffer *rb = NULL;
    struct sigaction sa = { .sa_handler = on_signal };
    struct timespec deadline;
    bool has_deadline = false;
    __u32 pid = 0;
    int duration = 0;
    bool only_failed = false;
    int err;
    int opt;

    while ((opt = getopt(argc, argv, "p:n:xd:Tvh")) != -1) {
        switch (opt) {
        case 'p':
            pid = (__u32)strtoul(optarg, NULL, 10);
            break;
        case 'n':
            name_filter = optarg;
            break;
        case 'x':
            only_failed = true;
            break;
        case 'T':
            show_time = true;
            break;
        case 'd':
            duration = atoi(optarg);
            break;
        case 'v':
            verbose_log = true;
            break;
        case 'h':
            usage(argv[0]);
            return 0;
        default:
            usage(argv[0]);
            return 1;
        }
    }
    if (optind < argc) {
        usage(argv[0]);
        return 1;
    }

    libbpf_set_print(libbpf_log);

    skel = openwatch_bpf__open();
    if (skel == NULL) {
        fprintf(stderr, "打开 BPF 对象失败\n");
        return 1;
    }

    /* 只读参数必须在 open 之后、load 之前写入 */
    skel->rodata->targ_tgid = pid;
    skel->rodata->targ_failed = only_failed;

    err = openwatch_bpf__load(skel);
    if (err != 0) {
        fprintf(stderr, "加载 BPF 对象失败: %s\n", strerror(-err));
        goto cleanup;
    }

    err = openwatch_bpf__attach(skel);
    if (err != 0) {
        fprintf(stderr, "附加 BPF 程序失败: %s\n", strerror(-err));
        goto cleanup;
    }

    rb = ring_buffer__new(bpf_map__fd(skel->maps.events), handle_event, NULL,
                          NULL);
    if (rb == NULL) {
        fprintf(stderr, "创建环形缓冲区失败: %s\n", strerror(errno));
        err = 1;
        goto cleanup;
    }

    sigemptyset(&sa.sa_mask);
    if (sigaction(SIGINT, &sa, NULL) != 0 ||
        sigaction(SIGTERM, &sa, NULL) != 0) {
        fprintf(stderr, "设置信号处理函数失败: %s\n", strerror(errno));
        err = 1;
        goto cleanup;
    }

    if (show_time) {
        printf("%-14s", "TIME(s)");
    }
    printf("%-6s %-16s %4s %3s %s\n", "PID", "COMM", "FD", "ERR", "PATH");

    if (duration > 0) {
        clock_gettime(CLOCK_MONOTONIC, &deadline);
        deadline.tv_sec += duration;
        has_deadline = true;
    }

    err = 0;
    while (!exiting) {
        int n = ring_buffer__poll(rb, 100);
        if (n == -EINTR) {
            continue;
        }
        if (n < 0) {
            fprintf(stderr, "轮询环形缓冲区失败: %s\n", strerror(-n));
            err = 1;
            break;
        }
        if (has_deadline) {
            struct timespec now;
            clock_gettime(CLOCK_MONOTONIC, &now);
            if (now.tv_sec > deadline.tv_sec ||
                (now.tv_sec == deadline.tv_sec &&
                 now.tv_nsec >= deadline.tv_nsec)) {
                break;
            }
        }
    }

cleanup:
    print_dropped(skel);
    ring_buffer__free(rb);
    openwatch_bpf__destroy(skel);
    return err != 0;
}
