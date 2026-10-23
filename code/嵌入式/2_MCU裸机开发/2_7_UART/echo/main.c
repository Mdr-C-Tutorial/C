#include "clock.h"
#include "console.h"

int main(void) {
    clock_init_72mhz();
    console_init(72000000u, 115200u);

    uart_puts("USART1 echo ready (115200 8N1)\n");

    for (;;) {
        char c = uart_getc();
        if (c == '\r') {
            uart_putc('\r');
            uart_putc('\n');
        } else {
            uart_putc(c);
        }
    }
}
