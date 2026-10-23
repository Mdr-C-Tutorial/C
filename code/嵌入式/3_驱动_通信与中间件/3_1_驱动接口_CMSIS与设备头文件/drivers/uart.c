#include "uart.h"

enum uart_err uart_init(USART_TypeDef *u, uint32_t pclk_hz,
                        const struct uart_config *cfg) {
    if (u == NULL || cfg == NULL || cfg->baudrate == 0u) {
        return UART_ERR_PARAM;
    }
    uint32_t brr = (pclk_hz + cfg->baudrate / 2u) / cfg->baudrate;
    if (brr < 16u || brr > 0xFFFFu) {
        return UART_ERR_PARAM;
    }

    u->CR1 = 0u;
    u->CR2 = 0u;
    u->CR3 = 0u;
    u->BRR = brr;
    u->CR1 = USART_CR1_UE | USART_CR1_TE | USART_CR1_RE;
    return UART_OK;
}

void uart_write(USART_TypeDef *u, const void *buf, size_t len) {
    const uint8_t *p = (const uint8_t *)buf;
    for (size_t i = 0; i < len; ++i) {
        while (READ_BIT(u->SR, USART_SR_TXE) == 0u) {
        }
        u->DR = p[i];
    }
    while (READ_BIT(u->SR, USART_SR_TC) == 0u) {
    }
}

bool uart_read_byte(USART_TypeDef *u, uint8_t *out) {
    if (READ_BIT(u->SR, USART_SR_RXNE) == 0u) {
        return false;
    }
    *out = (uint8_t)(u->DR & 0xFFu);
    return true;
}
