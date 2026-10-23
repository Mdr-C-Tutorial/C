#include "console.h"
#include "stm32f1xx.h"

void console_init(uint32_t pclk2_hz, uint32_t baud) {
    /* 1. 开启 GPIOA 与 USART1 时钟 */
    RCC->APB2ENR |= RCC_APB2ENR_IOPAEN | RCC_APB2ENR_USART1EN;

    /* 2. PA9（CRH[7:4]）：50 MHz 复用推挽输出（CNF=10, MODE=11 -> 0xB）
     *    PA10（CRH[11:8]）：浮空输入（CNF=01, MODE=00 -> 0x4）
     */
    GPIOA->CRH = (GPIOA->CRH & ~(0xFFu << 4)) | (0x4Bu << 4);

    /* 3. 配置波特率：BRR = round(f_CK / baud) */
    USART1->BRR = (pclk2_hz + baud / 2u) / baud;

    /* 4. 8 数据位、无校验、1 停止位（CR2/CR3 保持复位值 0），使能收发与 USART */
    USART1->CR2 = 0;
    USART1->CR3 = 0;
    USART1->CR1 = USART_CR1_TE | USART_CR1_RE | USART_CR1_UE;
}

void uart_putc(char c) {
    while ((USART1->SR & USART_SR_TXE) == 0) {
    }
    USART1->DR = (uint8_t)c;
}

char uart_getc(void) {
    while ((USART1->SR & USART_SR_RXNE) == 0) {
    }
    return (char)(USART1->DR & 0xFFu);
}

void uart_puts(const char *s) {
    while (*s != '\0') {
        if (*s == '\n') {
            uart_putc('\r');
        }
        uart_putc(*s++);
    }
}

void uart_write(const void *buf, size_t len) {
    const uint8_t *p = (const uint8_t *)buf;
    for (size_t i = 0; i < len; ++i) {
        uart_putc((char)p[i]);
    }
}

void uart_flush(void) {
    while ((USART1->SR & USART_SR_TC) == 0) {
    }
}
