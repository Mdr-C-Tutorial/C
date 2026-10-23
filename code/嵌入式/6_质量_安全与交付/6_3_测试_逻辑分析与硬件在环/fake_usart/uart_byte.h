/*
 * uart_byte.h：一个只依赖「外设结构体」的发送驱动，可在 PC 上测试。
 *
 * 真实驱动收 USART_TypeDef *；为了在主机上编译，这里用一个最小结构体
 * 模拟 SR 与 DR 两个寄存器，测试时传入 RAM 里的对象。
 *
 * 讲解见教程 6.3 节「测试、逻辑分析与硬件在环」。
 */
#ifndef UART_BYTE_H
#define UART_BYTE_H

#include <stdbool.h>
#include <stdint.h>

/* 只保留驱动用到的那几位 */
struct uart_regs {
    volatile uint32_t sr;
    volatile uint32_t dr;
    volatile uint32_t cr1;
};

#define UART_SR_TXE (1u << 7)
#define UART_SR_TC (1u << 6)
#define UART_SR_RXNE (1u << 5)
#define UART_CR1_UE (1u << 13)
#define UART_CR1_TE (1u << 3)

/* 发送一个字节；等待 TXE 时最多轮询 max_polls 次，超时返回 false */
bool uart_putc(struct uart_regs *u, uint8_t byte, uint32_t max_polls);

/* 读一个字节；没有数据时立即返回 false */
bool uart_getc(struct uart_regs *u, uint8_t *out);

#endif /* UART_BYTE_H */