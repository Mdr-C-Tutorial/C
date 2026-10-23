/* syscall() 不在 POSIX 里，glibc 只在 _GNU_SOURCE 下声明它。 */
#define _GNU_SOURCE

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/syscall.h>
#include <unistd.h>

#include <bpf/libbpf.h>
#include <linux/perf_event.h>

#include "perf_sample.h"
#include "perf_sample.skel.h"

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

/* libbpf 内部有一个同名静态函数，这里按 profile.c 的做法自己包一层。 */
static long perf_event_open(struct perf_event_attr *attr, pid_t pid, int cpu, int group_fd,
                            unsigned long flags)
{
    return syscall(__NR_perf_event_open, attr, pid, cpu, group_fd, flags);
}

static int handle_event(void *ctx, void *data, size_t size)
{
    const struct stack_sample *e = data;

    (void)ctx;
    (void)size;
    printf("%-16s pid=%-6u cpu=%u 栈帧 %d 个\n", (const char *)e->comm, e->pid, e->cpu,
           e->kstack_sz / (int)sizeof(__u64));
    return 0;
}

int main(int argc, char *argv[])
{
    struct perf_sample_bpf *skel;
    struct perf_event_attr attr;
    struct ring_buffer *rb = NULL;
    struct bpf_link **links = NULL;
    int freq = 99;
    int ncpu = 0;
    int cpu;
    int err = 0;

    if (argc > 1) {
        freq = atoi(argv[1]);
        if (freq < 1) {
            freq = 1;
        }
    }

    libbpf_set_print(print_libbpf);

    skel = perf_sample_bpf__open_and_load();
    if (skel == NULL) {
        fprintf(stderr, "无法打开并加载骨架\n");
        return 1;
    }

    rb = ring_buffer__new(bpf_map__fd(skel->maps.events), handle_event, NULL, NULL);
    if (rb == NULL) {
        fprintf(stderr, "创建环形缓冲区失败\n");
        err = 1;
        goto cleanup;
    }

    ncpu = (int)sysconf(_SC_NPROCESSORS_CONF);
    links = calloc((size_t)ncpu, sizeof(*links));
    if (links == NULL) {
        err = 1;
        goto cleanup;
    }

    memset(&attr, 0, sizeof(attr));
    attr.type = PERF_TYPE_SOFTWARE;
    attr.size = sizeof(attr);
    attr.config = PERF_COUNT_SW_CPU_CLOCK;
    attr.sample_freq = (unsigned long)freq;
    attr.freq = 1;

    for (cpu = 0; cpu < ncpu; cpu++) {
        int pefd = (int)perf_event_open(&attr, -1, cpu, -1, PERF_FLAG_FD_CLOEXEC);

        if (pefd < 0) {
            continue;
        }
        /* 附加成功后，perf 事件描述符交给 link 管理，bpf_link__destroy 会关闭它；
         * 失败时 link 不接管，由这里关掉。 */
        links[cpu] = bpf_program__attach_perf_event(skel->progs.profile, pefd);
        if (links[cpu] == NULL) {
            fprintf(stderr, "附加到 CPU %d 失败：%s\n", cpu, strerror(errno));
            close(pefd);
        }
    }

    printf("按 %d Hz 采样，Ctrl-C 结束\n", freq);
    while (!stop) {
        ring_buffer__poll(rb, 100);
    }

cleanup:
    if (links != NULL) {
        for (cpu = 0; cpu < ncpu; cpu++) {
            bpf_link__destroy(links[cpu]);
        }
        free(links);
    }
    ring_buffer__free(rb);
    perf_sample_bpf__destroy(skel);
    return err;
}