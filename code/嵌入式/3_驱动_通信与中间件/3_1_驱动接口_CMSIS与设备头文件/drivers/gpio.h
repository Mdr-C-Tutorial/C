#ifndef DRIVERS_GPIO_H
#define DRIVERS_GPIO_H

#include <stdbool.h>
#include <stdint.h>
#include "stm32f1xx.h"

enum gpio_mode {
    GPIO_MODE_OUT_PP_2MHZ,
    GPIO_MODE_AF_PP_50MHZ,
    GPIO_MODE_IN_FLOATING,
    GPIO_MODE_IN_PULL_UP,
    GPIO_MODE_IN_PULL_DOWN,
};

struct gpio_pin {
    GPIO_TypeDef *port;
    uint8_t pin;     /* 0 ~ 15 */
    bool active_low; /* true: 低电平为有效态；false: 高电平为有效态 */
};

void gpio_pin_init(const struct gpio_pin *p, enum gpio_mode mode);
void gpio_pin_write(const struct gpio_pin *p, bool active);
bool gpio_pin_read(const struct gpio_pin *p);

#endif
