/*
 * target_init.c：让同一份主机测试跑到目标板上。
 *
 * 测试文件的 main() 与 setUp/tearDown 一行不改。这个文件用 constructor
 * 在 main 之前初始化 USART1，并把 _write 接到串口，测试输出就从 UART 出来。
 *
 * 构建命令见正文，讲解见教程 6.3 节「测试、逻辑分析与硬件在环」。
 */
#include <stdint.h>

#include "stm32f1xx.h"

int _write(int fd, const char *buf, int len) {
    (void)fd;
    for (int i = 0; i < len; ++i) {
        while ((USART1->SR & USART_SR_TXE) == 0u) {
        }
        USART1->DR = (uint8_t)buf[i];
    }
    while ((USART1->SR & USART_SR_TC) == 0u) {
    }
    return len;
}

__attribute__((constructor)) static void target_console_init(void) {
    RCC->APB2ENR |= RCC_APB2ENR_IOPAEN | RCC_APB2ENR_USART1EN;
    GPIOA->CRH = (GPIOA->CRH & ~(0xFFu << 4)) | (0x4Bu << 4);
    USART1->BRR = 72000000u / 115200u;
    USART1->CR1 = USART_CR1_TE | USART_CR1_RE | USART_CR1_UE;
}