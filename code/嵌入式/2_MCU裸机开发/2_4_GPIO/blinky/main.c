/*
 * blinky/main.c：驱动 PC13 板载 LED 闪烁（低电平点亮）。
 *
 * 讲解见教程 2.4 节「GPIO」。
 */
#include <stdint.h>
#include "stm32f1xx.h"

static void delay_cycles(uint32_t count) {
    for (volatile uint32_t i = 0; i < count; ++i) {
    }
}

int main(void) {
    /* 1. 开启 GPIOC 的 APB2 外设时钟 */
    RCC->APB2ENR |= RCC_APB2ENR_IOPCEN;

    /* 2. 把 PC13（CRH 位 23:20）配置为通用推挽输出，最大速度 2 MHz（0x2） */
    GPIOC->CRH &= ~(0xFu << 20);
    GPIOC->CRH |= 0x2u << 20;

    for (;;) {
        /* PC13 输出低电平，点亮板载 LED */
        GPIOC->BSRR = GPIO_BSRR_BR13;
        delay_cycles(400000u);

        /* PC13 输出高电平，熄灭板载 LED */
        GPIOC->BSRR = GPIO_BSRR_BS13;
        delay_cycles(400000u);
    }
}
