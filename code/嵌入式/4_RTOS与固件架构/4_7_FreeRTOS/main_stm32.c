/*
 * main_stm32.c：FreeRTOS 接管 SysTick 后的最小 STM32F103C8 程序。
 *
 * 两个任务：led 每 500 ms 翻转 PC13，print 每 500 ms 经 USART1 打印节拍数。
 * USART1 的输出由 common/freertos/rtos_console_uart.c 提供。
 */
#include <stdint.h>

#include "stm32f1xx.h"

#include "FreeRTOS.h"
#include "task.h"
#include "rtos_console.h"

static void led_task(void *param) {
    TickType_t last_wake = xTaskGetTickCount();

    (void)param;
    for (;;) {
        GPIOC->ODR ^= (1u << 13);
        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(500));
    }
}

static void print_task(void *param) {
    (void)param;
    for (;;) {
        console_printf("tick=%lu\r\n", (unsigned long)xTaskGetTickCount());
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

int main(void) {
    /* PC13：2 MHz 推挽输出 */
    RCC->APB2ENR |= RCC_APB2ENR_IOPCEN;
    GPIOC->CRH = (GPIOC->CRH & ~(0xFu << 20)) | (0x2u << 20);

    console_init();
    xTaskCreate(led_task, "led", configMINIMAL_STACK_SIZE, NULL, 2, NULL);
    xTaskCreate(print_task, "print", configMINIMAL_STACK_SIZE, NULL, 1, NULL);
    vTaskStartScheduler();

    for (;;) {
    }
}
