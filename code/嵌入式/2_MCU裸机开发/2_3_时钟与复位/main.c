#include "clock.h"
#include "stm32f1xx.h"

/* 供调试器观察的全局对象 */
volatile reset_cause_t g_last_reset = RESET_CAUSE_UNKNOWN;

int main(void) {
    g_last_reset = reset_cause_get();
    clock_init_72mhz();
    mco_enable_pll_div2();

    /* 开启 GPIOC 时钟并点亮 PC13 板载 LED（低电平点亮）表示时钟切换完成 */
    RCC->APB2ENR |= RCC_APB2ENR_IOPCEN;
    GPIOC->CRH = (GPIOC->CRH & ~(0xFu << 20)) | (0x2u << 20);
    GPIOC->BSRR = GPIO_BSRR_BR13;

    for (;;) {
    }
}
