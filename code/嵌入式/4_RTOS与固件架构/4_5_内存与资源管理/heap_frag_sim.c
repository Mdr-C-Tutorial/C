/*
 * heap_frag_sim.c：在模拟器上演示 heap_4 的相邻块合并与交替释放导致的碎片。
 *
 * 先把堆按“128 B 短命块 + 64 B 长命块”交替占满，再释放全部短命块。
 * 这时空闲总量足够，但每个空闲块都被 64 B 块隔开，申请 600 B 连续内存失败。
 * 释放长命块后 heap_4 把相邻空闲块合并回一整块，同样的申请成功。
 *
 * 讲解见教程 4.5 节「内存与资源管理」。
 */
#include <stdio.h>
#include <stdlib.h>

#include "FreeRTOS.h"
#include "task.h"

#define PAIR_MAX 64

static void print_heap_status(const char *stage) {
    printf("[%s] 当前空闲 %lu B，历史最低空闲 %lu B\n", stage,
           (unsigned long)xPortGetFreeHeapSize(),
           (unsigned long)xPortGetMinimumEverFreeHeapSize());
    fflush(stdout);
}

static void demo_task(void *arg) {
    (void)arg;
    static void *short_lived[PAIR_MAX];
    static void *long_lived[PAIR_MAX];
    size_t pairs = 0;

    print_heap_status("1. 初始：demo 任务已创建");

    /* 交替申请，一直占到堆再也放不下一对为止 */
    for (size_t i = 0; i < PAIR_MAX; ++i) {
        void *s = pvPortMalloc(128);
        void *l = pvPortMalloc(64);
        if (s == NULL || l == NULL) {
            if (s != NULL) {
                vPortFree(s);
            }
            if (l != NULL) {
                vPortFree(l);
            }
            break;
        }
        short_lived[i] = s;
        long_lived[i] = l;
        pairs++;
    }
    printf("   交替占满堆：共 %lu 对 (128 B + 64 B)\n", (unsigned long)pairs);
    print_heap_status("2. 堆已占满");

    /* 释放全部 128 B 块，64 B 块仍夹在它们中间，阻止相邻合并 */
    for (size_t i = 0; i < pairs; ++i) {
        vPortFree(short_lived[i]);
        short_lived[i] = NULL;
    }
    print_heap_status("3. 释放全部 128 B 块");

    /* 空闲总量远超 600 B，但最大的连续块只有 128 B 出头 */
    void *big = pvPortMalloc(600);
    printf("   -> 申请 600 B 连续块: %s\n", big != NULL ? "成功" : "失败（返回 NULL）");
    fflush(stdout);

    /* 再释放夹在中间的 64 B 块，heap_4 把相邻空闲块合并回一整块 */
    for (size_t i = 0; i < pairs; ++i) {
        vPortFree(long_lived[i]);
        long_lived[i] = NULL;
    }
    print_heap_status("4. 释放全部 64 B 块，相邻空闲块合并");

    big = pvPortMalloc(600);
    printf("   -> 再次申请 600 B 连续块: %s\n", big != NULL ? "成功" : "失败（返回 NULL）");
    fflush(stdout);

    vPortFree(big);
    exit(0);
}

int main(void) {
    xTaskCreate(demo_task, "demo", 512, NULL, 1, NULL);
    vTaskStartScheduler();
    return 0;
}