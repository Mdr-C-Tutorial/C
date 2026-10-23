/*
 * uart_cli_sim.c：用一个 Windows 线程充当串口外设，把收到的字节经流缓冲区交给命令行任务。
 *
 * 讲解见教程 4.4 节「中断与任务协作」。
 *
 * 结构对应目标上的 USART1 空闲中断 + DMA：中断里只把新数据塞进流缓冲区，
 * 任务在流缓冲区上阻塞，按行解析命令。模拟器上用
 * vPortGenerateSimulatedInterruptFromWindowsThread() 从 Windows 线程触发模拟
 * 中断；中断处理函数以返回值代替 portYIELD_FROM_ISR。
 */
#include <stdint.h>
#include <string.h>
#include <windows.h>

#include "FreeRTOS.h"
#include "task.h"
#include "stream_buffer.h"

#include "rtos_console.h"

#define UART_IRQ_NUMBER 3u
#define STREAM_BUF_SIZE 128u
#define LINE_MAX        64u

static StreamBufferHandle_t g_rx_stream;
static HANDLE g_irq_acked;
static const char *volatile g_pending_data;
static volatile size_t g_pending_len;
static volatile int g_uart_done;

/* 模拟“串口外设”发来的一段字节，由模拟中断线程调用 */
static uint32_t uart_isr(void) {
    BaseType_t higher_woken = pdFALSE;

    if (g_pending_len != 0u) {
        /* 流缓冲区只有一个写者（中断）和一个读者（命令行任务），不必额外加锁 */
        xStreamBufferSendFromISR(g_rx_stream, g_pending_data, g_pending_len,
                                 &higher_woken);
        g_pending_data = NULL;
        g_pending_len = 0u;
    }

    /* 告诉 Windows 线程：这一段已经交给任务了 */
    SetEvent(g_irq_acked);

    /* 模拟器用返回值代替目标板上的 portYIELD_FROM_ISR(higher_woken) */
    return (uint32_t)higher_woken;
}

static void cli_task(void *param) {
    (void)param;

    char line[LINE_MAX];
    size_t used = 0;

    for (;;) {
        char byte;

        /* 一次只取一个字节，方便按行切分；数据没到时任务在这里阻塞 */
        size_t got = xStreamBufferReceive(g_rx_stream, &byte, 1, pdMS_TO_TICKS(200));
        if (got == 1u) {
            if (byte == '\n') {
                if (used > 0u && line[used - 1] == '\r') {
                    used--;
                }
                line[used] = '\0';
                if (used > 0u) {
                    console_printf("[cli ] 执行命令：\"%s\"\n", line);
                }
                used = 0u;
            } else if (used < LINE_MAX - 1u) {
                line[used++] = byte;
            }
        }

        if (g_uart_done != 0 && xStreamBufferIsEmpty(g_rx_stream) != 0) {
            console_printf("[cli ] 外设停止发送，命令行任务退出\n");
            break;
        }
    }

    g_uart_done = 2;
    sim_finish();
}

/* 一段“串口数据”分几次到达的脚本；"\r\n" 和 "\n" 两种行尾都要能处理 */
static const char *const g_chunks[] = {
    "led o",
    "n\r\n",
    "help\n",
    "led off\r\n",
};

static DWORD WINAPI uart_thread(LPVOID param) {
    (void)param;

    for (size_t i = 0; i < sizeof(g_chunks) / sizeof(g_chunks[0]); ++i) {
        Sleep(30);

        g_pending_data = g_chunks[i];
        g_pending_len = strlen(g_chunks[i]);

        ResetEvent(g_irq_acked);
        vPortGenerateSimulatedInterruptFromWindowsThread(UART_IRQ_NUMBER);
        WaitForSingleObject(g_irq_acked, 1000);
    }

    g_uart_done = 1;
    return 0;
}

int main(void) {
    g_rx_stream = xStreamBufferCreate(STREAM_BUF_SIZE, 1);
    g_irq_acked = CreateEvent(NULL, TRUE, FALSE, NULL);
    if (g_rx_stream == NULL || g_irq_acked == NULL) {
        return 1;
    }

    vPortSetInterruptHandler(UART_IRQ_NUMBER, uart_isr);

    if (xTaskCreate(cli_task, "cli", 512, NULL, 2, NULL) != pdPASS) {
        return 1;
    }

    HANDLE thread = CreateThread(NULL, 0, uart_thread, NULL, 0, NULL);
    if (thread == NULL) {
        return 1;
    }

    vTaskStartScheduler();
    return 0;
}