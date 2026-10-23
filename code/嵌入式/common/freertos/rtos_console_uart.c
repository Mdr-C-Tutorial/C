/*
 * rtos_console_uart.c：STM32F103 上的控制台实现，走 USART1（PA9/PA10，115200 8N1）。
 *
 * 只用轮询收发，不涉及中断；2.7 节的 console.c 是同一套寄存器写法，这里
 * 为了不让第 4 章的示例依赖第 2 章的目录结构而单独写一份。
 */
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>

#include "stm32f1xx.h"
#include "FreeRTOS.h"
#include "task.h"
#include "rtos_console.h"

void console_init(void) {
    RCC->APB2ENR |= RCC_APB2ENR_IOPAEN | RCC_APB2ENR_USART1EN;

    /* PA9：50 MHz 复用推挽输出；PA10：浮空输入 */
    GPIOA->CRH = (GPIOA->CRH & ~(0xFFu << 4)) | (0x4Bu << 4);

    USART1->BRR = (72000000u + 115200u / 2u) / 115200u;
    USART1->CR2 = 0;
    USART1->CR3 = 0;
    USART1->CR1 = USART_CR1_TE | USART_CR1_RE | USART_CR1_UE;
}

static void uart_putc(char c) {
    while ((USART1->SR & USART_SR_TXE) == 0) {
    }
    USART1->DR = (uint8_t)c;
}

int _write(int fd, const char *buf, int len) {
    (void)fd;
    for (int i = 0; i < len; ++i) {
        if (buf[i] == '\n') {
            uart_putc('\r');
        }
        uart_putc(buf[i]);
    }
    return len;
}

void console_printf(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    taskENTER_CRITICAL();
    vprintf(fmt, args);
    fflush(stdout);
    taskEXIT_CRITICAL();
    va_end(args);
}
