#include "clock.h"
#include "stm32f1xx.h"

uint32_t SystemCoreClock = 8000000u;

void clock_init_72mhz(void) {
    /* 1. 开启外部 8 MHz 晶振（HSE），等待振荡稳定 */
    RCC->CR |= RCC_CR_HSEON;
    while ((RCC->CR & RCC_CR_HSERDY) == 0) {
    }

    /* 2. 提高 SYSCLK 前先开启 Flash 预取缓冲并设为 2 个等待周期（48~72 MHz） */
    FLASH->ACR = (FLASH->ACR & ~FLASH_ACR_LATENCY) |
                 FLASH_ACR_LATENCY_2 | FLASH_ACR_PRFTBE;

    /* 3. 配置总线分频与 PLL：
     *    HCLK = SYSCLK / 1 = 72 MHz
     *    PCLK2 = HCLK / 1 = 72 MHz
     *    PCLK1 = HCLK / 2 = 36 MHz（TIM2~TIM4 时钟自动 ×2 = 72 MHz）
     *    ADCCLK = PCLK2 / 6 = 12 MHz（<= 14 MHz）
     *    USBCLK = PLLCLK / 1.5 = 48 MHz（USBPRE = 0）
     *    PLL 输入 = HSE 不分频（8 MHz），倍频 ×9 = 72 MHz
     */
    uint32_t cfgr_mask = RCC_CFGR_HPRE | RCC_CFGR_PPRE1 | RCC_CFGR_PPRE2 |
                         RCC_CFGR_ADCPRE | RCC_CFGR_PLLSRC |
                         RCC_CFGR_PLLXTPRE | RCC_CFGR_PLLMULL |
                         RCC_CFGR_USBPRE;
    uint32_t cfgr_val = RCC_CFGR_HPRE_DIV1 | RCC_CFGR_PPRE2_DIV1 |
                        RCC_CFGR_PPRE1_DIV2 | RCC_CFGR_ADCPRE_DIV6 |
                        RCC_CFGR_PLLSRC | RCC_CFGR_PLLMULL9;
    RCC->CFGR = (RCC->CFGR & ~cfgr_mask) | cfgr_val;

    /* 4. 开启 PLL，等待锁定 */
    RCC->CR |= RCC_CR_PLLON;
    while ((RCC->CR & RCC_CR_PLLRDY) == 0) {
    }

    /* 5. 将系统时钟切换到 PLL，等待硬件确认切换完成 */
    RCC->CFGR = (RCC->CFGR & ~RCC_CFGR_SW) | RCC_CFGR_SW_PLL;
    while ((RCC->CFGR & RCC_CFGR_SWS) != RCC_CFGR_SWS_PLL) {
    }

    /* 6. 更新全局系统时钟频率对象 */
    SystemCoreClock = 72000000u;
}

void mco_enable_pll_div2(void) {
    /* 开启 GPIOA 时钟，把 PA8（CRH[3:0]）配置为 50 MHz 复用推挽输出（0xB） */
    RCC->APB2ENR |= RCC_APB2ENR_IOPAEN;
    GPIOA->CRH = (GPIOA->CRH & ~0xFu) | 0xBu;

    /* 选择 PLLCLK / 2（72 MHz / 2 = 36 MHz）从 PA8（MCO）输出 */
    RCC->CFGR = (RCC->CFGR & ~RCC_CFGR_MCO) | RCC_CFGR_MCO_PLLCLK_DIV2;
}

reset_cause_t reset_cause_get(void) {
    uint32_t csr = RCC->CSR;
    reset_cause_t cause = RESET_CAUSE_UNKNOWN;

    /* 内部复位源也会驱动 NRST 引脚拉低并置位 PINRSTF，因此先判别具体复位源 */
    if ((csr & RCC_CSR_LPWRRSTF) != 0) {
        cause = RESET_CAUSE_LOW_POWER;
    } else if ((csr & RCC_CSR_WWDGRSTF) != 0) {
        cause = RESET_CAUSE_WWDG;
    } else if ((csr & RCC_CSR_IWDGRSTF) != 0) {
        cause = RESET_CAUSE_IWDG;
    } else if ((csr & RCC_CSR_SFTRSTF) != 0) {
        cause = RESET_CAUSE_SOFTWARE;
    } else if ((csr & RCC_CSR_PORRSTF) != 0) {
        cause = RESET_CAUSE_POR;
    } else if ((csr & RCC_CSR_PINRSTF) != 0) {
        cause = RESET_CAUSE_PIN;
    }

    /* 向 RMVF 写 1 清除所有复位标志，以免影响下一次复位后的判别 */
    RCC->CSR |= RCC_CSR_RMVF;
    return cause;
}

const char *reset_cause_name(reset_cause_t cause) {
    switch (cause) {
    case RESET_CAUSE_LOW_POWER:
        return "LOW_POWER";
    case RESET_CAUSE_WWDG:
        return "WWDG";
    case RESET_CAUSE_IWDG:
        return "IWDG";
    case RESET_CAUSE_SOFTWARE:
        return "SOFTWARE";
    case RESET_CAUSE_POR:
        return "POR_PDR";
    case RESET_CAUSE_PIN:
        return "NRST_PIN";
    default:
        return "UNKNOWN";
    }
}
