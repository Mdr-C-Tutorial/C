#include "uart_dma.h"
#include "uart_it.h"
#include <string.h>

static struct uart_dma_handle g_uart1;
static volatile bool g_rx_event;
static uint8_t g_app_buf[128];
static size_t g_app_len;

/* 发送缓冲区放在静态存储期对象中，保证 DMA 传输完成前一直有效 */
static uint8_t g_tx_frame[128];

static void on_rx_chunk(const uint8_t *data, size_t len, void *user_data) {
    (void)user_data;
    size_t space = sizeof(g_app_buf) - g_app_len;
    size_t copy_len = len < space ? len : space;
    if (copy_len > 0u) {
        memcpy(&g_app_buf[g_app_len], data, copy_len);
        g_app_len += copy_len;
        g_rx_event = true;
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

int main(void) {
    uart_dma1_init(&g_uart1, 72000000u, 115200u, on_rx_chunk, NULL);

    NVIC_SetPriority(DMA1_Channel5_IRQn, 1u);
    NVIC_SetPriority(USART1_IRQn, 1u);
    NVIC_SetPriority(DMA1_Channel4_IRQn, 2u);
    NVIC_EnableIRQ(DMA1_Channel5_IRQn);
    NVIC_EnableIRQ(USART1_IRQn);
    NVIC_EnableIRQ(DMA1_Channel4_IRQn);

    static const char banner[] = "UART1 DMA circular RX + IDLE ready\r\n";
    (void)uart_dma1_send_async(&g_uart1, banner,
                               (uint16_t)(sizeof(banner) - 1u),
                               NULL, NULL);

    for (;;) {
        if (g_rx_event && !uart_dma1_tx_busy(&g_uart1)) {
            __disable_irq();
            size_t n = g_app_len;
            memcpy(g_tx_frame, g_app_buf, n);
            g_app_len = 0u;
            g_rx_event = false;
            __enable_irq();

            if (n > 0u) {
                (void)uart_dma1_send_async(&g_uart1, g_tx_frame,
                                           (uint16_t)n, NULL, NULL);
            }
        }
    }
}
