#include "clock.h"
#include "console.h"
#include "stm32f1xx.h"
#include "tick.h"

static volatile uint32_t g_wakeup_count;

void EXTI0_IRQHandler(void) {
    if ((EXTI->PR & EXTI_PR_PR0) != 0) {
        EXTI->PR = EXTI_PR_PR0; /* 写 1 清除挂起标志 */
        ++g_wakeup_count;
    }
}

static void pa0_exti_init(void) {
    RCC->APB2ENR |= RCC_APB2ENR_IOPAEN | RCC_APB2ENR_IOPCEN | RCC_APB2ENR_AFIOEN;

    /* PC13：2 MHz 推挽输出 */
    GPIOC->CRH = (GPIOC->CRH & ~(0xFu << 20)) | (0x2u << 20);

    /* PA0：下拉输入 */
    GPIOA->CRL = (GPIOA->CRL & ~0xFu) | 0x8u;
    GPIOA->BRR = 1u << 0;

    /* EXTI0 映射到 PA0，上升沿触发 */
    AFIO->EXTICR[0] &= ~AFIO_EXTICR1_EXTI0;
    EXTI->RTSR |= EXTI_RTSR_TR0;
    EXTI->FTSR &= ~EXTI_FTSR_TR0;
    EXTI->PR = EXTI_PR_PR0;
    EXTI->IMR |= EXTI_IMR_MR0;

    NVIC_SetPriority(EXTI0_IRQn, 2u);
    NVIC_EnableIRQ(EXTI0_IRQn);
}

static void enter_stop_mode(void) {
    /* 1. 等待串口最后一帧完全移出移位寄存器 */
    uart_flush();

    /* 2. 开启电源接口时钟，选择 Stop 模式 + 低功耗调压器 */
    RCC->APB1ENR |= RCC_APB1ENR_PWREN;
    PWR->CR = (PWR->CR & ~PWR_CR_PDDS) | PWR_CR_LPDS;

    /* 3. 内核置位 SLEEPDEEP，关闭 SysTick 防止节拍中断提前唤醒 */
    SCB->SCR |= SCB_SCR_SLEEPDEEP_Msk;
    SysTick->CTRL &= ~SysTick_CTRL_TICKINT_Msk;

    __DSB();
    __WFI();
    __ISB();

    /* 4. 唤醒后系统时钟退回 HSI 8 MHz，恢复 72 MHz 与 SysTick 中断 */
    SCB->SCR &= ~SCB_SCR_SLEEPDEEP_Msk;
    clock_init_72mhz();
    SysTick->CTRL |= SysTick_CTRL_TICKINT_Msk;
}

int main(void) {
    clock_init_72mhz();
    tick_init();
    console_init(72000000u, 115200u);
    pa0_exti_init();

    /* 调试期间保持 Stop 模式下的调试连接（测真实电流时应去掉） */
    DBGMCU->CR |= DBGMCU_CR_DBG_STOP;

    for (;;) {
        GPIOC->BSRR = GPIO_BSRR_BR13; /* 点亮 LED 2 秒 */
        uart_puts("run: active for 2 s, then entering Stop mode\n");
        delay_ms(2000u);

        GPIOC->BSRR = GPIO_BSRR_BS13; /* 熄灭 LED 后进入 Stop */
        uart_puts("stop: press PA0 to wake up\n");
        enter_stop_mode();

        uart_puts("wakeup: clock restored to 72 MHz\n");
    }
}
