/*
 * superloop/main.c：基于 SysTick 1 ms 节拍的非阻塞超级循环与按键消抖状态机。
 *
 * 硬件连接：
 *   - PC13：板载 LED（低电平点亮）
 *   - PA0 ：按键（按下接 3.3 V，内部下拉）
 */
#include <stdint.h>
#include "stm32f1xx.h"
#include "clock.h"
#include "../tick.h"

typedef struct {
    uint8_t stable_level;
    uint8_t confirm_cnt;
} debounce_t;

static void gpio_init(void) {
    RCC->APB2ENR |= RCC_APB2ENR_IOPAEN | RCC_APB2ENR_IOPCEN;

    /* PC13：通用推挽输出，最大速度 2 MHz（CNF13 = 00，MODE13 = 10） */
    GPIOC->CRH &= ~(0xFu << 20);
    GPIOC->CRH |= 0x2u << 20;
    GPIOC->BSRR = GPIO_BSRR_BS13; /* 初始熄灭 */

    /* PA0：上拉/下拉输入（CNF0 = 10，MODE0 = 00），ODR0 = 0 选下拉 */
    GPIOA->CRL &= ~(0xFu << 0);
    GPIOA->CRL |= 0x8u << 0;
    GPIOA->BSRR = GPIO_BSRR_BR0;
}

static void led_toggle(void) {
    if ((GPIOC->ODR & GPIO_ODR_ODR13) != 0u) {
        GPIOC->BSRR = GPIO_BSRR_BR13;
    } else {
        GPIOC->BSRR = GPIO_BSRR_BS13;
    }
}

/*
 * 每 10 ms 调用一次。连续 3 次采样与当前稳定电平不同时才确认翻转，
 * 仅在确认稳定按下（0 -> 1）的那一次返回 1。
 */
static int button_scan_10ms(debounce_t *db) {
    uint8_t raw = (GPIOA->IDR & GPIO_IDR_IDR0) != 0u ? 1u : 0u;
    if (raw == db->stable_level) {
        db->confirm_cnt = 0u;
        return 0;
    }

    ++db->confirm_cnt;
    if (db->confirm_cnt >= 3u) {
        db->stable_level = raw;
        db->confirm_cnt = 0u;
        if (raw == 1u) {
            return 1;
        }
    }
    return 0;
}

int main(void) {
    clock_init_72mhz();
    tick_init();
    gpio_init();

    debounce_t key_pa0 = { 0u, 0u };
    uint32_t last_led_ms = tick_now();
    uint32_t last_key_ms = last_led_ms;
    uint32_t led_period_ms = 500u;

    for (;;) {
        uint32_t now = tick_now();

        if ((uint32_t)(now - last_led_ms) >= led_period_ms) {
            last_led_ms = now;
            led_toggle();
        }

        if ((uint32_t)(now - last_key_ms) >= 10u) {
            last_key_ms = now;
            if (button_scan_10ms(&key_pa0) != 0) {
                /* 每次按下在 500 ms 慢闪与 100 ms 快闪之间切换 */
                led_period_ms = (led_period_ms == 500u) ? 100u : 500u;
            }
        }
    }
}
