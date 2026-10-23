#ifndef CONSOLE_H
#define CONSOLE_H

#include <stddef.h>
#include <stdint.h>

void console_init(uint32_t pclk2_hz, uint32_t baud);
void uart_putc(char c);
char uart_getc(void);
void uart_puts(const char *s);
void uart_write(const void *buf, size_t len);
void uart_flush(void);

#endif /* CONSOLE_H */
