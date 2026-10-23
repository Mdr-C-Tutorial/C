/*
 * starvation.c：高优先级任务忙等时，低优先级任务完全得不到运行。
 *
 * 用编译期开关 BUSY_WAIT 切换两种写法：
 *   1：高优先级任务原地忙等，不让出 CPU
 *   0：高优先级任务用 vTaskDelay 阻塞，让低优先级任务运行
 * 构建时加 -DBUSY_WAIT=1 或 -DBUSY_WAIT=0，见 CMakeLists.txt 里的两个目标。
 */
#include <stdint.h>

#include "FreeRTOS.h"
#include "task.h"
#include "rtos_console.h"

#ifndef BUSY_WAIT
#define BUSY_WAIT 1
#endif

static volatile uint32_t g_spin_count;

static void high_task(void *param) {
    (void)param;

    console_printf("[high] start, priority %u\n",
                   (unsigned)uxTaskPriorityGet(NULL));

    for (uint32_t round = 1; round <= 3; ++round) {
        /* 用一段有界的空循环模拟“要算 20 ms”的活 */
        for (uint32_t i = 0; i < 200000u; ++i) {
            g_spin_count += i;
        }
        console_printf("[high] round %u done\n", (unsigned)round);

#if BUSY_WAIT
        /* 忙等：时间到了继续占着 CPU，低优先级任务拿不到时间片 */
        TickType_t start = xTaskGetTickCount();
        while ((TickType_t)(xTaskGetTickCount() - start) < pdMS_TO_TICKS(20)) {
        }
#else
        /* 阻塞：让出 CPU，低优先级任务得以运行 */
        vTaskDelay(pdMS_TO_TICKS(20));
#endif
    }

    sim_finish();
    for (;;) {
        vTaskDelay(portMAX_DELAY);
    }
}

static void low_task(void *param) {
    (void)param;
    uint32_t ticks = 0;

    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(10));
        ticks += 10;
        console_printf("[low ] alive after %u ms\n", (unsigned)ticks);
    }
}

int main(void) {
    console_init();

    xTaskCreate(low_task, "low", 256, NULL, 1, NULL);
    xTaskCreate(high_task, "high", 256, NULL, 2, NULL);

    vTaskStartScheduler();
    return 0;
}
