#include "at_parser.h"
#include "ringbuf.h"
#include "stm32f1xx.h"
#include <stddef.h>
#include <stdint.h>
#include <string.h>

static volatile uint32_t g_ms;
static uint8_t g_at_rx_mem[256];
static struct ringbuf g_at_rx_rb;
static struct at_client g_at;

enum demo_step {
    STEP_WAIT_READY = 0,
    STEP_ATE0,
    STEP_CWMODE,
    STEP_CWJAP,
    STEP_CIPSTART,
    STEP_CIPSEND,
    STEP_IDLE,
};

static enum demo_step g_step = STEP_WAIT_READY;
static bool g_step_in_flight;

void SysTick_Handler(void) {
    ++g_ms;
}

static void clock_init_72mhz(void) {
    RCC->CR |= RCC_CR_HSEON;
    while ((RCC->CR & RCC_CR_HSERDY) == 0u) {
    }
    FLASH->ACR = 0x12u;
    RCC->CFGR = RCC_CFGR_PPRE1_DIV2 | RCC_CFGR_ADCPRE_DIV6 |
                RCC_CFGR_PLLSRC | RCC_CFGR_PLLMULL9;
    RCC->CR |= RCC_CR_PLLON;
    while ((RCC->CR & RCC_CR_PLLRDY) == 0u) {
    }
    RCC->CFGR = (RCC->CFGR & ~RCC_CFGR_SW) | RCC_CFGR_SW_PLL;
    while ((RCC->CFGR & RCC_CFGR_SWS) != RCC_CFGR_SWS_PLL) {
    }
}

static void tick_init(void) {
    SysTick->LOAD = 72000u - 1u;
    SysTick->VAL = 0u;
    SysTick->CTRL = 7u;
}

/* USART1（PA9/PA10，PCLK2 = 72 MHz）：接 CH340 打印调试日志 */
static void console_init(void) {
    RCC->APB2ENR |= RCC_APB2ENR_IOPAEN | RCC_APB2ENR_USART1EN;
    GPIOA->CRH = (GPIOA->CRH & ~(0xFFu << 4)) | (0x4Bu << 4);
    USART1->BRR = (72000000u + 115200u / 2u) / 115200u;
    USART1->CR1 = USART_CR1_UE | USART_CR1_TE;
}

static void console_puts(const char *s) {
    while (*s != '\0') {
        while ((USART1->SR & USART_SR_TXE) == 0u) {
        }
        USART1->DR = (uint8_t)*s++;
    }
}

/* USART3（PB10 TX -> ESP GPIO6，PB11 RX <- ESP GPIO7，PCLK1 = 36 MHz） */
static void usart3_at_init(uint32_t baudrate) {
    RCC->APB2ENR |= RCC_APB2ENR_IOPBEN;
    RCC->APB1ENR |= RCC_APB1ENR_USART3EN;

    /* PB10：复用推挽输出 50 MHz（0xB）；PB11：上拉输入（0x8，ODR11=1） */
    GPIOB->CRH = (GPIOB->CRH & ~(0xFFu << 8)) | (0x8Bu << 8);
    GPIOB->BSRR = GPIO_BSRR_BS11;

    USART3->CR1 = 0u;
    USART3->CR2 = 0u;
    USART3->CR3 = 0u;
    USART3->BRR = (36000000u + baudrate / 2u) / baudrate;
    USART3->CR1 =
        USART_CR1_UE | USART_CR1_TE | USART_CR1_RE | USART_CR1_RXNEIE;

    NVIC_SetPriority(USART3_IRQn, 1u);
    NVIC_EnableIRQ(USART3_IRQn);
}

void USART3_IRQHandler(void) {
    uint32_t sr = USART3->SR;
    if ((sr & (USART_SR_RXNE | USART_SR_ORE)) != 0u) {
        uint8_t byte = (uint8_t)(USART3->DR & 0xFFu);
        (void)rb_put(&g_at_rx_rb, byte);
    }
}

static void usart3_write(const uint8_t *data, size_t len, void *user_data) {
    (void)user_data;
    for (size_t i = 0u; i < len; ++i) {
        while ((USART3->SR & USART_SR_TXE) == 0u) {
        }
        USART3->DR = data[i];
    }
}

static void on_urc(const char *urc_line, void *user_data) {
    (void)user_data;
    console_puts("[URC] ");
    console_puts(urc_line);
    console_puts("\r\n");

    if (g_step == STEP_WAIT_READY && strcmp(urc_line, "ready") == 0) {
        g_step = STEP_ATE0;
        g_step_in_flight = false;
    }
}

static void on_ipd(const uint8_t *data, size_t len, void *user_data) {
    (void)user_data;
    console_puts("[IPD] ");
    for (size_t i = 0u; i < len; ++i) {
        while ((USART1->SR & USART_SR_TXE) == 0u) {
        }
        USART1->DR = data[i];
    }
    console_puts("\r\n");
}

static void on_prompt(void *user_data) {
    struct at_client *at = (struct at_client *)user_data;
    const uint8_t msg[] = "hello";
    at->write_fn(msg, 5u, at->io_user);
}

static void on_step_done(enum at_result res,
                         const char *info,
                         void *user_data) {
    (void)info;
    (void)user_data;
    g_step_in_flight = false;
    if (res != AT_RES_OK) {
        console_puts("[ERR] step failed: ");
        console_puts(at_result_str(res));
        console_puts("\r\n");
        g_step = STEP_IDLE;
        return;
    }
    if (g_step < STEP_IDLE) {
        g_step = (enum demo_step)((int)g_step + 1);
    }
}

static void drive_steps(void) {
    if (g_step_in_flight) {
        return;
    }
    switch (g_step) {
    case STEP_WAIT_READY:
        if (g_ms >= 3000u) {
            /* 若上电时模块已启动完毕，超时后直接进入 ATE0 */
            g_step = STEP_ATE0;
        }
        break;
    case STEP_ATE0:
        if (at_send(&g_at, "ATE0", 1000u, on_step_done, NULL)) {
            g_step_in_flight = true;
        }
        break;
    case STEP_CWMODE:
        if (at_send(&g_at, "AT+CWMODE=1", 2000u, on_step_done, NULL)) {
            g_step_in_flight = true;
        }
        break;
    case STEP_CWJAP:
        if (at_send(&g_at, "AT+CWJAP=\"LabAP\",\"pass1234\"",
                    15000u, on_step_done, NULL)) {
            g_step_in_flight = true;
        }
        break;
    case STEP_CIPSTART:
        if (at_send(&g_at, "AT+CIPSTART=\"TCP\",\"192.168.1.100\",8080",
                    10000u, on_step_done, NULL)) {
            g_step_in_flight = true;
        }
        break;
    case STEP_CIPSEND:
        if (at_send(&g_at, "AT+CIPSEND=5", 5000u, on_step_done, NULL)) {
            g_step_in_flight = true;
        }
        break;
    case STEP_IDLE:
    default:
        break;
    }
}

int main(void) {
    clock_init_72mhz();
    tick_init();
    console_init();

    rb_init(&g_at_rx_rb, g_at_rx_mem, sizeof(g_at_rx_mem));
    at_client_init(&g_at, &g_at_rx_rb, usart3_write, NULL);
    at_set_callbacks(&g_at, on_urc, on_ipd, on_prompt, &g_at);
    usart3_at_init(115200u);

    console_puts("ESP-AT Wi-Fi TCP demo start\r\n");

    for (;;) {
        at_poll(&g_at, g_ms);
        drive_steps();
    }
}
