#include "frame.h"
#include "ringbuf.h"
#include "stm32f1xx.h"
#include "uart_dma.h"
#include <stddef.h>
#include <stdint.h>

static struct uart_dma_handle g_uart1;
static uint8_t g_rx_rb_mem[256];
static struct ringbuf g_rx_rb;
static struct frame_parser g_parser;
static struct device_state g_dev;

/* 发送缓冲区采用静态存储期，保证 DMA 传输期间一直有效 */
static uint8_t g_tx_frame_buf[FRAME_MAX_WIRE];

/* 在 DMA 半传输/全传输/USART IDLE 中断回调中作为唯一生产者写入环形缓冲区 */
static void on_uart_rx_chunk(const uint8_t *data, size_t len, void *user_data) {
    struct ringbuf *rb = (struct ringbuf *)user_data;
    for (size_t i = 0u; i < len; ++i) {
        (void)rb_put(rb, data[i]);
    }
}

void DMA1_Channel4_IRQHandler(void) {
    uart_dma1_tx_dma_isr(&g_uart1);
}

void DMA1_Channel5_IRQHandler(void) {
    uart_dma1_rx_dma_isr(&g_uart1);
}

void USART1_IRQHandler(void) {
    uart_dma1_usart_isr(&g_uart1);
}

static void board_led_init(void) {
    RCC->APB2ENR |= RCC_APB2ENR_IOPCEN;
    GPIOC->CRH &= ~(0xFu << 20);
    GPIOC->CRH |= (0x2u << 20);
    GPIOC->BSRR = GPIO_BSRR_BS13; /* PC13 低电平点亮，默认熄灭 */
}

static void board_led_apply(uint8_t on) {
    if (on != 0u) {
        GPIOC->BSRR = GPIO_BSRR_BR13;
    } else {
        GPIOC->BSRR = GPIO_BSRR_BS13;
    }
}

int main(void) {
    board_led_init();
    rb_init(&g_rx_rb, g_rx_rb_mem, sizeof(g_rx_rb_mem));
    parser_init(&g_parser);

    uart_dma1_init(&g_uart1, 72000000u, 115200u,
                   on_uart_rx_chunk, &g_rx_rb);

    NVIC_SetPriority(DMA1_Channel5_IRQn, 1u);
    NVIC_SetPriority(USART1_IRQn, 1u);
    NVIC_SetPriority(DMA1_Channel4_IRQn, 2u);
    NVIC_EnableIRQ(DMA1_Channel5_IRQn);
    NVIC_EnableIRQ(USART1_IRQn);
    NVIC_EnableIRQ(DMA1_Channel4_IRQn);

    for (;;) {
        uint8_t byte = 0u;
        while (!uart_dma1_tx_busy(&g_uart1) && rb_get(&g_rx_rb, &byte)) {
            struct frame req;
            if (parser_feed(&g_parser, byte, &req) == PARSE_OK) {
                struct frame resp;
                cmd_dispatch(&g_dev, &req, &resp);
                board_led_apply(g_dev.led_on);

                size_t tx_len = frame_encode(g_tx_frame_buf,
                                             sizeof(g_tx_frame_buf),
                                             resp.cmd,
                                             resp.payload,
                                             resp.len);
                if (tx_len > 0u) {
                    (void)uart_dma1_send_async(&g_uart1,
                                               g_tx_frame_buf,
                                               (uint16_t)tx_len,
                                               NULL, NULL);
                    break;
                }
            }
        }
    }
}
