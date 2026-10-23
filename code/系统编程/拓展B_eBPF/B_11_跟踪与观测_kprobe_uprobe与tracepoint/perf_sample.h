#ifndef PERF_SAMPLE_H
#define PERF_SAMPLE_H

/* 内核态与用户态共用的样本结构。只使用定长类型，
 * 两边看到的内存布局一致。栈深度取 8 层就够画一张火焰图的骨架。 */
#define MAX_STACK_DEPTH 8

struct stack_sample {
    __u32 pid;
    __u32 cpu;
    __s32 kstack_sz;
    __u8 comm[16];
    __u64 kstack[MAX_STACK_DEPTH];
};

#endif