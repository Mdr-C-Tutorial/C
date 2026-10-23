/*
 * button_semaphore.c：用二值信号量把“按键按下”事件从检测端传给处理任务。
 *
 * 讲解见教程 4.3 节「队列、信号量、互斥量与事件」。
 */
#include <stdint.h>
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "sim_support.h"

static SemaphoreHandle_t g_button_sem;

static void button_sim_task(void *param) {
    (void)param;

    for (unsigned i = 1; i <= 3; ++i) {
        vTaskDelay(pdMS_TO_TICKS(20));
        console_printf("[button]  press #%u -> xSemaphoreGive\n", i);
        xSemaphoreGive(g_button_sem);
    }

    for (;;) {
        vTaskDelay(portMAX_DELAY);
    }
}

static void handler_task(void *param) {
    (void)param;

    for (unsigned i = 1; i <= 3; ++i) {
        if (xSemaphoreTake(g_button_sem, portMAX_DELAY) == pdTRUE) {
            console_printf("[handler] woke up, handling press #%u\n", i);
        }
    }

    sim_finish();
}

int main(void) {
    g_button_sem = xSemaphoreCreateBinary();
    if (g_button_sem == NULL) {
        return 1;
    }

    if (xTaskCreate(handler_task, "handler", 256, NULL, 2, NULL) != pdPASS) {
        return 1;
    }
    if (xTaskCreate(button_sim_task, "button", 256, NULL, 1, NULL) != pdPASS) {
        return 1;
    }

    vTaskStartScheduler();
    return 0;
}
