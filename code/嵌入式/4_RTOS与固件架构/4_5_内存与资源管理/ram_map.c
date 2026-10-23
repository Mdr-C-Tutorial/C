/*
 * ram_map.c：观察 FreeRTOS 固件在 STM32F103C8（20 KiB SRAM）中的内存分布。
 * 编译链接后使用 arm-none-eabi-size -A 与 arm-none-eabi-nm --size-sort 查看。
 */
#include "FreeRTOS.h"
#include "task.h"
#include <stdint.h>

/* .rodata：只读查找表，留在 Flash 中，不占 SRAM */
const uint16_t g_filter_coeffs[16] = {
    32, 64, 128, 256, 512, 1024, 2048, 4096,
    4096, 2048, 1024, 512, 256, 128, 64, 32
};

/* .data：有非零初值的全局配置对象，占 16 字节 SRAM（初值在 Flash 中） */
int32_t g_accel_bias[4] = {12, -8, 1024, 0};

/* .bss：零初值的串口 DMA 接收双缓冲区，占 256 字节 SRAM */
uint8_t g_uart_rx_buf[256];

/* .bss：静态创建的采样任务栈（128 字 = 512 字节）与任务控制块 */
static StackType_t g_sensor_stack[128];
static StaticTask_t g_sensor_tcb;

static void sensor_task(void *arg) {
    (void)arg;
    TickType_t last_wake = xTaskGetTickCount();
    for (;;) {
        g_uart_rx_buf[0] = (uint8_t)(g_accel_bias[0] + g_filter_coeffs[0]);
        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(10));
    }
}

static void comm_task(void *arg) {
    (void)arg;
    for (;;) {
        g_accel_bias[3]++;
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName) {
    (void)xTask;
    (void)pcTaskName;
    for (;;) {
    }
}

int main(void) {
    /* 静态创建高优先级采样任务：栈与 TCB 位于各自的 .bss 符号中 */
    xTaskCreateStatic(sensor_task, "sensor", 128, NULL, 3,
                      g_sensor_stack, &g_sensor_tcb);

    /* 动态创建通信任务：栈（256 字 = 1024 字节）与 TCB 从 ucHeap 中切出 */
    xTaskCreate(comm_task, "comm", 256, NULL, 2, NULL);

    vTaskStartScheduler();
    for (;;) {
    }
}
