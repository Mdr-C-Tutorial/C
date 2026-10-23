#include "clock.h"
#include "console.h"
#include "stm32f1xx.h"
#include "tick.h"

static void enter_standby_mode(void) {
    uart_flush();

    /* 使能 PA0（WKUP 引脚）上升沿唤醒，清除之前的唤醒标志 WUF */
    PWR->CSR |= PWR_CSR_EWUP;
    PWR->CR |= PWR_CR_CWUF | PWR_CR_PDDS;

    SCB->SCR |= SCB_SCR_SLEEPDEEP_Msk;
    __DSB();
    __WFI();
}

int main(void) {
    clock_init_72mhz();
    tick_init();
    console_init(72000000u, 115200u);

    RCC->APB2ENR |= RCC_APB2ENR_IOPCEN;
    GPIOC->CRH = (GPIOC->CRH & ~(0xFu << 20)) | (0x2u << 20);
    GPIOC->BSRR = GPIO_BSRR_BR13; /* 点亮 LED */

    /* 访问 PWR 寄存器前必须先开启电源接口时钟 */
    RCC->APB1ENR |= RCC_APB1ENR_PWREN;

    if ((PWR->CSR & PWR_CSR_SBF) != 0) {
        /* 写 CSBF 与 CWUF 清除待机标志和唤醒标志 */
        PWR->CR |= PWR_CR_CSBF | PWR_CR_CWUF;
        uart_puts("boot: woke up from Standby mode (SBF = 1)\n");
    } else {
        uart_puts("boot: cold reset or normal reset (SBF = 0)\n");
    }

    delay_ms(2000u);
    uart_puts("standby: entering Standby mode, press PA0 (WKUP) to wake up\n");
    GPIOC->BSRR = GPIO_BSRR_BS13;
    enter_standby_mode();

    for (;;) {
    }
}
