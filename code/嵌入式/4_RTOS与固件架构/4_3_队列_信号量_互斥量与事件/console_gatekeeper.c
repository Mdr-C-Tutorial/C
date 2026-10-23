/*
 * console_gatekeeper.c：用守门任务（gatekeeper task）集中管理控制台输出。
 *
 * 讲解见教程 4.3 节「队列、信号量、互斥量与事件」。
 */
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "sim_support.h"

#define LOG_MSG_MAX 64

typedef struct {
    char text[LOG_MSG_MAX];
} log_msg_t;

static QueueHandle_t g_log_queue;

static void log_print(const char *fmt, ...) {
    log_msg_t msg;
    va_list args;
    va_start(args, fmt);
    vsnprintf(msg.text, sizeof(msg.text), fmt, args);
    va_end(args);
    xQueueSend(g_log_queue, &msg, pdMS_TO_TICKS(50));
}

static void console_gatekeeper_task(void *param) {
    (void)param;
    unsigned printed = 0;

    for (;;) {
        log_msg_t msg;
        if (xQueueReceive(g_log_queue, &msg, portMAX_DELAY) == pdPASS) {
            console_printf("%s\n", msg.text);
            printed++;
            if (printed >= 6u) {
                sim_finish();
            }
        }
    }
}

static void worker_task(void *param) {
    const char *name = (const char *)param;
    for (unsigned i = 1; i <= 3; ++i) {
        log_print("[%s] step %u completed", name, i);
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    for (;;) {
        vTaskDelay(portMAX_DELAY);
    }
}

int main(void) {
    g_log_queue = xQueueCreate(8, sizeof(log_msg_t));
    if (g_log_queue == NULL) {
        return 1;
    }

    if (xTaskCreate(console_gatekeeper_task, "gate", 256, NULL, 1, NULL) != pdPASS) {
        return 1;
    }
    if (xTaskCreate(worker_task, "sensor", 256, (void *)"sensor", 2, NULL) != pdPASS) {
        return 1;
    }
    if (xTaskCreate(worker_task, "net", 256, (void *)"net   ", 2, NULL) != pdPASS) {
        return 1;
    }

    vTaskStartScheduler();
    return 0;
}
