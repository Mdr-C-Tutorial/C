/*
 * uart_byte.c：驱动实现。
 *
 * 每次使用之前把 SR 的 TXE 重新读一遍，测试用的假外设可以在两次读之间
 * 改变状态，模拟「发送缓冲满了」。
 *
 * 讲解见教程 6.3 节「测试、逻辑分析与硬件在环」。
 */
#include "uart_byte.h"

bool uart_putc(struct uart_regs *u, uint8_t byte, uint32_t max_polls) {
    for (uint32_t i = 0; i < max_polls; ++i) {
        if ((u->sr & UART_SR_TXE) != 0u) {
            u->dr = byte;
            return true;
        }
    }
    return false;
}

bool uart_getc(struct uart_regs *u, uint8_t *out) {
    if ((u->sr & UART_SR_RXNE) == 0u) {
        return false;
    }
    *out = (uint8_t)(u->dr & 0xFFu);
    return true;
}