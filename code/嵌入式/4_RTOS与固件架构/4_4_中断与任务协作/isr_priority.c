/*
 * isr_priority.c：中断优先级与 FromISR 的两段初始化对比。
 *
 * 讲解见教程 4.4 节「中断与任务协作」。
 *
 * 编译检查：
 *   arm-none-eabi-gcc -std=c11 -mcpu=cortex-m3 -mthumb -O2 -Wall -Wextra \
 *       -DSTM32F103xB -c isr_priority.c
 */
#include <stdint.h>

#include "FreeRTOS.h"
#include "queue.h"
#include "task.h"

#include "stm32f1xx.h"

#define UART_IRQ_PRIO 5

static QueueHandle_t g_rx_queue;

/* 中断里只把字节交给队列，解析在任务里做 */
void USART1_IRQHandler(void) {
    BaseType_t higher_woken = pdFALSE;
    uint8_t byte = (uint8_t)(USART1->DR & 0xFFu);

    xQueueSendFromISR(g_rx_queue, &byte, &higher_woken);
    portYIELD_FROM_ISR(higher_woken);
}

#ifdef WRONG_INIT
/* 错误：没有设置优先级，中断停在复位值 0，高于 configMAX_SYSCALL_INTERRUPT_PRIORITY */
void uart_irq_init_wrong(void) {
    g_rx_queue = xQueueCreate(32, sizeof(uint8_t));
    NVIC_EnableIRQ(USART1_IRQn);
}
#else
/* 正确：优先级数值不小于 configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY（这里是 5） */
void uart_irq_init(void) {
    g_rx_queue = xQueueCreate(32, sizeof(uint8_t));

    /* 全部 4 位都作抢占优先级（2.5 节的分组设置），FreeRTOS 的断言要求如此 */
    NVIC_SetPriorityGrouping(3u);
    NVIC_SetPriority(USART1_IRQn, UART_IRQ_PRIO);
    NVIC_EnableIRQ(USART1_IRQn);
}
#endif

int main(void) {
#ifdef WRONG_INIT
    uart_irq_init_wrong();
#else
    uart_irq_init();
#endif
    vTaskStartScheduler();
    for (;;) {
    }
}