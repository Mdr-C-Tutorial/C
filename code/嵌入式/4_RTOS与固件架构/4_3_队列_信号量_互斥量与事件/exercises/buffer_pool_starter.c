/*
 * exercises/buffer_pool_starter.c：习题 60433 初始代码（计数信号量管理缓冲池）。
 */
#include <stdint.h>
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"
#include "../sim_support.h"

#define POOL_SIZE 4u
#define TOTAL_JOBS 8u

static uint32_t g_buffers[POOL_SIZE][16];
static uint8_t g_free_stack[POOL_SIZE] = { 0, 1, 2, 3 };
static size_t g_free_top = POOL_SIZE;

static SemaphoreHandle_t g_pool_sem;
static QueueHandle_t g_ready_queue;

/* 调用前须已通过 g_pool_sem 获得配额 */
static uint8_t pool_pop_index(void) {
    uint8_t idx;
    taskENTER_CRITICAL();
    idx = g_free_stack[--g_free_top];
    taskEXIT_CRITICAL();
    return idx;
}

/* 归还后须调用 xSemaphoreGive(g_pool_sem) 释放配额 */
static void pool_push_index(uint8_t idx) {
    taskENTER_CRITICAL();
    g_free_stack[g_free_top++] = idx;
    taskEXIT_CRITICAL();
}

static void producer_task(void *param) {
    (void)param;
    for (unsigned job = 1; job <= TOTAL_JOBS; ++job) {
        /* TODO:
         * 1. 用 xSemaphoreTake(g_pool_sem, portMAX_DELAY) 申请配额；
         * 2. 调用 pool_pop_index() 取得缓冲区编号 idx，往 g_buffers[idx][0] 写入 job；
         * 3. 把 idx 发送到 g_ready_queue。
         */
        (void)pool_pop_index;
    }
    for (;;) {
        vTaskDelay(portMAX_DELAY);
    }
}

static void consumer_task(void *param) {
    (void)param;
    for (unsigned job = 1; job <= TOTAL_JOBS; ++job) {
        uint8_t idx = 0;
        /* TODO:
         * 1. 从 g_ready_queue 接收 idx；
         * 2. 读取 g_buffers[idx][0] 并打印；
         * 3. 调用 pool_push_index(idx) 归还槽位，并 xSemaphoreGive(g_pool_sem)。
         */
        (void)idx;
        (void)pool_push_index;
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    sim_finish();
}

int main(void) {
    g_pool_sem = xSemaphoreCreateCounting(POOL_SIZE, POOL_SIZE);
    g_ready_queue = xQueueCreate(POOL_SIZE, sizeof(uint8_t));
    if (g_pool_sem == NULL || g_ready_queue == NULL) {
        return 1;
    }

    xTaskCreate(producer_task, "prod", 256, NULL, 2, NULL);
    xTaskCreate(consumer_task, "cons", 256, NULL, 1, NULL);

    vTaskStartScheduler();
    return 0;
}
