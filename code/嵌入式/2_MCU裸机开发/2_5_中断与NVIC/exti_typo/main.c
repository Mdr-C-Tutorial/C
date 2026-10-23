/*
 * main.c：2.5 节错误对照示例（故意把 EXTI0_IRQHandler 拼错为 EXTI0_IRQhandler）。
 *
 * 编译与链接均不报错，但向量表中的 EXTI0_IRQHandler 未被替换，仍指向 Default_Handler。
 */
#include <stdint.h>
#include "stm32f1xx.h"
#include "clock.h"

/* 错误：H 写成了小写 h，没有覆盖启动文件中的弱符号 EXTI0_IRQHandler */
void EXTI0_IRQhandler(void) {
    EXTI->PR = EXTI_PR_PR0;
    GPIOC->BSRR = GPIO_BSRR_BR13;
}

int main(void) {
    clock_init_72mhz();

    RCC->APB2ENR |= RCC_APB2ENR_IOPAEN | RCC_APB2ENR_IOPCEN | RCC_APB2ENR_AFIOEN;
    GPIOC->CRH = (GPIOC->CRH & ~(0xFu << 20)) | (0x2u << 20);
    GPIOC->BSRR = GPIO_BSRR_BS13;

    GPIOA->CRL = (GPIOA->CRL & ~(0xFu << 0)) | (0x8u << 0);
    GPIOA->BSRR = GPIO_BSRR_BR0;

    AFIO->EXTICR[0] &= ~AFIO_EXTICR1_EXTI0;
    EXTI->RTSR |= EXTI_RTSR_TR0;
    EXTI->PR = EXTI_PR_PR0;
    EXTI->IMR |= EXTI_IMR_MR0;

    NVIC_SetPriorityGrouping(3u);
    NVIC_SetPriority(EXTI0_IRQn, 2u);
    NVIC_EnableIRQ(EXTI0_IRQn);

    for (;;) {
    }
}
