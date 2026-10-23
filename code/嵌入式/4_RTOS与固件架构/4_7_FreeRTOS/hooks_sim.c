/*
 * hooks_sim.c：空闲钩子、节拍钩子与 malloc 失败钩子的调用时机。
 *
 * 本示例自己定义全部钩子，构建时不链接 common/freertos/idle_stub.c。
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "FreeRTOS.h"
#include "task.h"
#include "rtos_console.h"

static volatile uint32_t g_idle_count;
static volatile uint32_t g_tick_count;
static volatile int g_malloc_failed;

void vApplicationIdleHook(void) {
    g_idle_count++;
}

void vApplicationTickHook(void) {
    g_tick_count++;
}

void vApplicationMallocFailedHook(void) {
    g_malloc_failed = 1;
    console_printf("[hook] vApplicationMallocFailedHook: heap left=%u\n",
                   (unsigned)xPortGetFreeHeapSize());
}

void vApplicationStackOverflowHook(TaskHandle_t task, char *name) {
    (void)task;
    console_printf("[hook] stack overflow in %s\n", name);
    for (;;) {
    }
}

static void worker(void *param) {
    (void)param;

    /* 反复申请不释放，直到堆耗尽 */
    for (int i = 0; i < 200 && !g_malloc_failed; ++i) {
        if (pvPortMalloc(1024) == NULL) {
            break;
        }
    }

    vTaskDelay(pdMS_TO_TICKS(20));
    console_printf("[worker] idle hook ran %lu times, tick hook %lu times\n",
                   (unsigned long)g_idle_count, (unsigned long)g_tick_count);
    sim_finish();
}

int main(void) {
    console_init();
    xTaskCreate(worker, "worker", 512, NULL, 2, NULL);
    vTaskStartScheduler();
    return 0;
}
