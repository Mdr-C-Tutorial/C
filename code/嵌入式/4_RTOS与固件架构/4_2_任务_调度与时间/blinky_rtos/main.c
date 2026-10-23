/*
 * main.c：STM32F103C8 上的两个任务：一个闪 PC13 的 LED，一个经 USART1 打印。
 *
 * 控制台只在打印任务里使用，串口没有并发访问，所以这里不做额外保护；
 * 多个任务共用一个串口的做法见 4.3 节。
 */
#include <stdint.h>

#include "FreeRTOS.h"
#include "task.h"
#include "rtos_console.h"
#include "board.h"

static void blink_task(void *param) {
    (void)param;

    for (;;) {
        led_toggle();
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

static void report_task(void *param) {
    (void)param;

    for (;;) {
        console_printf("uptime %u ms, heap free %u bytes\n",
                       (unsigned)(xTaskGetTickCount() * portTICK_PERIOD_MS),
                       (unsigned)xPortGetFreeHeapSize());
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

int main(void) {
    board_init();
    console_init();

    xTaskCreate(blink_task, "blink", 128, NULL, 1, NULL);
    xTaskCreate(report_task, "report", 256, NULL, 2, NULL);

    vTaskStartScheduler();
    return 0;
}
