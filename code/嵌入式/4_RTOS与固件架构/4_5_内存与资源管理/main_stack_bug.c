/*
 * main_stack_bug.c：演示为什么不能把 main() 中的自动存储期对象地址传给任务。
 *
 * 在 Cortex-M3 移植层中，vTaskStartScheduler() 启动第一个任务时会从向量表首项
 * 重新加载 MSP（重置回 _estack），main() 的栈帧随即被后续中断覆盖。
 */
#include "FreeRTOS.h"
#include "task.h"
#include <stdint.h>

typedef struct {
    uint32_t baudrate;
    uint16_t sample_period_ms;
    uint8_t  device_id;
} app_cfg_t;

static void worker_task(void *arg) {
    const app_cfg_t *cfg = (const app_cfg_t *)arg;
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(cfg->sample_period_ms));
    }
}

void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName) {
    (void)xTask;
    (void)pcTaskName;
    for (;;) {
    }
}

#ifdef SHOW_BUG
int main(void) {
    /* 错误：cfg 位于 main() 的主栈帧上，调度器启动后会被 SysTick 等中断覆写 */
    app_cfg_t cfg = {115200u, 20u, 0x01u};
    xTaskCreate(worker_task, "worker", 128, &cfg, 2, NULL);
    vTaskStartScheduler();
    for (;;) {
    }
}
#else
int main(void) {
    /* 正确：static 对象位于 .data 段，生命周期贯穿整个程序运行期 */
    static const app_cfg_t cfg = {115200u, 20u, 0x01u};
    xTaskCreate(worker_task, "worker", 128, (void *)&cfg, 2, NULL);
    vTaskStartScheduler();
    for (;;) {
    }
}
#endif
