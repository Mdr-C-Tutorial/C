#include "gpio.h"

static void enable_port_clock(const GPIO_TypeDef *port) {
    if (port == GPIOA) {
        SET_BIT(RCC->APB2ENR, RCC_APB2ENR_IOPAEN);
    } else if (port == GPIOB) {
        SET_BIT(RCC->APB2ENR, RCC_APB2ENR_IOPBEN);
    } else if (port == GPIOC) {
        SET_BIT(RCC->APB2ENR, RCC_APB2ENR_IOPCEN);
    }
}

void gpio_pin_init(const struct gpio_pin *p, enum gpio_mode mode) {
    enable_port_clock(p->port);

    uint32_t cfg = 0x4u;
    switch (mode) {
    case GPIO_MODE_OUT_PP_2MHZ:
        cfg = 0x2u;
        break;
    case GPIO_MODE_AF_PP_50MHZ:
        cfg = 0xBu;
        break;
    case GPIO_MODE_IN_FLOATING:
        cfg = 0x4u;
        break;
    case GPIO_MODE_IN_PULL_UP:
        cfg = 0x8u;
        p->port->BSRR = 1u << p->pin;
        break;
    case GPIO_MODE_IN_PULL_DOWN:
        cfg = 0x8u;
        p->port->BRR = 1u << p->pin;
        break;
    }

    __IO uint32_t *cr = (p->pin < 8u) ? &p->port->CRL : &p->port->CRH;
    uint32_t shift = ((uint32_t)(p->pin & 7u)) * 4u;
    MODIFY_REG(*cr, 0xFu << shift, cfg << shift);
}

void gpio_pin_write(const struct gpio_pin *p, bool active) {
    bool high = p->active_low ? !active : active;
    if (high) {
        p->port->BSRR = 1u << p->pin;
    } else {
        p->port->BSRR = 1u << (p->pin + 16u);
    }
}

bool gpio_pin_read(const struct gpio_pin *p) {
    bool high = (p->port->IDR & (1u << p->pin)) != 0u;
    return p->active_low ? !high : high;
}
