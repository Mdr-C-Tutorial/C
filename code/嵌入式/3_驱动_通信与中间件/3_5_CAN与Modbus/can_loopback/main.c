#include "can_drv.h"
#include "stm32f1xx.h"
#include <stdint.h>

static void clock_init_72mhz_local(void) {
    RCC->CR |= RCC_CR_HSEON;
    while ((RCC->CR & RCC_CR_HSERDY) == 0u) {
    }

    FLASH->ACR = (FLASH->ACR & ~FLASH_ACR_LATENCY) |
                 FLASH_ACR_LATENCY_2 | FLASH_ACR_PRFTBE;

    uint32_t mask = RCC_CFGR_HPRE | RCC_CFGR_PPRE1 | RCC_CFGR_PPRE2 |
                    RCC_CFGR_ADCPRE | RCC_CFGR_PLLSRC | RCC_CFGR_PLLXTPRE |
                    RCC_CFGR_PLLMULL;
    uint32_t val = RCC_CFGR_HPRE_DIV1 | RCC_CFGR_PPRE2_DIV1 |
                   RCC_CFGR_PPRE1_DIV2 | RCC_CFGR_ADCPRE_DIV6 |
                   RCC_CFGR_PLLSRC | RCC_CFGR_PLLMULL9;
    RCC->CFGR = (RCC->CFGR & ~mask) | val;

    RCC->CR |= RCC_CR_PLLON;
    while ((RCC->CR & RCC_CR_PLLRDY) == 0u) {
    }

    RCC->CFGR = (RCC->CFGR & ~RCC_CFGR_SW) | RCC_CFGR_SW_PLL;
    while ((RCC->CFGR & RCC_CFGR_SWS) != RCC_CFGR_SWS_PLL) {
    }
}

static void led_pc13_init(void) {
    RCC->APB2ENR |= RCC_APB2ENR_IOPCEN;
    GPIOC->CRH = (GPIOC->CRH & ~(0xFu << 20)) | (0x2u << 20);
    GPIOC->BSRR = GPIO_BSRR_BS13; /* 默认熄灭 */
}

int main(void) {
    clock_init_72mhz_local();
    led_pc13_init();

    /* 开启 LBKM + SILM 单板回环静默自测，无需外接 CAN 收发器 */
    if (!can1_init_500k(true)) {
        for (;;) {
        }
    }

    struct can_frame tx = {
        .id = 0x123u,
        .is_ext = 0u,
        .is_rtr = 0u,
        .dlc = 4u,
        .data = { 0x11u, 0x22u, 0x33u, 0x44u },
    };
    (void)can1_send(&tx);

    for (;;) {
        struct can_frame rx;
        if (can1_receive_fifo0(&rx)) {
            if (rx.id == 0x123u && rx.dlc == 4u && rx.data[0] == 0x11u) {
                GPIOC->BSRR = GPIO_BSRR_BR13; /* 收到预期回环帧，点亮 PC13 */
            }
        }
    }
}
