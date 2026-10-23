#ifndef UART_DMA_H
#define UART_DMA_H

#include "dma_rx_pos.h"
#include "stm32f1xx.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define UART_DMA_RX_BUF_SIZE 64u

enum uart_dma_status {
    UART_DMA_OK = 0,
    UART_DMA_ERR_PARAM = -1,
    UART_DMA_ERR_BUSY = -2,
};

typedef void (*uart_dma_tx_cb_t)(void *user_data);

struct uart_dma_handle {
    uint8_t rx_dma_buf[UART_DMA_RX_BUF_SIZE];
    size_t rx_old_pos;
    dma_rx_on_data_fn rx_on_data;
    void *rx_user;

    volatile bool tx_busy;
    uart_dma_tx_cb_t tx_cb;
    void *tx_user;
};

void uart_dma1_init(struct uart_dma_handle *h,
                    uint32_t pclk2_hz,
                    uint32_t baudrate,
                    dma_rx_on_data_fn rx_on_data,
                    void *rx_user);

enum uart_dma_status uart_dma1_send_async(struct uart_dma_handle *h,
                                          const void *buf,
                                          uint16_t len,
                                          uart_dma_tx_cb_t cb,
                                          void *user_data);

bool uart_dma1_tx_busy(const struct uart_dma_handle *h);

void uart_dma1_poll_rx(struct uart_dma_handle *h);

void uart_dma1_tx_dma_isr(struct uart_dma_handle *h);
void uart_dma1_rx_dma_isr(struct uart_dma_handle *h);
void uart_dma1_usart_isr(struct uart_dma_handle *h);

#endif
