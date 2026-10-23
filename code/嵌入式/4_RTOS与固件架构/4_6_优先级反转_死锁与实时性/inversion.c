/*
 * inversion.c：用二值信号量或互斥量保护同一段临界区，比较高优先级任务的等待时间。
 *
 * 讲解见教程 4.6 节「优先级反转、死锁与实时性」。
 *
 * 三个任务，只跑一轮：
 *   L（优先级 1）启动后立刻取锁，在临界区里做 L_ROUNDS 轮计算，然后释放；
 *   H（优先级 3）在 10 ms 时请求同一把锁；
 *   M（优先级 2）在 30 ms 时做 M_ROUNDS 轮与锁无关的计算。
 *
 * 计算用固定轮数的忙循环，而不是按墙上时间等待：被抢占时它不前进，
 * 所以抢占会实实在在推迟任务的完成时刻。轮数只是“工作量”的单位，
 * 换成毫秒要看具体机器，程序里打印的是节拍。
 *
 * 编译时用 -DUSE_MUTEX=1 选择互斥量，默认用二值信号量。
 */
#include <stdint.h>

#include "FreeRTOS.h"
#include "semphr.h"
#include "task.h"

#include "rtos_console.h"

#ifndef USE_MUTEX
#define USE_MUTEX 0
#endif

#define L_ROUNDS 250000u
#define M_ROUNDS 100000u
#define ROUND_WORK 1000u

#define H_OFFSET 10
#define M_OFFSET 30

static SemaphoreHandle_t g_lock;

/* 固定轮数的纯计算；被抢占时循环停在原地，不会因为时间流逝而自动完成 */
static void work_rounds(uint32_t rounds) {
    volatile uint32_t acc = 0;
    for (uint32_t r = 0; r < rounds; ++r) {
        for (uint32_t i = 0; i < ROUND_WORK; ++i) {
            acc += i;
        }
    }
}

static void low_task(void *param) {
    (void)param;

    xSemaphoreTake(g_lock, portMAX_DELAY);
    TickType_t begin = xTaskGetTickCount();
    work_rounds(L_ROUNDS);
    TickType_t end = xTaskGetTickCount();

    console_printf("[L] tick %lu 取锁，tick %lu 做完临界区里的计算\n",
                   (unsigned long)begin, (unsigned long)end);

    xSemaphoreGive(g_lock);

    for (;;) {
        vTaskDelay(portMAX_DELAY);
    }
}

static void medium_task(void *param) {
    (void)param;

    vTaskDelay(pdMS_TO_TICKS(M_OFFSET));
    TickType_t begin = xTaskGetTickCount();
    console_printf("[M] tick %lu 醒来，开始与锁无关的计算\n", (unsigned long)begin);
    work_rounds(M_ROUNDS);
    TickType_t end = xTaskGetTickCount();

    console_printf("[M] tick %lu 计算结束\n", (unsigned long)end);

    for (;;) {
        vTaskDelay(portMAX_DELAY);
    }
}

static void high_task(void *param) {
    (void)param;

    vTaskDelay(pdMS_TO_TICKS(H_OFFSET));
    TickType_t request = xTaskGetTickCount();
    xSemaphoreTake(g_lock, portMAX_DELAY);
    TickType_t acquired = xTaskGetTickCount();

    console_printf("[H] tick %lu 请求锁，tick %lu 拿到，等待 %lu 个节拍\n",
                   (unsigned long)request, (unsigned long)acquired,
                   (unsigned long)(acquired - request));

    xSemaphoreGive(g_lock);

    /* 留一段时间给 M 跑完，好从输出里看出 M 的计算被推到什么时候 */
    vTaskDelay(pdMS_TO_TICKS(150));
    sim_finish();
}

int main(void) {
#if USE_MUTEX
    g_lock = xSemaphoreCreateMutex();
    console_printf("=== 用互斥量保护（优先级继承生效）===\n");
#else
    g_lock = xSemaphoreCreateBinary();
    console_printf("=== 用二值信号量保护（没有优先级继承）===\n");
    xSemaphoreGive(g_lock);
#endif
    if (g_lock == NULL) {
        return 1;
    }

    if (xTaskCreate(low_task, "L", 256, NULL, 1, NULL) != pdPASS) {
        return 1;
    }
    if (xTaskCreate(medium_task, "M", 256, NULL, 2, NULL) != pdPASS) {
        return 1;
    }
    if (xTaskCreate(high_task, "H", 256, NULL, 3, NULL) != pdPASS) {
        return 1;
    }

    vTaskStartScheduler();
    return 0;
}