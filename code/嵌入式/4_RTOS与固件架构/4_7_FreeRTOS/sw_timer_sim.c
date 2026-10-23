/*
 * sw_timer_sim.c：软件定时器做心跳 LED 与按键去抖。
 *
 * 心跳：自动重载定时器每 500 ms 翻转一次 LED。
 * 去抖：按键由模拟中断产生，每个边沿重置单次定时器，20 ms 内没有新边沿才判定按下。
 *
 * 模拟器上运行：
 *   cmake -B build -G Ninja -DFREERTOS_KERNEL_PATH=<FreeRTOS-Kernel 目录>
 *   cmake --build build
 *   ./build/sw_timer_sim
 */
#include <stdint.h>

#include "FreeRTOS.h"
#include "task.h"
#include "timers.h"
#include "portmacro.h"
#include "rtos_console.h"

/* Windows 移植层给应用留的模拟中断号从 2 开始（0 是让出，1 是节拍） */
#define BUTTON_IRQ_APP 2u

static TimerHandle_t g_heartbeat_timer;
static TimerHandle_t g_debounce_timer;
static volatile int g_led_level;
static volatile uint32_t g_bounce_count;
static volatile uint32_t g_press_count;

static void heartbeat_callback(TimerHandle_t timer) {
    (void)timer;
    g_led_level = !g_led_level;
    console_printf("[heartbeat] LED -> %d (tick %lu)\n", g_led_level,
                   (unsigned long)xTaskGetTickCount());
}

static void debounce_expired(TimerHandle_t timer) {
    (void)timer;
    g_press_count++;
    console_printf("[debounce]  press accepted, count=%lu (tick %lu)\n",
                   (unsigned long)g_press_count,
                   (unsigned long)xTaskGetTickCount());
}

/* 模拟的按键中断：记录边沿，并把 20 ms 单次定时器推后 */
static uint32_t button_isr(void) {
    BaseType_t woken = pdFALSE;

    g_bounce_count++;
    xTimerResetFromISR(g_debounce_timer, &woken);
    return (uint32_t)woken;
}

/* 模拟按键任务：每组三个边沿，间隔 3 ms，模拟一次按下时的抖动 */
static void button_sim_task(void *param) {
    (void)param;

    for (unsigned round = 0; round < 3; ++round) {
        for (unsigned edge = 0; edge < 3; ++edge) {
            vPortGenerateSimulatedInterrupt(BUTTON_IRQ_APP);
            vTaskDelay(pdMS_TO_TICKS(3));
        }
        console_printf("[button]   round %u edges sent, bouncing=%lu\n", round + 1,
                       (unsigned long)g_bounce_count);
        vTaskDelay(pdMS_TO_TICKS(600));
    }

    vTaskDelay(pdMS_TO_TICKS(100));
    console_printf("[button]   done: edges=%lu presses=%lu led=%d\n",
                   (unsigned long)g_bounce_count, (unsigned long)g_press_count,
                   g_led_level);
    sim_finish();
}

int main(void) {
    console_init();

    g_heartbeat_timer = xTimerCreate("heartbeat", pdMS_TO_TICKS(500), pdTRUE, NULL,
                                     heartbeat_callback);
    g_debounce_timer = xTimerCreate("debounce", pdMS_TO_TICKS(20), pdFALSE, NULL,
                                    debounce_expired);
    if (g_heartbeat_timer == NULL || g_debounce_timer == NULL) {
        return 1;
    }
    vPortSetInterruptHandler(BUTTON_IRQ_APP, button_isr);

    if (xTimerStart(g_heartbeat_timer, 0) != pdPASS) {
        return 1;
    }
    /* 去抖定时器不在这里启动：xTimerResetFromISR 对停止中的定时器就是「启动」 */
    if (xTaskCreate(button_sim_task, "button", 512, NULL, 2, NULL) != pdPASS) {
        return 1;
    }

    vTaskStartScheduler();
    return 0;
}
