#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

/* 模拟 USART 寄存器与标志位，便于在 PC 上单步验证中断发送状态转换 */
struct mock_usart {
    volatile uint32_t SR;
    volatile uint32_t DR;
    volatile uint32_t CR1;
};

#define MOCK_SR_TC       (1u << 6)
#define MOCK_SR_TXE      (1u << 7)
#define MOCK_CR1_TCIE    (1u << 6)
#define MOCK_CR1_TXEIE   (1u << 7)

typedef void (*uart_tx_done_fn)(void *user);

/* TODO: 定义发送描述符并在 uart_write_async 与 mock_usart_isr 中完成状态转移 */

int uart_write_async(struct mock_usart *u,
                     const void *buf,
                     size_t len,
                     uart_tx_done_fn cb,
                     void *user) {
    (void)u;
    (void)buf;
    (void)len;
    (void)cb;
    (void)user;
    return -1;
}

void mock_usart_isr(struct mock_usart *u) {
    (void)u;
}

int main(void) {
    struct mock_usart u = {.SR = MOCK_SR_TXE, .DR = 0u, .CR1 = 0u};
    const char msg[] = "OK";
    int rc = uart_write_async(&u, msg, 2u, NULL, NULL);
    printf("start rc = %d, CR1 = 0x%02X\n", rc, (unsigned)u.CR1);
    return 0;
}
