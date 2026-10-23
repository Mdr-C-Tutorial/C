#include "clock.h"
#include "console.h"
#include "stm32f1xx.h"
#include "tick.h"

/*
 * 配置 IWDG 超时约为 1 s（按典型值 f_LSI = 40 kHz 计算）：
 *   分频系数 = 4 * 2^PR = 64（PR = 4）
 *   重装载值 RLR = 624
 *   T_out = 64 * (624 + 1) / 40000 = 1.000 s
 */
static void iwdg_init_1s(void) {
    /* 调试暂停 CPU 时同步冻结 IWDG 计数器 */
    DBGMCU->CR |= DBGMCU_CR_DBG_IWDG_STOP;

    IWDG->KR = 0x5555u; /* 解锁 PR 与 RLR 写保护 */
    IWDG->PR = 4u;      /* 64 分频 */
    IWDG->RLR = 624u;   /* 重装载值 624 */
    IWDG->KR = 0xAAAAu; /* 立即把 RLR 装入计数器 */
    IWDG->KR = 0xCCCCu; /* 启动独立看门狗 */
}

static inline void iwdg_feed(void) {
    IWDG->KR = 0xAAAAu;
}

static void gpio_init_led_button(void) {
    RCC->APB2ENR |= RCC_APB2ENR_IOPAEN | RCC_APB2ENR_IOPCEN;

    /* PC13：2 MHz 推挽输出（CNF=00, MODE=10 -> 0x2） */
    GPIOC->CRH = (GPIOC->CRH & ~(0xFu << 20)) | (0x2u << 20);
    GPIOC->BSRR = GPIO_BSRR_BS13; /* 初始熄灭 */

    /* PA0：下拉输入（CNF=10, MODE=00 -> 0x8，ODR0=0） */
    GPIOA->CRL = (GPIOA->CRL & ~0xFu) | 0x8u;
    GPIOA->BRR = 1u << 0;
}

int main(void) {
    clock_init_72mhz();
    tick_init();
    console_init(72000000u, 115200u);
    gpio_init_led_button();

    reset_cause_t cause = reset_cause_get();
    uart_puts("boot: reset cause = ");
    uart_puts(reset_cause_name(cause));
    uart_puts("\n");

    iwdg_init_1s();

    uint32_t last_feed = tick_now();
    int led_on = 0;

    for (;;) {
        /* 模拟主循环各项任务；按住 PA0 按键时死等，模拟任务卡死 */
        while ((GPIOA->IDR & 1u) != 0) {
        }

        if (tick_elapsed(last_feed, 250u)) {
            last_feed = tick_now();
            led_on = !led_on;
            GPIOC->BSRR = led_on ? GPIO_BSRR_BR13 : GPIO_BSRR_BS13;
            iwdg_feed();
            uart_puts("heartbeat: watchdog fed\n");
        }
    }
}
