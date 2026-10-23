/*
 * button/main.c：读取 PA0 按键（接 3.3 V，内部下拉），间隔采样消抖后控制 PC13 LED。
 *
 * 讲解见教程 2.4 节「GPIO」。
 */
#include <stdint.h>
#include "stm32f1xx.h"
#include "../gpio/gpio.h"

static void delay_cycles(uint32_t count) {
    for (volatile uint32_t i = 0; i < count; ++i) {
    }
}

/* 两次采样间隔约几毫秒（默认 8 MHz HSI），稳定后返回电平（0 或 1） */
static int button_read_debounced(void) {
    int first = gpio_read(GPIOA, 0u);
    delay_cycles(4000u);
    int second = gpio_read(GPIOA, 0u);
    while (first != second) {
        first = second;
        delay_cycles(4000u);
        second = gpio_read(GPIOA, 0u);
    }
    return second;
}

int main(void) {
    /* 开启 GPIOA 与 GPIOC 时钟 */
    RCC->APB2ENR |= RCC_APB2ENR_IOPAEN | RCC_APB2ENR_IOPCEN;

    /* PC13：2 MHz 推挽输出，初始输出高电平（熄灭） */
    gpio_write(GPIOC, 13u, 1);
    gpio_config(GPIOC, 13u, GPIO_CFG_OUT_PP_2M);

    /* PA0：上下拉输入（0x8），ODR0 = 0 选择内部下拉 */
    gpio_write(GPIOA, 0u, 0);
    gpio_config(GPIOA, 0u, GPIO_CFG_IN_PUPD);

    int prev_state = 0;
    int led_on = 0;

    for (;;) {
        int cur_state = button_read_debounced();
        /* 检测按键从松开（0）到按下（1）的上升沿 */
        if (cur_state == 1 && prev_state == 0) {
            led_on = !led_on;
            gpio_write(GPIOC, 13u, led_on ? 0 : 1);
        }
        prev_state = cur_state;
    }
}
