#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/prctl.h>
#include <sys/resource.h>
#include <unistd.h>

#define SIZE (32UL * 1024 * 1024)

struct sample {
    long vm_size; /* 虚拟地址空间大小，KiB */
    long rss;     /* 驻留集大小，KiB */
    long minflt;
    long majflt;
};

static struct sample take(void) {
    struct sample s = {0, 0, 0, 0};
    struct rusage ru;
    if (getrusage(RUSAGE_SELF, &ru) == -1) {
        perror("getrusage");
        exit(EXIT_FAILURE);
    }
    s.minflt = ru.ru_minflt;
    s.majflt = ru.ru_majflt;

    FILE *fp = fopen("/proc/self/status", "r");
    if (fp == NULL) {
        perror("fopen");
        exit(EXIT_FAILURE);
    }
    char line[256];
    while (fgets(line, sizeof line, fp) != NULL) {
        if (strncmp(line, "VmSize:", 7) == 0) {
            sscanf(line + 7, "%ld", &s.vm_size);
        } else if (strncmp(line, "VmRSS:", 6) == 0) {
            sscanf(line + 6, "%ld", &s.rss);
        }
    }
    fclose(fp);
    return s;
}

static void report(const char *stage, struct sample s) {
    printf("%s：VmSize %ld KiB，VmRSS %ld KiB，minflt %ld，majflt %ld\n", stage, s.vm_size,
           s.rss, s.minflt, s.majflt);
}

int main(void) {
    long page = sysconf(_SC_PAGESIZE);

    /* 关掉透明大页：这台机器的 THP 是 always，一次缺页可能装进 2 MiB，
     * 那样缺页次数就对不上 4 KiB 的页数了 */
    if (prctl(PR_SET_THP_DISABLE, 1, 0, 0, 0) == -1) {
        perror("prctl");
        return EXIT_FAILURE;
    }

    struct sample a = take();

    char *p = malloc(SIZE);
    if (p == NULL) {
        perror("malloc");
        return EXIT_FAILURE;
    }
    struct sample b = take();

    for (size_t off = 0; off < SIZE; off += (size_t)page) {
        p[off] = 1;
    }
    struct sample c = take();

    report("启动时", a);
    report("malloc 32 MiB 返回后", b);
    report("每页写 1 字节后", c);
    printf("页大小 %ld，共 %lu 页；写入阶段 minflt 增加 %ld\n", page,
           SIZE / (unsigned long)page, c.minflt - b.minflt);
    free(p);
    return 0;
}
