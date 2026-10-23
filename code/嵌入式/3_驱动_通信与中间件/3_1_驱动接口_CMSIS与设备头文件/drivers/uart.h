#ifndef DRIVERS_UART_H
#define DRIVERS_UART_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "stm32f1xx.h"

enum uart_err {
    UART_OK = 0,
    UART_ERR_PARAM = -1,
};

struct uart_config {
    uint32_t baudrate;
};

enum uart_err uart_init(USART_TypeDef *u, uint32_t pclk_hz,
                        const struct uart_config *cfg);
void uart_write(USART_TypeDef *u, const void *buf, size_t len);
bool uart_read_byte(USART_TypeDef *u, uint8_t *out);

#endif
