/*
 * superloop.c：非阻塞超级循环实现三项周期不同的活动，在 PC 上按虚拟时间运行。
 *
 *   1. LED 每 500 ms 翻转一次
 *   2. 传感器每 10 ms 启动一次转换，转换要 2 ms，用状态机拆开等待
 *   3. 串口字节随时到达，轮询取走并交给命令解析器
 *
 * 时间来源与硬件都换成假的，循环本身与裸机上的写法完全一样。
 * 同一需求的 FreeRTOS 任务版见 rtos_tasks.c。
 */
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#define RUN_MS 1100u

static uint32_t g_now_ms;

/* --- 假的硬件 --- */

static uint32_t tick_now(void) {
    return g_now_ms;
}

static void led_toggle(void) {
    static unsigned toggles;
    toggles++;
    printf("t=%4u ms  LED 翻转（第 %u 次）\n", (unsigned)g_now_ms, toggles);
}

static uint32_t g_conv_start_ms;
static bool g_converting;

static void sensor_start_conv(void) {
    g_converting = true;
    g_conv_start_ms = g_now_ms;
}

static bool sensor_conv_ready(void) {
    return g_converting && (uint32_t)(g_now_ms - g_conv_start_ms) >= 2u;
}

static int16_t sensor_read_raw(void) {
    static const int16_t table[] = { 998, 1006, 1002, 994, 1010, 990 };
    static unsigned index;
    g_converting = false;
    return table[index++ % (sizeof(table) / sizeof(table[0]))];
}

static void filter_push(int16_t sample) {
    static int32_t sum;
    static unsigned count;
    sum += sample;
    count++;
    if (count % 25u == 0u) {
        printf("t=%4u ms  采样 %u 次，均值 %d mg\n",
               (unsigned)g_now_ms, count, (int)(sum / (int32_t)count));
    }
}

/* 串口：第 200、450、700、950 ms 各来一个命令字节 */
static bool uart_try_getc(uint8_t *out_byte) {
    static const uint32_t arrival_ms[] = { 200, 450, 700, 950, 1050 };
    static unsigned next;
    if (next >= sizeof(arrival_ms) / sizeof(arrival_ms[0]) ||
        g_now_ms < arrival_ms[next]) {
        return false;
    }
    *out_byte = (uint8_t)('A' + next);
    next++;
    return true;
}

static void cmd_feed_byte(uint8_t byte) {
    printf("t=%4u ms  收到命令字节 '%c'\n", (unsigned)g_now_ms, (char)byte);
}

/* --- 循环本身 --- */

typedef enum {
    SENSOR_IDLE = 0,
    SENSOR_CONVERTING
} sensor_state_t;

int main(void) {
    uint32_t last_led_ms = 0u;
    uint32_t last_sample_ms = 0u;
    sensor_state_t state = SENSOR_IDLE;

    while (g_now_ms < RUN_MS) {
        uint32_t now = tick_now();

        if ((uint32_t)(now - last_led_ms) >= 500u) {
            last_led_ms = now;
            led_toggle();
        }

        if (state == SENSOR_IDLE) {
            if ((uint32_t)(now - last_sample_ms) >= 10u) {
                last_sample_ms += 10u;
                sensor_start_conv();
                state = SENSOR_CONVERTING;
            }
        } else if (sensor_conv_ready()) {
            filter_push(sensor_read_raw());
            state = SENSOR_IDLE;
        }

        uint8_t ch = 0u;
        if (uart_try_getc(&ch)) {
            cmd_feed_byte(ch);
        }

        g_now_ms++;
    }

    printf("虚拟时间 %u ms 结束\n", (unsigned)RUN_MS);
    return 0;
}
