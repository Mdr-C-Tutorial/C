/*
 * uart_rx_rtos.c：USART1 空闲中断 + DMA1 通道 5 循环接收，经流缓冲区交给命令行任务。
 *
 * 讲解见教程 4.4 节「中断与任务协作」。
 *
 * 接收路径沿用 3.3 节的做法：DMA 把字节搬进 64 字节循环缓冲区，空闲线中断
 * 与 DMA 半满/全满中断算出“上次位置到当前位置”这一段，然后把它塞进流缓冲区。
 * 任务侧只调用 uart_read_line()，在流缓冲区上阻塞。
 */
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "FreeRTOS.h"
#include "queue.h"
#include "stream_buffer.h"
#include "task.h"

#include "stm32f1xx.h"

#define UART_IRQ_PRIO   5
#define RX_DMA_BUF_SIZE 64u
#define RX_STREAM_SIZE  256u
#define LINE_MAX        64u

static uint8_t g_rx_dma_buf[RX_DMA_BUF_SIZE];
static size_t g_rx_old_pos;
static StreamBufferHandle_t g_rx_stream;

volatile uint32_t g_last_cmd_len;

/* 把上次位置到 DMA 当前位置之间的字节送进流缓冲区。中断上下文调用 */
static void uart_rx_push_from_isr(void) {
    size_t pos = RX_DMA_BUF_SIZE - DMA1_Channel5->CNDTR;
    BaseType_t higher_woken = pdFALSE;

    if (pos == g_rx_old_pos) {
        return;
    }

    if (pos > g_rx_old_pos) {
        xStreamBufferSendFromISR(g_rx_stream, &g_rx_dma_buf[g_rx_old_pos],
                                 pos - g_rx_old_pos, &higher_woken);
    } else {
        /* 绕回缓冲区开头，分两段送 */
        xStreamBufferSendFromISR(g_rx_stream, &g_rx_dma_buf[g_rx_old_pos],
                                 RX_DMA_BUF_SIZE - g_rx_old_pos, &higher_woken);
        xStreamBufferSendFromISR(g_rx_stream, g_rx_dma_buf, pos, &higher_woken);
    }
    g_rx_old_pos = pos;

    portYIELD_FROM_ISR(higher_woken);
}

/* USART1 空闲线中断：一帧数据后面没有新字节时触发 */
void USART1_IRQHandler(void) {
    uint32_t sr = USART1->SR;

    if ((sr & USART_SR_IDLE) != 0u) {
        /* 先读 SR 再读 DR 才能清 IDLE 标志 */
        (void)USART1->DR;
        uart_rx_push_from_isr();
    }
}

/* DMA1 通道 5 半满/全满中断：缓冲区绕回时兜底 */
void DMA1_Channel5_IRQHandler(void) {
    if ((DMA1->ISR & (DMA_ISR_HTIF5 | DMA_ISR_TCIF5)) != 0u) {
        DMA1->IFCR = DMA_IFCR_CHTIF5 | DMA_IFCR_CTCIF5 | DMA_IFCR_CGIF5;
        uart_rx_push_from_isr();
    }
}

static void uart_rx_dma_start(uint32_t pclk2_hz, uint32_t baudrate) {
    RCC->APB2ENR |= RCC_APB2ENR_IOPAEN | RCC_APB2ENR_USART1EN;
    RCC->AHBENR |= RCC_AHBENR_DMA1EN;

    /* PA9 复用推挽输出 50 MHz，PA10 浮空输入 */
    GPIOA->CRH = (GPIOA->CRH & ~((0xFu << 4) | (0xFu << 8))) |
                 (0xBu << 4) | (0x4u << 8);

    DMA1_Channel5->CCR = 0u;
    DMA1->IFCR = DMA_IFCR_CGIF5;
    DMA1_Channel5->CPAR = (uint32_t)(uintptr_t)&USART1->DR;
    DMA1_Channel5->CMAR = (uint32_t)(uintptr_t)g_rx_dma_buf;
    DMA1_Channel5->CNDTR = RX_DMA_BUF_SIZE;
    DMA1_Channel5->CCR = DMA_CCR_MINC | DMA_CCR_CIRC | DMA_CCR_HTIE |
                         DMA_CCR_TCIE | DMA_CCR_EN;

    USART1->CR1 = 0u;
    USART1->CR2 = 0u;
    USART1->CR3 = USART_CR3_DMAR;
    USART1->BRR = (pclk2_hz + baudrate / 2u) / baudrate;
    USART1->CR1 = USART_CR1_UE | USART_CR1_TE | USART_CR1_RE | USART_CR1_IDLEIE;

    NVIC_SetPriority(USART1_IRQn, UART_IRQ_PRIO);
    NVIC_SetPriority(DMA1_Channel5_IRQn, UART_IRQ_PRIO);
    NVIC_EnableIRQ(USART1_IRQn);
    NVIC_EnableIRQ(DMA1_Channel5_IRQn);
}

/* 任务侧：阻塞读一行，超时返回 0 */
static size_t uart_read_line(char *line, size_t cap) {
    size_t used = 0;

    while (used < cap - 1u) {
        char byte;
        if (xStreamBufferReceive(g_rx_stream, &byte, 1, pdMS_TO_TICKS(100)) != 1u) {
            return 0;
        }
        if (byte == '\n') {
            if (used > 0u && line[used - 1u] == '\r') {
                used--;
            }
            line[used] = '\0';
            return used;
        }
        line[used++] = byte;
    }
    line[used] = '\0';
    return used;
}

static void cli_task(void *param) {
    (void)param;
    char line[LINE_MAX];

    uart_rx_dma_start(72000000u, 115200u);

    for (;;) {
        size_t len = uart_read_line(line, sizeof line);
        if (len > 0u) {
            /* 命令解析在 4.9 节展开，这里只记录最近一条的长度供调试器观察 */
            g_last_cmd_len = (uint32_t)len;
        }
    }
}

int main(void) {
    g_rx_stream = xStreamBufferCreate(RX_STREAM_SIZE, 1);
    if (g_rx_stream == NULL) {
        return 1;
    }
    if (xTaskCreate(cli_task, "cli", 256, NULL, 2, NULL) != pdPASS) {
        return 1;
    }
    vTaskStartScheduler();
    for (;;) {
    }
}