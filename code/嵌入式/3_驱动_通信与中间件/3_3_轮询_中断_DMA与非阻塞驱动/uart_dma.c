#include "uart_dma.h"

void uart_dma1_init(struct uart_dma_handle *h,
                    uint32_t pclk2_hz,
                    uint32_t baudrate,
                    dma_rx_on_data_fn rx_on_data,
                    void *rx_user) {
    *h = (struct uart_dma_handle){0};
    h->rx_on_data = rx_on_data;
    h->rx_user = rx_user;

    /* 1. 开启 GPIOA、USART1 与 DMA1 时钟 */
    RCC->APB2ENR |= RCC_APB2ENR_IOPAEN | RCC_APB2ENR_USART1EN;
    RCC->AHBENR |= RCC_AHBENR_DMA1EN;

    /* 2. PA9 复用推挽输出（50 MHz），PA10 浮空输入 */
    GPIOA->CRH &= ~((0xFu << 4) | (0xFu << 8));
    GPIOA->CRH |= (0xBu << 4) | (0x4u << 8);

    /* 3. 配置 DMA1 通道 5 用于 USART1_RX 循环接收 */
    DMA1_Channel5->CCR = 0u;
    DMA1->IFCR = DMA_IFCR_CGIF5;
    DMA1_Channel5->CPAR = (uint32_t)(uintptr_t)&USART1->DR;
    DMA1_Channel5->CMAR = (uint32_t)(uintptr_t)h->rx_dma_buf;
    DMA1_Channel5->CNDTR = UART_DMA_RX_BUF_SIZE;
    DMA1_Channel5->CCR = DMA_CCR_MINC |
                         DMA_CCR_CIRC |
                         DMA_CCR_HTIE |
                         DMA_CCR_TCIE |
                         DMA_CCR_EN;

    /* 4. 配置 DMA1 通道 4 的外设地址（USART1_TX），发送时再设 CMAR/CNDTR */
    DMA1_Channel4->CCR = 0u;
    DMA1->IFCR = DMA_IFCR_CGIF4;
    DMA1_Channel4->CPAR = (uint32_t)(uintptr_t)&USART1->DR;

    /* 5. 配置 USART1：115200 8N1，开启 DMAT、DMAR 与空闲中断 IDLEIE */
    USART1->CR1 = 0u;
    USART1->CR2 = 0u;
    USART1->CR3 = USART_CR3_DMAT | USART_CR3_DMAR;
    USART1->BRR = (pclk2_hz + baudrate / 2u) / baudrate;
    USART1->CR1 = USART_CR1_UE |
                  USART_CR1_TE |
                  USART_CR1_RE |
                  USART_CR1_IDLEIE;
}

enum uart_dma_status uart_dma1_send_async(struct uart_dma_handle *h,
                                          const void *buf,
                                          uint16_t len,
                                          uart_dma_tx_cb_t cb,
                                          void *user_data) {
    if (h == NULL || buf == NULL || len == 0u) {
        return UART_DMA_ERR_PARAM;
    }
    if (h->tx_busy) {
        return UART_DMA_ERR_BUSY;
    }

    h->tx_cb = cb;
    h->tx_user = user_data;
    h->tx_busy = true;

    /* 关闭通道后写入本次缓冲区地址和长度，清除残留标志后重启 */
    DMA1_Channel4->CCR &= ~DMA_CCR_EN;
    DMA1->IFCR = DMA_IFCR_CGIF4 | DMA_IFCR_CTCIF4 |
                 DMA_IFCR_CHTIF4 | DMA_IFCR_CTEIF4;
    USART1->SR = ~USART_SR_TC;

    DMA1_Channel4->CMAR = (uint32_t)(uintptr_t)buf;
    DMA1_Channel4->CNDTR = len;
    DMA1_Channel4->CCR = DMA_CCR_MINC |
                         DMA_CCR_DIR |
                         DMA_CCR_TCIE |
                         DMA_CCR_EN;
    return UART_DMA_OK;
}

bool uart_dma1_tx_busy(const struct uart_dma_handle *h) {
    return h != NULL && h->tx_busy;
}

void uart_dma1_poll_rx(struct uart_dma_handle *h) {
    size_t cndtr = (size_t)DMA1_Channel5->CNDTR;
    dma_rx_drain(h->rx_dma_buf,
                 UART_DMA_RX_BUF_SIZE,
                 &h->rx_old_pos,
                 cndtr,
                 h->rx_on_data,
                 h->rx_user);
}

void uart_dma1_tx_dma_isr(struct uart_dma_handle *h) {
    if ((DMA1->ISR & DMA_ISR_TCIF4) != 0u) {
        DMA1->IFCR = DMA_IFCR_CTCIF4 | DMA_IFCR_CGIF4;
        DMA1_Channel4->CCR &= ~DMA_CCR_EN;
        h->tx_busy = false;
        if (h->tx_cb != NULL) {
            h->tx_cb(h->tx_user);
        }
    }
}

void uart_dma1_rx_dma_isr(struct uart_dma_handle *h) {
    if ((DMA1->ISR & (DMA_ISR_HTIF5 | DMA_ISR_TCIF5)) != 0u) {
        DMA1->IFCR = DMA_IFCR_CHTIF5 | DMA_IFCR_CTCIF5 | DMA_IFCR_CGIF5;
        uart_dma1_poll_rx(h);
    }
}

void uart_dma1_usart_isr(struct uart_dma_handle *h) {
    uint32_t sr = USART1->SR;
    if ((sr & USART_SR_IDLE) != 0u) {
        /* 读 SR 后再读 DR 才能清除 IDLE 标志 */
        (void)USART1->DR;
        uart_dma1_poll_rx(h);
    }
}
