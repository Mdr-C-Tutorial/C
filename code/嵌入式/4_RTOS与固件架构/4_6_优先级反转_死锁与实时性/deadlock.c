/*
 * deadlock.c：两个任务以相反顺序获取两把互斥量，双方都停住。
 *
 * 讲解见教程 4.6 节「优先级反转、死锁与实时性」。
 *
 * 三种模式，用编译选项选择：
 *   默认            任务 A 先拿 uart 再拿 flash，任务 B 先拿 flash 再拿 uart，
 *                   两者各得一把后互相等对方释放——死锁；
 *   -DWITH_TIMEOUT  每把锁只等 500 ms，超时放弃，能看出“双方都在等”；
 *   -DFIXED_ORDER   两个任务都先拿 uart 再拿 flash，死锁消失。
 *
 * 死锁版用一道“双方都拿到第一把锁”的屏障，让顺序固定下来，不依赖时序。
 */
#include <stdint.h>

#include "FreeRTOS.h"
#include "semphr.h"
#include "task.h"

#include "rtos_console.h"

#define HOLD_MS 100

static SemaphoreHandle_t g_uart_lock;
static SemaphoreHandle_t g_flash_lock;

#ifdef WITH_TIMEOUT
#define TAKE_OR_GIVE_UP(lock, tag, name)                                      \
    do {                                                                      \
        if (xSemaphoreTake((lock), pdMS_TO_TICKS(500)) != pdTRUE) {           \
            console_printf("[%s] 等 %s 超过 500 ms，放弃并退出\n", tag, name); \
            sim_finish();                                                     \
        }                                                                     \
    } while (0)
#else
#define TAKE_OR_GIVE_UP(lock, tag, name)                                      \
    do {                                                                      \
        (void)(name);                                                         \
        xSemaphoreTake((lock), portMAX_DELAY);                                \
    } while (0)
#endif

static void hold_ms(const char *tag, const char *first, const char *second) {
    console_printf("[%s] 同时持有 %s 与 %s\n", tag, first, second);
    vTaskDelay(pdMS_TO_TICKS(HOLD_MS));
    console_printf("[%s] 释放 %s 与 %s\n", tag, second, first);
}

#if defined(FIXED_ORDER)
/* 修复：所有任务按同一顺序取锁 */
static void task_a(void *param) {
    (void)param;
    TAKE_OR_GIVE_UP(g_uart_lock, "A", "uart");
    TAKE_OR_GIVE_UP(g_flash_lock, "A", "flash");
    hold_ms("A", "uart", "flash");
    xSemaphoreGive(g_flash_lock);
    xSemaphoreGive(g_uart_lock);

    console_printf("[A] 完成\n");
    vTaskDelay(pdMS_TO_TICKS(300));
    sim_finish();
}

static void task_b(void *param) {
    (void)param;
    TAKE_OR_GIVE_UP(g_uart_lock, "B", "uart");
    TAKE_OR_GIVE_UP(g_flash_lock, "B", "flash");
    hold_ms("B", "uart", "flash");
    xSemaphoreGive(g_flash_lock);
    xSemaphoreGive(g_uart_lock);

    console_printf("[B] 完成\n");
    for (;;) {
        vTaskDelay(portMAX_DELAY);
    }
}
#else
/* A 与 B 的加锁顺序相反。拿到第一把后先睡 50 ms，让另一个任务也拿到它的
 * 第一把锁，之后两者再同时去要对方手里的那一把。 */
static void task_a(void *param) {
    (void)param;
    TAKE_OR_GIVE_UP(g_uart_lock, "A", "uart");
    console_printf("[A] 拿到 uart\n");
    vTaskDelay(pdMS_TO_TICKS(50));

    TAKE_OR_GIVE_UP(g_flash_lock, "A", "flash");
    hold_ms("A", "uart", "flash");
    xSemaphoreGive(g_flash_lock);
    xSemaphoreGive(g_uart_lock);

    console_printf("[A] 完成\n");
    vTaskDelay(pdMS_TO_TICKS(300));
    sim_finish();
}

static void task_b(void *param) {
    (void)param;
    TAKE_OR_GIVE_UP(g_flash_lock, "B", "flash");
    console_printf("[B] 拿到 flash\n");
    vTaskDelay(pdMS_TO_TICKS(50));

    TAKE_OR_GIVE_UP(g_uart_lock, "B", "uart");
    hold_ms("B", "flash", "uart");
    xSemaphoreGive(g_uart_lock);
    xSemaphoreGive(g_flash_lock);

    console_printf("[B] 完成\n");
    for (;;) {
        vTaskDelay(portMAX_DELAY);
    }
}
#endif

int main(void) {
    g_uart_lock = xSemaphoreCreateMutex();
    g_flash_lock = xSemaphoreCreateMutex();
    if (g_uart_lock == NULL || g_flash_lock == NULL) {
        return 1;
    }

    if (xTaskCreate(task_a, "A", 256, NULL, 2, NULL) != pdPASS) {
        return 1;
    }
    if (xTaskCreate(task_b, "B", 256, NULL, 2, NULL) != pdPASS) {
        return 1;
    }

    vTaskStartScheduler();
    return 0;
}