/*
 * rta.c：单调速率调度的利用率检查与响应时间分析。
 *
 * 讲解见教程 4.6 节「优先级反转、死锁与实时性」。
 *
 * 用法：
 *   rta            用内置的三任务例子
 *   rta tasks.txt  从文件读任务集
 *
 * 文件里每行 "C T B"，单位 ms，按周期从短到长排（周期最短的优先级最高）。
 * C 是最坏情况执行时间，T 是周期（截止时间等于周期），B 是被低优先级任务阻塞的时间。
 *
 * 编译：gcc -std=c17 -Wall -Wextra rta.c -o rta -lm
 */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#define MAX_TASKS 32

struct task {
    double c;
    double t;
    double b;
};

static struct task g_tasks[MAX_TASKS];

/* 累加所有任务的利用率 */
static double utilization(int n) {
    double u = 0.0;
    for (int i = 0; i < n; ++i) {
        u += g_tasks[i].c / g_tasks[i].t;
    }
    return u;
}

/* Liu 与 Layland 上界 n(2^(1/n) - 1) */
static double ll_bound(int n) {
    return n * (pow(2.0, 1.0 / n) - 1.0);
}

/* 迭代 R = C + B + sum(ceil(R / Tj) * Cj)，返回不动点；超过截止时间返回 -1 */
static double response_time(int index) {
    double c = g_tasks[index].c;
    double b = g_tasks[index].b;
    double t = g_tasks[index].t;
    double r = c + b;

    printf("  任务 %d（C=%.1f T=%.1f B=%.1f）：\n", index, c, t, b);
    for (int round = 1; round <= 20; ++round) {
        double sum = 0.0;
        for (int j = 0; j < index; ++j) {
            double ratio = r / g_tasks[j].t;
            double ceil_ratio = (double)(long long)(ratio);
            if (ceil_ratio < ratio) {
                ceil_ratio += 1.0;
            }
            sum += ceil_ratio * g_tasks[j].c;
        }
        double next = c + b + sum;
        printf("    第 %2d 轮：R = %.1f + %.1f + %.1f = %.1f\n", round, c, b, sum,
               next);
        if (next == r) {
            return next > t ? -1.0 : next;
        }
        if (next > t) {
            return -1.0;
        }
        r = next;
    }
    return -1.0;
}

/* 从文件读任务集；读不到时返回 0，由调用者换用内置例子 */
static int read_tasks(const char *path) {
    FILE *fp = fopen(path, "r");
    int n = 0;

    if (fp == NULL) {
        fprintf(stderr, "打不开 %s\n", path);
        return 0;
    }
    while (n < MAX_TASKS) {
        double c;
        double t;
        double b;
        if (fscanf(fp, "%lf %lf %lf", &c, &t, &b) != 3) {
            break;
        }
        g_tasks[n].c = c;
        g_tasks[n].t = t;
        g_tasks[n].b = b;
        n++;
    }
    fclose(fp);
    return n;
}

int main(int argc, char **argv) {
    int n = 0;

    if (argc > 1) {
        n = read_tasks(argv[1]);
        if (n == 0) {
            return 2;
        }
    } else {
        /* 内置例子，按周期从短到长 */
        static const struct task defaults[] = {
            { 1.0, 4.0, 0.0 },
            { 2.0, 6.0, 0.0 },
            { 3.0, 12.0, 0.0 },
        };
        n = (int)(sizeof defaults / sizeof defaults[0]);
        for (int i = 0; i < n; ++i) {
            g_tasks[i] = defaults[i];
        }
    }

    double u = utilization(n);
    double bound = ll_bound(n);

    printf("任务数 %d，总利用率 U = %.3f，Liu-Layland 上界 %.3f\n", n, u, bound);
    printf("利用率%s上界：%s\n", u <= bound ? "不超过" : "超过",
           u <= bound ? "上界保证可调度" : "需要进一步做响应时间分析");

    int ok = 1;
    for (int i = 0; i < n; ++i) {
        double r = response_time(i);
        if (r < 0.0) {
            printf("  -> 无法在截止时间 %.1f 内完成\n", g_tasks[i].t);
            ok = 0;
        } else {
            printf("  -> 响应时间 %.1f <= 截止时间 %.1f\n", r, g_tasks[i].t);
        }
    }
    printf("结论：%s\n", ok ? "全部任务可调度" : "存在任务会超期");
    return ok ? 0 : 1;
}