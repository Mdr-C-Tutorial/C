#include <stdint.h>
#include "clock.h"
#include "console.h"
#include "cli_parse.h"
#include "stm32f1xx.h"

#define LINE_BUF_SIZE 64u

static char g_line_buf[LINE_BUF_SIZE];
static uint32_t g_line_len = 0;
static volatile uint8_t g_line_ready = 0;

static void led_init(void) {
    RCC->APB2ENR |= RCC_APB2ENR_IOPCEN;
    GPIOC->CRH = (GPIOC->CRH & ~(0xFu << 20)) | (0x2u << 20);
    GPIOC->BSRR = GPIO_BSRR_BS13; /* PC13 低电平亮，初始熄灭 */
}

static void led_set(int on) {
    if (on) {
        GPIOC->BSRR = GPIO_BSRR_BR13;
    } else {
        GPIOC->BSRR = GPIO_BSRR_BS13;
    }
}

void USART1_IRQHandler(void) {
    uint32_t sr = USART1->SR;
    if ((sr & (USART_SR_RXNE | USART_SR_ORE)) != 0) {
        char c = (char)(USART1->DR & 0xFFu); /* 读 SR 后读 DR 同时清除 RXNE 与 ORE */
        if (g_line_ready != 0) {
            return; /* 上一行尚未由主循环处理完，暂丢弃新字符 */
        }
        if (c == '\r' || c == '\n') {
            g_line_buf[g_line_len] = '\0';
            g_line_len = 0;
            g_line_ready = 1;
        } else if (g_line_len + 1u < LINE_BUF_SIZE) {
            g_line_buf[g_line_len++] = c;
        }
    }
}

int main(void) {
    clock_init_72mhz();
    led_init();
    console_init(72000000u, 115200u);

    USART1->CR1 |= USART_CR1_RXNEIE;
    NVIC_SetPriority(USART1_IRQn, 2u);
    NVIC_EnableIRQ(USART1_IRQn);

    cli_state st = { .led_on = 0, .rx_lines = 0 };
    char reply[96];

    uart_puts("CLI ready. Commands: led on | led off | status\n> ");

    for (;;) {
        if (g_line_ready != 0) {
            cli_handle_line(g_line_buf, &st, reply, sizeof reply);
            led_set(st.led_on);
            g_line_ready = 0;
            if (reply[0] != '\0') {
                uart_puts(reply);
            }
            uart_puts("> ");
        }
    }
}
