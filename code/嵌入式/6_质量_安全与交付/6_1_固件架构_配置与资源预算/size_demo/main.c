/*
 * size_demo/main.c：一个带 LED 与 UART printf 的最小固件。
 *
 * 用同一份源文件在不同的编译选项下构建，对比 Flash 与 RAM 占用。
 * 讲解见教程 6.1 节「固件架构、配置与资源预算」。
 */
#include <stdint.h>
#include <stdio.h>

#include "stm32f1xx.h"

#define LED_PIN 13u

static uint32_t g_tick_ms;

/* 环形平均滤波，模拟产品里常见的一小段数据处理代码 */
#define FILTER_LEN 8u
static int16_t g_filter_buf[FILTER_LEN];
static uint32_t g_filter_pos;

static int16_t filter_push(int16_t sample) {
    int32_t sum = 0;
    g_filter_buf[g_filter_pos] = sample;
    g_filter_pos = (g_filter_pos + 1u) % FILTER_LEN;
    for (uint32_t i = 0; i < FILTER_LEN; ++i) {
        sum += g_filter_buf[i];
    }
    return (int16_t)(sum / (int32_t)FILTER_LEN);
}

static void delay_ms(uint32_t ms) {
    /* 72 MHz 下约 9000 次空循环接近 1 ms，只用于演示 */
    for (uint32_t i = 0; i < ms; ++i) {
        for (volatile uint32_t n = 0; n < 9000u; ++n) {
        }
    }
}

int main(void) {
    RCC->APB2ENR |= RCC_APB2ENR_IOPAEN | RCC_APB2ENR_IOPCEN | RCC_APB2ENR_USART1EN;

    /* PC13 推挽输出，2 MHz */
    GPIOC->CRH = (GPIOC->CRH & ~(0xFu << 20)) | (0x2u << 20);
    /* PA9 复用推挽输出，PA10 浮空输入 */
    GPIOA->CRH = (GPIOA->CRH & ~(0xFFu << 4)) | (0x4Bu << 4);
    USART1->BRR = 72000000u / 115200u;
    USART1->CR1 = USART_CR1_TE | USART_CR1_RE | USART_CR1_UE;

    printf("size_demo boot\r\n");

    for (;;) {
        g_tick_ms += 100u;
        GPIOC->ODR ^= 1u << LED_PIN;
        int16_t v = filter_push((int16_t)(g_tick_ms & 0x3FFu));
#if defined(USE_FLOAT_PRINT)
        printf("t=%lu v=%d temp=%.1f\r\n", (unsigned long)g_tick_ms, (int)v,
               (double)v / 10.0);
#else
        printf("t=%lu v=%d\r\n", (unsigned long)g_tick_ms, (int)v);
#endif
        delay_ms(100u);
    }
}