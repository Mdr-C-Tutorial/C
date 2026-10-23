/*
 * gpio.h：STM32F103 GPIO 最小封装接口。
 *
 * 讲解见教程 2.4 节「GPIO」。
 */
#ifndef GPIO_H
#define GPIO_H

#include <stdint.h>
#include "stm32f1xx.h"

enum {
    GPIO_CFG_IN_ANALOG  = 0x0u, /* 模拟输入 */
    GPIO_CFG_OUT_PP_10M = 0x1u, /* 通用推挽输出，10 MHz */
    GPIO_CFG_OUT_PP_2M  = 0x2u, /* 通用推挽输出，2 MHz */
    GPIO_CFG_OUT_PP_50M = 0x3u, /* 通用推挽输出，50 MHz */
    GPIO_CFG_IN_FLOAT   = 0x4u, /* 浮空输入 */
    GPIO_CFG_OUT_OD_10M = 0x5u, /* 通用开漏输出，10 MHz */
    GPIO_CFG_OUT_OD_2M  = 0x6u, /* 通用开漏输出，2 MHz */
    GPIO_CFG_OUT_OD_50M = 0x7u, /* 通用开漏输出，50 MHz */
    GPIO_CFG_IN_PUPD    = 0x8u, /* 上拉或下拉输入（由 ODR 对应位选择） */
    GPIO_CFG_AF_PP_10M  = 0x9u, /* 复用推挽输出，10 MHz */
    GPIO_CFG_AF_PP_2M   = 0xAu, /* 复用推挽输出，2 MHz */
    GPIO_CFG_AF_PP_50M  = 0xBu, /* 复用推挽输出，50 MHz */
    GPIO_CFG_AF_OD_10M  = 0xDu, /* 复用开漏输出，10 MHz */
    GPIO_CFG_AF_OD_2M   = 0xEu, /* 复用开漏输出，2 MHz */
    GPIO_CFG_AF_OD_50M  = 0xFu  /* 复用开漏输出，50 MHz */
};

void gpio_config(GPIO_TypeDef *port, unsigned pin, uint32_t cfg);
void gpio_write(GPIO_TypeDef *port, unsigned pin, int level);
int gpio_read(const GPIO_TypeDef *port, unsigned pin);

#endif
