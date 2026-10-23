/*
 * exti9_5_dispatch_starter.c：习题 62243 起始代码。
 *
 * 任务：配置 PB5（下降沿触发 EXTI5）与 PA8（上升沿触发 EXTI8），
 * 并在共用的 EXTI9_5_IRQHandler 中分别检查、写 1 清除各自的挂起位并计数。
 */
#include <stdint.h>
#include "stm32f1xx.h"

volatile uint32_t g_pb5_fall_count;
volatile uint32_t g_pa8_rise_count;

void exti9_5_inputs_init(void) {
    /* TODO:
     * 1. 开启 GPIOA、GPIOB、AFIO 时钟
     * 2. 配置 PB5 为上拉输入，PA8 为下拉输入
     * 3. 配置 AFIO->EXTICR[1]（EXTI5 -> PB5）与 AFIO->EXTICR[2]（EXTI8 -> PA8）
     * 4. 配置 EXTI5 下降沿触发、EXTI8 上升沿触发，清除 PR 后使能 IMR 对应位
     * 5. 配置 NVIC 中 EXTI9_5_IRQn 的优先级并使能
     */
}

void EXTI9_5_IRQHandler(void) {
    /* TODO: 分别检查 EXTI_PR_PR5 与 EXTI_PR_PR8，只向触发的位写 1 清除并累加对应计数 */
}
