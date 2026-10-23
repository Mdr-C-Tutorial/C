/*
 * deadline_monitor.c：周期任务比较实际唤醒时刻与应唤醒时刻，超期就计数。
 *
 * 讲解见教程 4.6 节「优先级反转、死锁与实时性」。
 *
 * 一个 20 ms 周期的任务，前 6 轮每轮做 10 ms 计算，之后改成 25 ms。
 * 计算时间一超过周期，下一轮就赶不上点：xTaskDelayUntil 的基准仍按周期
 * 前进，实际唤醒时刻却越来越晚，差值就是要统计的超期时间。
 */
#include <stdint.h>

#include "FreeRTOS.h"
#include "task.h"

#include "rtos_console.h"

#define PERIOD_MS 20
#define CYCLES    10

static void busy_ms(uint32_t ms) {
    TickType_t start = xTaskGetTickCount();
    while ((xTaskGetTickCount() - start) < pdMS_TO_TICKS(ms)) {
    }
}

static void periodic_task(void *param) {
    (void)param;
    TickType_t last_wake = xTaskGetTickCount();
    uint32_t total_late = 0;
    unsigned overruns = 0;

    for (unsigned i = 0; i < CYCLES; ++i) {
        /* 第 0～5 轮做 10 ms，之后做 25 ms：超过周期，必然超期 */
        uint32_t work = (i < 6u) ? 10u : 25u;
        busy_ms(work);

        TickType_t expected = last_wake + pdMS_TO_TICKS(PERIOD_MS);
        TickType_t now = xTaskGetTickCount();

        /* 实际唤醒时刻减去应唤醒时刻，正数表示超期 */
        int32_t late = (int32_t)(now - expected);
        if (late > 0) {
            overruns++;
            total_late += (uint32_t)late;
            console_printf("第 %2u 轮：应于 tick %3lu 醒来，实际 tick %3lu，超期 %ld ms\n",
                           i + 1u, (unsigned long)expected, (unsigned long)now,
                           (long)late);
        }

        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(PERIOD_MS));
    }

    console_printf("结束：%u 轮超期，累计超期 %lu ms\n", overruns,
                   (unsigned long)total_late);
    sim_finish();
}

int main(void) {
    if (xTaskCreate(periodic_task, "periodic", 256, NULL, 2, NULL) != pdPASS) {
        return 1;
    }
    vTaskStartScheduler();
    return 0;
}