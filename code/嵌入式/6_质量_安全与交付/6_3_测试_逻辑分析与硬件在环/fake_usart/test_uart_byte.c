/*
 * test_uart_byte.c：用 RAM 里的假寄存器测试 uart_putc。
 *
 * 假外设按次数改变 TXE 位，模拟「发送缓冲暂时是满的」。
 *
 * 编译（在 fake_usart/ 下）：
 *   gcc -std=c17 -Wall -Wextra -I. -I../common uart_byte.c test_uart_byte.c -o test_uart
 *
 * 讲解见教程 6.3 节「测试、逻辑分析与硬件在环」。
 */
#include <stdio.h>
#include <string.h>

#include "../common/test_assert.h"
#include "uart_byte.h"

static struct uart_regs g_uart;

void setUp(void) {
    memset(&g_uart, 0, sizeof g_uart);
}

void tearDown(void) {
}

static void test_putc_txe_ready(void) {
    g_uart.sr = UART_SR_TXE;
    TEST_ASSERT_TRUE(uart_putc(&g_uart, 'A', 10u));
    TEST_ASSERT_EQUAL_HEX8('A', g_uart.dr & 0xFFu);
}

static void test_putc_timeout(void) {
    /* TXE 永远是 0，驱动必须放弃而不是无限等待 */
    g_uart.sr = 0u;
    TEST_ASSERT_FALSE(uart_putc(&g_uart, 'C', 100u));
    /* 放弃时不能往数据寄存器里写东西 */
    TEST_ASSERT_EQUAL_HEX8(0u, g_uart.dr & 0xFFu);
}

static void test_putc_no_overwrite_before_txe(void) {
    /* 上一次发送还没完成就调用：SR 说没准备好，这次发送放弃，DR 保持原值 */
    g_uart.sr = 0u;
    g_uart.dr = 0x55u;
    TEST_ASSERT_FALSE(uart_putc(&g_uart, 'D', 3u));
    TEST_ASSERT_EQUAL_HEX8(0x55u, g_uart.dr & 0xFFu);
}

static void test_getc_empty(void) {
    uint8_t out = 0xAAu;
    g_uart.sr = 0u;
    TEST_ASSERT_FALSE(uart_getc(&g_uart, &out));
    TEST_ASSERT_EQUAL_HEX8(0xAAu, out); /* 没有数据时不能改输出 */
}

static void test_getc_ready(void) {
    uint8_t out = 0;
    g_uart.sr = UART_SR_RXNE;
    g_uart.dr = 0x5Au;
    TEST_ASSERT_TRUE(uart_getc(&g_uart, &out));
    TEST_ASSERT_EQUAL_HEX8(0x5Au, out);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_putc_txe_ready);
    RUN_TEST(test_putc_timeout);
    RUN_TEST(test_putc_no_overwrite_before_txe);
    RUN_TEST(test_getc_empty);
    RUN_TEST(test_getc_ready);
    return UNITY_END();
}