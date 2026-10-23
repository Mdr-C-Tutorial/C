#include "uart_it.h"

enum uart_status uart_it_init(struct uart_it_handle *h,
                              USART_TypeDef *u,
                              uint32_t pclk_hz,
                              uint32_t baudrate) {
    if (h == NULL || u == NULL || baudrate == 0u) {
        return UART_ERR_PARAM;
    }
    uint32_t brr = (pclk_hz + baudrate / 2u) / baudrate;
    if (brr < 16u || brr > 0xFFFFu) {
        return UART_ERR_PARAM;
    }

    *h = (struct uart_it_handle){0};
    h->instance = u;

    u->CR1 = 0u;
    u->CR2 = 0u;
    u->CR3 = 0u;
    u->BRR = brr;
    u->CR1 = USART_CR1_UE | USART_CR1_TE | USART_CR1_RE;
    return UART_OK;
}

enum uart_status uart_it_send_async(struct uart_it_handle *h,
                                    const void *buf,
                                    size_t len,
                                    uart_callback_t cb,
                                    void *user_data) {
    if (h == NULL || buf == NULL || len == 0u) {
        return UART_ERR_PARAM;
    }
    if (h->tx_busy) {
        return UART_ERR_BUSY;
    }

    h->tx_buf = (const uint8_t *)buf;
    h->tx_len = len;
    h->tx_pos = 0u;
    h->tx_cb = cb;
    h->tx_user = user_data;
    h->tx_busy = true;

    h->instance->CR1 |= USART_CR1_TXEIE;
    return UART_OK;
}

enum uart_status uart_it_recv_async(struct uart_it_handle *h,
                                    void *buf,
                                    size_t len,
                                    uart_callback_t cb,
                                    void *user_data) {
    if (h == NULL || buf == NULL || len == 0u) {
        return UART_ERR_PARAM;
    }
    if (h->rx_busy) {
        return UART_ERR_BUSY;
    }

    h->rx_buf = (uint8_t *)buf;
    h->rx_len = len;
    h->rx_pos = 0u;
    h->rx_cb = cb;
    h->rx_user = user_data;
    h->rx_busy = true;

    h->instance->CR1 |= USART_CR1_RXNEIE;
    return UART_OK;
}

bool uart_it_tx_busy(const struct uart_it_handle *h) {
    return h != NULL && h->tx_busy;
}

bool uart_it_rx_busy(const struct uart_it_handle *h) {
    return h != NULL && h->rx_busy;
}

void uart_it_irq_handler(struct uart_it_handle *h) {
    USART_TypeDef *u = h->instance;
    uint32_t sr = u->SR;
    uint32_t cr1 = u->CR1;

    /* 接收分支：读 SR 后再读 DR 可同时清除 RXNE 与 ORE */
    if ((cr1 & USART_CR1_RXNEIE) != 0u &&
        (sr & (USART_SR_RXNE | USART_SR_ORE)) != 0u) {
        uint8_t byte = (uint8_t)(u->DR & 0xFFu);
        if (h->rx_pos < h->rx_len) {
            h->rx_buf[h->rx_pos++] = byte;
        }
        if (h->rx_pos >= h->rx_len) {
            u->CR1 &= ~USART_CR1_RXNEIE;
            h->rx_busy = false;
            if (h->rx_cb != NULL) {
                h->rx_cb(h->rx_user);
            }
        }
    }

    /* 发送数据寄存器空：继续填入下一个字节 */
    if ((cr1 & USART_CR1_TXEIE) != 0u && (sr & USART_SR_TXE) != 0u) {
        if (h->tx_pos < h->tx_len) {
            u->DR = h->tx_buf[h->tx_pos++];
        }
        if (h->tx_pos >= h->tx_len) {
            /* 最后一个字节已写入 DR，关闭 TXEIE，改等移位寄存器发完（TC） */
            u->CR1 = (u->CR1 & ~USART_CR1_TXEIE) | USART_CR1_TCIE;
        }
    }

    /* 发送完成：停止位也已发出，通知上层 */
    if ((cr1 & USART_CR1_TCIE) != 0u && (sr & USART_SR_TC) != 0u) {
        u->CR1 &= ~USART_CR1_TCIE;
        u->SR = ~USART_SR_TC;
        h->tx_busy = false;
        if (h->tx_cb != NULL) {
            h->tx_cb(h->tx_user);
        }
    }
}
