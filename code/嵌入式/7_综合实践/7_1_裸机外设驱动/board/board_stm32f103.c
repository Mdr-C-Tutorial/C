/*
 * board_stm32f103.c：Blue Pill 板级初始化。
 *
 * 时钟 72 MHz、串口 USART1（PA9/PA10，115200 8N1）、心跳 LED 在 PC13（低电平亮）、
 * I2C1 用 PB6/PB7 上的 GPIO 模拟后端。串口接收进中断，写入 256 字节的环形缓冲区，
 * 主循环从这里按行取。
 */
#include <stdio.h>
#include <string.h>

#include "board.h"

#include "stm32f1xx.h"

#include "i2c_bus.h"

#define CONSOLE_RING_SIZE 256u

static volatile uint8_t g_ring[CONSOLE_RING_SIZE];
static volatile uint32_t g_head; /* 中断写 */
static volatile uint32_t g_tail; /* 主循环读 */
static char g_line[80];
static int g_line_len;

/* CMSIS 声明在 system_stm32f1xx.h 里，本工程不链接 ST 的 system_stm32f1xx.c */
uint32_t SystemCoreClock = 8000000u;

static i2c_bus_t g_i2c_bus;

/* SysTick 的 1 ms 节拍；tick_now 供应用做周期调度 */
static volatile uint32_t g_ms;

void SysTick_Handler(void) {
    ++g_ms;
}

uint32_t board_tick_now(void) {
    return g_ms;
}

static void tick_init(void) {
    SysTick_Config(72000000u / 1000u);
}

void USART1_IRQHandler(void) {
    if ((USART1->SR & USART_SR_RXNE) != 0u) {
        uint8_t byte = (uint8_t)USART1->DR;
        uint32_t next = (g_head + 1u) % CONSOLE_RING_SIZE;
        if (next != g_tail) {
            g_ring[g_head] = byte;
            g_head = next;
        }
        /* 满了就丢新字节：命令行丢一两个字符不影响使用，不能在这里等 */
    }
}

void board_led_set(int on) {
    /* PC13 低电平亮 */
    if (on != 0) {
        GPIOC->BSRR = GPIO_BSRR_BR13;
    } else {
        GPIOC->BSRR = GPIO_BSRR_BS13;
    }
}

void board_led_toggle(void) {
    static int state;
    state = !state;
    board_led_set(state);
}

void board_console_init(void) {
    RCC->APB2ENR |= RCC_APB2ENR_IOPAEN | RCC_APB2ENR_USART1EN;

    /* PA9：50 MHz 复用推挽输出；PA10：浮空输入 */
    GPIOA->CRH = (GPIOA->CRH & ~(0xFFu << 4)) | (0x4Bu << 4);

    USART1->BRR = (72000000u + 115200u / 2u) / 115200u;
    USART1->CR2 = 0;
    USART1->CR3 = 0;
    USART1->CR1 = USART_CR1_TE | USART_CR1_RE | USART_CR1_RXNEIE | USART_CR1_UE;

    NVIC_SetPriority(USART1_IRQn, 2u);
    NVIC_EnableIRQ(USART1_IRQn);
}

int board_console_try_read_line(char *buf, int max) {
    while (g_tail != g_head) {
        char c = (char)g_ring[g_tail];
        g_tail = (g_tail + 1u) % CONSOLE_RING_SIZE;

        if (c == '\r') {
            continue;
        }
        if (c == '\n') {
            int len = g_line_len;
            g_line[g_line_len] = '\0';
            g_line_len = 0;
            if (len > max - 1) {
                len = max - 1;
            }
            memcpy(buf, g_line, (size_t)len + 1u);
            return 1;
        }
        /* 退格 */
        if (c == '\b' || (uint8_t)c == 0x7Fu) {
            if (g_line_len > 0) {
                --g_line_len;
            }
            continue;
        }
        if (g_line_len < (int)sizeof g_line - 1) {
            g_line[g_line_len++] = c;
        }
    }
    return 0;
}

/* printf 重定向到 USART1 */
int _write(int fd, const char *buf, int len) {
    (void)fd;
    for (int i = 0; i < len; ++i) {
        if (buf[i] == '\n') {
            while ((USART1->SR & USART_SR_TXE) == 0) {
            }
            USART1->DR = (uint8_t)'\r';
        }
        while ((USART1->SR & USART_SR_TXE) == 0) {
        }
        USART1->DR = (uint8_t)buf[i];
    }
    return len;
}

const i2c_bus_t *board_i2c_bus(void) {
    return &g_i2c_bus;
}

void clock_init_72mhz(void) {
    RCC->CR |= RCC_CR_HSEON;
    while ((RCC->CR & RCC_CR_HSERDY) == 0) {
    }

    FLASH->ACR = (FLASH->ACR & ~FLASH_ACR_LATENCY) | FLASH_ACR_LATENCY_2 | FLASH_ACR_PRFTBE;

    RCC->CFGR = (RCC->CFGR & ~(RCC_CFGR_HPRE | RCC_CFGR_PPRE1 | RCC_CFGR_PPRE2 |
                               RCC_CFGR_ADCPRE | RCC_CFGR_PLLSRC | RCC_CFGR_PLLMULL)) |
                RCC_CFGR_HPRE_DIV1 | RCC_CFGR_PPRE2_DIV1 | RCC_CFGR_PPRE1_DIV2 |
                RCC_CFGR_ADCPRE_DIV6 | RCC_CFGR_PLLSRC | RCC_CFGR_PLLMULL9;

    RCC->CR |= RCC_CR_PLLON;
    while ((RCC->CR & RCC_CR_PLLRDY) == 0) {
    }

    RCC->CFGR = (RCC->CFGR & ~RCC_CFGR_SW) | RCC_CFGR_SW_PLL;
    while ((RCC->CFGR & RCC_CFGR_SWS) != RCC_CFGR_SWS_PLL) {
    }

    SystemCoreClock = 72000000u;
}

const char *board_reset_cause_name(void) {
    uint32_t csr = RCC->CSR;
    const char *name = "未知";

    if ((csr & RCC_CSR_LPWRRSTF) != 0u) {
        name = "低功耗";
    } else if ((csr & RCC_CSR_WWDGRSTF) != 0u) {
        name = "窗口看门狗";
    } else if ((csr & RCC_CSR_IWDGRSTF) != 0u) {
        name = "独立看门狗";
    } else if ((csr & RCC_CSR_SFTRSTF) != 0u) {
        name = "软件复位";
    } else if ((csr & RCC_CSR_PORRSTF) != 0u) {
        name = "上电";
    } else if ((csr & RCC_CSR_PINRSTF) != 0u) {
        name = "复位引脚";
    }
    RCC->CSR |= RCC_CSR_RMVF;
    return name;
}

void board_init(void) {
    /* 1. LED：PC13 推挽输出，复位后默认灭 */
    RCC->APB2ENR |= RCC_APB2ENR_IOPCEN;
    GPIOC->BSRR = GPIO_BSRR_BS13;
    /* PC13：2 MHz 推挽输出（CNF=00, MODE=10 -> 0x2） */
    GPIOC->CRH = (GPIOC->CRH & ~(0xFu << 20)) | (0x2u << 20);

    clock_init_72mhz();
    tick_init();

    board_console_init();
    i2c_soft_create(&g_i2c_bus);
}