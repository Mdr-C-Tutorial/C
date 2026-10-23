#ifndef UART_IT_H
#define UART_IT_H

#include "stm32f1xx.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum uart_status {
    UART_OK = 0,
    UART_ERR_PARAM = -1,
    UART_ERR_BUSY = -2,
};

typedef void (*uart_callback_t)(void *user_data);

struct uart_it_handle {
    USART_TypeDef *instance;

    const uint8_t *tx_buf;
    size_t tx_len;
    volatile size_t tx_pos;
    volatile bool tx_busy;
    uart_callback_t tx_cb;
    void *tx_user;

    uint8_t *rx_buf;
    size_t rx_len;
    volatile size_t rx_pos;
    volatile bool rx_busy;
    uart_callback_t rx_cb;
    void *rx_user;
};

enum uart_status uart_it_init(struct uart_it_handle *h,
                              USART_TypeDef *u,
                              uint32_t pclk_hz,
                              uint32_t baudrate);

enum uart_status uart_it_send_async(struct uart_it_handle *h,
                                    const void *buf,
                                    size_t len,
                                    uart_callback_t cb,
                                    void *user_data);

enum uart_status uart_it_recv_async(struct uart_it_handle *h,
                                    void *buf,
                                    size_t len,
                                    uart_callback_t cb,
                                    void *user_data);

bool uart_it_tx_busy(const struct uart_it_handle *h);
bool uart_it_rx_busy(const struct uart_it_handle *h);

void uart_it_irq_handler(struct uart_it_handle *h);

#endif
