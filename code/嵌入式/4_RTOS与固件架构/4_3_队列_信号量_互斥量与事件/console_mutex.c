/*
 * console_mutex.c：对比不加保护与使用互斥量保护时的多任务串口输出。
 *
 * 讲解见教程 4.3 节「队列、信号量、互斥量与事件」。
 */
#include <stdint.h>
#include <stdio.h>
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "sim_support.h"

static SemaphoreHandle_t g_uart_mutex;
static SemaphoreHandle_t g_done_sem;
static volatile int g_use_mutex;

/* 模拟低速串口分段发送一整行文本，中间发生任务切换 */
static void slow_uart_puts(const char *tag, const char *body, unsigned seq) {
    console_printf("[%s #%u] ", tag, seq);
    vTaskDelay(pdMS_TO_TICKS(2));
    console_printf("%s ", body);
    vTaskDelay(pdMS_TO_TICKS(2));
    console_printf("(status=OK)\n");
}

static void print_line(const char *tag, const char *body, unsigned seq) {
    if (g_use_mutex) {
        if (xSemaphoreTake(g_uart_mutex, portMAX_DELAY) == pdTRUE) {
            slow_uart_puts(tag, body, seq);
            xSemaphoreGive(g_uart_mutex);
        }
    } else {
        slow_uart_puts(tag, body, seq);
    }
}

static void worker_a_task(void *param) {
    (void)param;
    for (unsigned i = 1; i <= 2; ++i) {
        print_line("TaskA", "IMU_ACCEL_X=12mg_Y=-18mg_Z=998mg", i);
    }
    xSemaphoreGive(g_done_sem);

    vTaskSuspend(NULL);
}

static void worker_b_task(void *param) {
    (void)param;
    for (unsigned i = 1; i <= 2; ++i) {
        print_line("TaskB", "NET_MQTT_PUB_TOPIC=mdr/dev01/tel", i);
    }
    xSemaphoreGive(g_done_sem);

    vTaskSuspend(NULL);
}

static void coordinator_task(void *param) {
    (void)param;
    TaskHandle_t ha = NULL;
    TaskHandle_t hb = NULL;

    console_printf("=== 1. Unprotected output ===\n");
    g_use_mutex = 0;
    xTaskCreate(worker_a_task, "wa1", 256, NULL, 1, &ha);
    xTaskCreate(worker_b_task, "wb1", 256, NULL, 1, &hb);
    xSemaphoreTake(g_done_sem, portMAX_DELAY);
    xSemaphoreTake(g_done_sem, portMAX_DELAY);
    vTaskDelete(ha);
    vTaskDelete(hb);

    console_printf("=== 2. Mutex-protected output ===\n");
    g_use_mutex = 1;
    xTaskCreate(worker_a_task, "wa2", 256, NULL, 1, &ha);
    xTaskCreate(worker_b_task, "wb2", 256, NULL, 1, &hb);
    xSemaphoreTake(g_done_sem, portMAX_DELAY);
    xSemaphoreTake(g_done_sem, portMAX_DELAY);
    vTaskDelete(ha);
    vTaskDelete(hb);

    sim_finish();
}

int main(void) {
    g_uart_mutex = xSemaphoreCreateMutex();
    g_done_sem = xSemaphoreCreateCounting(2, 0);
    if (g_uart_mutex == NULL || g_done_sem == NULL) {
        return 1;
    }

    if (xTaskCreate(coordinator_task, "coord", 256, NULL, 2, NULL) != pdPASS) {
        return 1;
    }

    vTaskStartScheduler();
    return 0;
}
