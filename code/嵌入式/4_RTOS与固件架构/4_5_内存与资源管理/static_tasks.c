/*
 * static_tasks.c：使用 xTaskCreateStatic 与 xQueueCreateStatic 构建全静态系统。
 *
 * 通过宏 USE_DYNAMIC_ALLOC 可切换动态与静态创建方式，用于对比目标构建的 .bss 大小；
 * 在主机模拟器上直接运行可观察采样与消费过程。
 */
#include "FreeRTOS.h"
#include "queue.h"
#include "task.h"
#include <stdint.h>
#ifndef __ARM_ARCH
#include <stdio.h>
#include <stdlib.h>
#endif

typedef struct {
    TickType_t tick;
    int16_t ax;
    int16_t ay;
    int16_t az;
} sample_t;

#define SAMPLE_QUEUE_LEN 8

static QueueHandle_t g_sample_q;

#if !defined(USE_DYNAMIC_ALLOC) || (USE_DYNAMIC_ALLOC == 0)
/* 静态队列控制块与存储区 */
static StaticQueue_t g_sample_q_struct;
static uint8_t g_sample_q_storage[SAMPLE_QUEUE_LEN * sizeof(sample_t)];

/* 两个任务的静态栈与静态 TCB */
static StackType_t g_producer_stack[128];
static StaticTask_t g_producer_tcb;

static StackType_t g_consumer_stack[128];
static StaticTask_t g_consumer_tcb;
#endif

static void producer_task(void *arg) {
    (void)arg;
    static const int16_t table[4][3] = {
        {10, -20, 1002},
        {14, -18, 998},
        {8, -22, 1005},
        {12, -19, 1000}
    };
    for (uint32_t i = 0; i < 4u; ++i) {
        sample_t s = {
            .tick = xTaskGetTickCount(),
            .ax = table[i][0],
            .ay = table[i][1],
            .az = table[i][2]
        };
        xQueueSend(g_sample_q, &s, portMAX_DELAY);
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

static void consumer_task(void *arg) {
    (void)arg;
    sample_t s;
    for (uint32_t count = 1u; count <= 4u; ++count) {
        if (xQueueReceive(g_sample_q, &s, portMAX_DELAY) == pdPASS) {
#ifndef __ARM_ARCH
            taskENTER_CRITICAL();
            printf("[consumer] #%u tick=%lu ax=%d ay=%d az=%d\n",
                   (unsigned)count, (unsigned long)s.tick,
                   (int)s.ax, (int)s.ay, (int)s.az);
            fflush(stdout);
            taskEXIT_CRITICAL();
#else
            (void)s;
#endif
        }
    }
#ifndef __ARM_ARCH
    exit(0);
#else
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
#endif
}

#ifdef __ARM_ARCH
void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName) {
    (void)xTask;
    (void)pcTaskName;
    for (;;) {
    }
}
#endif

int main(void) {
#if defined(USE_DYNAMIC_ALLOC) && (USE_DYNAMIC_ALLOC != 0)
    g_sample_q = xQueueCreate(SAMPLE_QUEUE_LEN, sizeof(sample_t));
    xTaskCreate(producer_task, "prod", 128, NULL, 3, NULL);
    xTaskCreate(consumer_task, "cons", 128, NULL, 2, NULL);
#else
    g_sample_q = xQueueCreateStatic(SAMPLE_QUEUE_LEN, sizeof(sample_t),
                                    g_sample_q_storage, &g_sample_q_struct);
    xTaskCreateStatic(producer_task, "prod", 128, NULL, 3,
                      g_producer_stack, &g_producer_tcb);
    xTaskCreateStatic(consumer_task, "cons", 128, NULL, 2,
                      g_consumer_stack, &g_consumer_tcb);
#endif
    vTaskStartScheduler();
    for (;;) {
    }
}
