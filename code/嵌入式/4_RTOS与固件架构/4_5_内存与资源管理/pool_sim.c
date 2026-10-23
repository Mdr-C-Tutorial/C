/*
 * pool_sim.c：两个任务通过队列传递内存池里的缓冲区指针，发送方分配、接收方释放。
 *
 * 讲解见教程 4.5 节「内存与资源管理」。
 */
#include <stdint.h>

#include "FreeRTOS.h"
#include "queue.h"
#include "task.h"

#include "mem_pool.h"
#include "rtos_console.h"

#define BLOCK_SIZE  64u
#define BLOCK_COUNT 4u

static uint8_t g_storage[BLOCK_SIZE * BLOCK_COUNT];
static struct mem_pool g_pool;
static QueueHandle_t g_buf_queue;
static volatile int g_producer_done;

static void pool_lock(void) {
    taskENTER_CRITICAL();
}

static void pool_unlock(void) {
    taskEXIT_CRITICAL();
}

typedef struct {
    uint32_t seq;
    int16_t az;
} frame_t;

static void sampler_task(void *param) {
    (void)param;
    static const int16_t table[] = { 998, 1006, 1002, 994, 1010, 990 };

    for (uint32_t i = 0; i < 6u; ++i) {
        frame_t *frame = mem_pool_alloc(&g_pool);
        if (frame == NULL) {
            console_printf("[sampler] 池已空，丢弃本次采样\n");
            vTaskDelay(pdMS_TO_TICKS(10));
            continue;
        }
        frame->seq = i;
        frame->az = table[i % 6u];
        console_printf("[sampler] 分配块并发送 seq=%lu，池中剩余 %lu 块\n",
                       (unsigned long)frame->seq,
                       (unsigned long)mem_pool_free_count(&g_pool));
        xQueueSend(g_buf_queue, &frame, portMAX_DELAY);
        vTaskDelay(pdMS_TO_TICKS(10));
    }

    g_producer_done = 1;
    for (;;) {
        vTaskDelay(portMAX_DELAY);
    }
}

static void consumer_task(void *param) {
    (void)param;
    unsigned received = 0;
    unsigned dropped = 0;

    for (;;) {
        /* 消费者故意比生产者慢，池被耗尽时生产者会拿到 NULL */
        vTaskDelay(pdMS_TO_TICKS(40));

        frame_t *frame = NULL;
        if (xQueueReceive(g_buf_queue, &frame, 0) == pdPASS && frame != NULL) {
            int16_t az = frame->az;
            uint32_t seq = frame->seq;
            mem_pool_free(&g_pool, frame);
            received++;
            console_printf("[consumer] 收到 seq=%lu az=%d，已释放回池，剩余 %lu 块\n",
                           (unsigned long)seq, (int)az,
                           (unsigned long)mem_pool_free_count(&g_pool));
        }

        if (g_producer_done != 0 && uxQueueMessagesWaiting(g_buf_queue) == 0u) {
            break;
        }
    }

    (void)dropped;
    console_printf("[consumer] 结束：收到 %u 帧，池中尚余 %lu 块\n", received,
                   (unsigned long)mem_pool_free_count(&g_pool));
    sim_finish();
}

int main(void) {
    mem_pool_init(&g_pool, g_storage, BLOCK_SIZE, BLOCK_COUNT, pool_lock, pool_unlock);
    g_buf_queue = xQueueCreate(8, sizeof(frame_t *));
    if (g_buf_queue == NULL) {
        return 1;
    }

    if (xTaskCreate(consumer_task, "cons", 256, NULL, 2, NULL) != pdPASS) {
        return 1;
    }
    if (xTaskCreate(sampler_task, "samp", 256, NULL, 1, NULL) != pdPASS) {
        return 1;
    }

    vTaskStartScheduler();
    return 0;
}