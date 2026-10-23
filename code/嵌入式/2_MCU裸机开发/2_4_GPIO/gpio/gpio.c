/*
 * gpio.c：STM32F103 GPIO 最小封装实现。
 *
 * 讲解见教程 2.4 节「GPIO」。
 */
#include "gpio.h"

void gpio_config(GPIO_TypeDef *port, unsigned pin, uint32_t cfg) {
    volatile uint32_t *cr = (pin < 8u) ? &port->CRL : &port->CRH;
    unsigned shift = (pin & 7u) * 4u;
    uint32_t val = *cr;
    val &= ~(0xFu << shift);
    val |= (cfg & 0xFu) << shift;
    *cr = val;
}

void gpio_write(GPIO_TypeDef *port, unsigned pin, int level) {
    if (level != 0) {
        port->BSRR = 1u << pin;
    } else {
        port->BSRR = 1u << (pin + 16u);
    }
}

int gpio_read(const GPIO_TypeDef *port, unsigned pin) {
    return (int)((port->IDR >> pin) & 1u);
}
