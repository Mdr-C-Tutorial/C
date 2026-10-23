/*
 * uart_tx_blocking.c：把 DMA 发送 + 传输完成中断包装成任务可以阻塞等待的 uart_write()。
 *
 * 讲解见教程 4.4 节「中断与任务协作」。
 *
 * uart_write() 启动 DMA 发送，然后在任务通知上等；DMA1 通道 4 的传输完成
 * 中断里 vTaskNotifyGiveFromISR() 唤醒调用者。外层的互斥量保证同一时刻只有
 * 一个任务使用串口，任务是唯一调用者，所以通知不会发给别人。
 */
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "FreeRTOS.h"
#include "semphr.h"
#include "task.h"

#include "stm32f1xx.h"

#define UART_TX_PRIO     5
#define UART_TX_TIMEOUT  pdMS_TO_TICKS(50)

/* 传输完成通知必须发给发起发送的那个任务，所以记下它的句柄 */
static TaskHandle_t g_tx_waiter;
static SemaphoreHandle_t g_uart_mutex;

/* 发送完成中断 */
void DMA1_Channel4_IRQHandler(void) {
    BaseType_t higher_woken = pdFALSE;

    if ((DMA1->ISR & DMA_ISR_TCIF4) != 0u) {
        DMA1->IFCR = DMA_IFCR_CTCIF4 | DMA_IFCR_CGIF4;
        DMA1_Channel4->CCR &= ~DMA_CCR_EN;

        if (g_tx_waiter != NULL) {
            vTaskNotifyGiveFromISR(g_tx_waiter, &higher_woken);
        }
        portYIELD_FROM_ISR(higher_woken);
    }
}

static void uart_tx_init(uint32_t pclk2_hz, uint32_t baudrate) {
    RCC->APB2ENR |= RCC_APB2ENR_IOPAEN | RCC_APB2ENR_USART1EN;
    RCC->AHBENR |= RCC_AHBENR_DMA1EN;

    GPIOA->CRH = (GPIOA->CRH & ~(0xFu << 4)) | (0xBu << 4);

    DMA1_Channel4->CCR = 0u;
    DMA1->IFCR = DMA_IFCR_CGIF4;
    DMA1_Channel4->CPAR = (uint32_t)(uintptr_t)&USART1->DR;

    USART1->CR1 = 0u;
    USART1->CR2 = 0u;
    USART1->CR3 = USART_CR3_DMAT;
    USART1->BRR = (pclk2_hz + baudrate / 2u) / baudrate;
    USART1->CR1 = USART_CR1_UE | USART_CR1_TE | USART_CR1_RE;

    NVIC_SetPriority(DMA1_Channel4_IRQn, UART_TX_PRIO);
    NVIC_EnableIRQ(DMA1_Channel4_IRQn);
}

/* 任务接口：阻塞到发送完成，成功返回 0，超时返回 -1 */
static int uart_write(const char *buf, size_t len) {
    if (xSemaphoreTake(g_uart_mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        return -1;
    }

    /* 通知计数先清零，避免上一次发送留下的计数让本次立刻返回 */
    ulTaskNotifyTake(pdTRUE, 0);

    g_tx_waiter = xTaskGetCurrentTaskHandle();

    DMA1_Channel4->CCR &= ~DMA_CCR_EN;
    DMA1->IFCR = DMA_IFCR_CGIF4 | DMA_IFCR_CTCIF4;
    DMA1_Channel4->CMAR = (uint32_t)(uintptr_t)buf;
    DMA1_Channel4->CNDTR = (uint32_t)len;
    DMA1_Channel4->CCR = DMA_CCR_MINC | DMA_CCR_DIR | DMA_CCR_TCIE | DMA_CCR_EN;

    uint32_t notified = ulTaskNotifyTake(pdTRUE, UART_TX_TIMEOUT);
    g_tx_waiter = NULL;

    xSemaphoreGive(g_uart_mutex);

    return notified > 0u ? 0 : -1;
}

static void sender_task(void *param) {
    (void)param;
    static const char msg[] = "telemetry seq=1\r\n";

    uart_tx_init(72000000u, 115200u);
    g_uart_mutex = xSemaphoreCreateMutex();

    for (;;) {
        (void)uart_write(msg, sizeof msg - 1u);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

int main(void) {
    if (xTaskCreate(sender_task, "tx", 256, NULL, 2, NULL) != pdPASS) {
        return 1;
    }
    vTaskStartScheduler();
    for (;;) {
    }
}