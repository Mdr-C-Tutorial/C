/*
 * rtos_tasks.c：同一需求用三个 FreeRTOS 任务实现。
 *
 * 任务函数与 xTaskCreate 的用法在 4.2 节展开；这里只负责与 superloop.c 对照。
 * 目标构建的编译检查：
 *   arm-none-eabi-gcc -std=c11 -mcpu=cortex-m3 -mthumb -O2 -Wall -Wextra \
 *       -DSTM32F103xB -I../../common/freertos -I<FreeRTOS-Kernel>/include \
 *       -I<FreeRTOS-Kernel>/portable/GCC/ARM_CM3 \
 *       -include ../../common/freertos/FreeRTOSConfig_stm32.h -c rtos_tasks.c
 */
#include <stdint.h>

#include "FreeRTOS.h"
#include "task.h"

/* 板级与驱动接口，实现见各节的目标工程 */
void led_toggle(void);
void sensor_start_conv(void);
int16_t sensor_read_raw(void);
void filter_push(int16_t sample);
uint8_t uart_blocking_getc(void);
void cmd_feed_byte(uint8_t byte);

/* 高优先级：固定 10 ms 周期的采样，转换期间阻塞等待完成信号 */
static void sensor_task(void *param) {
    (void)param;
    TickType_t last_wake = xTaskGetTickCount();

    for (;;) {
        sensor_start_conv();
        /* 等转换完成（驱动内部用信号量或任务通知实现，见 4.3、4.4） */
        filter_push(sensor_read_raw());
        xTaskDelayUntil(&last_wake, pdMS_TO_TICKS(10));
    }
}

/* 中优先级：没有字节时阻塞在串口上，不占 CPU */
static void cmd_task(void *param) {
    (void)param;

    for (;;) {
        uint8_t ch = uart_blocking_getc();
        cmd_feed_byte(ch);
    }
}

/* 低优先级：每 500 ms 翻转一次 LED */
static void led_task(void *param) {
    (void)param;

    for (;;) {
        led_toggle();
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

void app_start_tasks(void) {
    xTaskCreate(sensor_task, "sensor", 256, NULL, 3, NULL);
    xTaskCreate(cmd_task, "cmd", 256, NULL, 2, NULL);
    xTaskCreate(led_task, "led", 256, NULL, 1, NULL);

    vTaskStartScheduler();
}
