/*
 * exercises/gatekeeper_msgbuf_starter.c：习题 60435 初始代码（消息缓冲区守门任务）。
 */
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "message_buffer.h"
#include "../sim_support.h"

#define LOG_BUF_BYTES 256u
#define LINE_MAX_LEN  80u

static MessageBufferHandle_t g_log_msgbuf;
static SemaphoreHandle_t g_write_mutex;

static void log_print(const char *fmt, ...) {
    char buf[LINE_MAX_LEN];
    va_list args;
    va_start(args, fmt);
    int n = vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    if (n <= 0) {
        return;
    }
    size_t len = (size_t)n < sizeof(buf) ? (size_t)n : sizeof(buf) - 1u;

    /* TODO: 用 g_write_mutex 保护 xMessageBufferSend，把 buf（长度 len）写入 g_log_msgbuf */
    (void)len;
}

static void gatekeeper_task(void *param) {
    (void)param;
    char rx_buf[LINE_MAX_LEN];
    unsigned count = 0;

    for (;;) {
        /* TODO: 调用 xMessageBufferReceive 接收一条变长消息，补 '\0' 后打印 */
        (void)rx_buf;
        count++;
        if (count >= 6u) {
            sim_finish();
        }
    }
}

static void worker_task(void *param) {
    const char *name = (const char *)param;
    for (unsigned i = 1; i <= 3; ++i) {
        log_print("[%s] variable-length log message #%u", name, i);
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    for (;;) {
        vTaskDelay(portMAX_DELAY);
    }
}

int main(void) {
    g_log_msgbuf = xMessageBufferCreate(LOG_BUF_BYTES);
    g_write_mutex = xSemaphoreCreateMutex();
    if (g_log_msgbuf == NULL || g_write_mutex == NULL) {
        return 1;
    }

    xTaskCreate(gatekeeper_task, "gate",   256, NULL,            1, NULL);
    xTaskCreate(worker_task,     "sensor", 256, (void *)"sensor", 2, NULL);
    xTaskCreate(worker_task,     "net",    256, (void *)"net",    2, NULL);

    vTaskStartScheduler();
    return 0;
}
