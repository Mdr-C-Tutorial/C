/*
 * main.c：7.1 的应用层。
 *
 * 超级循环里三件事按各自的周期进行：每 10 ms 采一次 MPU6050，每 500 ms 翻转
 * 心跳 LED，随时处理命令行。stream 打开时，按 rate 指定的频率输出采样。
 * 主循环所有工作做完之后喂一次独立看门狗。
 */
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "board.h"
#include "cli.h"
#include "mpu6050.h"
#include "stm32f1xx.h"

#define SAMPLE_PERIOD_MS 10u
#define LED_PERIOD_MS 500u
#define IWDG_RELOAD 1249u /* 64 × 1250 / 40 kHz = 2.0 s */

static mpu6050_t g_dev;
static uint8_t g_who;
static bool g_stream;
static uint32_t g_rate_hz = 10u;
static uint32_t g_last_stream_ms;

static void iwdg_init(void) {
    /* LSI ≈ 40 kHz，预分频 64，重装值 1249：4 × 2^4 × 1250 / 40 kHz = 2.0 s */
    IWDG->KR = 0x5555u; /* 允许写 PR/RLR */
    IWDG->PR = 0x04u;   /* 64 分频 */
    IWDG->RLR = IWDG_RELOAD;
    IWDG->KR = 0xAAAAu; /* 首次喂狗 */
    IWDG->KR = 0xCCCCu; /* 启动 */
}

static void iwdg_feed(void) {
    IWDG->KR = 0xAAAAu;
}

/* 倾角：atan2f 只用在对数输出上，不参与采样与控制 */
static void print_attitude(const mpu6050_sample_t *s) {
    float ax = (float)s->ax / 1000.0f;
    float ay = (float)s->ay / 1000.0f;
    float az = (float)s->az / 1000.0f;
    float pitch = atan2f(ax, sqrtf(ay * ay + az * az)) * 57.29578f;
    float roll = atan2f(ay, sqrtf(ax * ax + az * az)) * 57.29578f;

    printf("ax=%5d ay=%5d az=%5d mg  pitch=%6.1f roll=%6.1f deg\n", s->ax, s->ay,
           s->az, (double)pitch, (double)roll);
}

static int cmd_help(const char *args) {
    (void)args;
    printf("命令：\n");
    printf("  id            打印 WHO_AM_I（应为 0x68）\n");
    printf("  read          读一次加速度与倾角\n");
    printf("  stream on|off 连续输出\n");
    printf("  rate <Hz>     设置 stream 输出频率（1–100）\n");
    printf("  help          本页\n");
    return 0;
}

static int cmd_id(const char *args) {
    (void)args;
    printf("WHO_AM_I = 0x%02X（期望 0x68）\n", (unsigned)g_who);
    return 0;
}

static int cmd_read(const char *args) {
    (void)args;
    mpu6050_sample_t s;
    if (!mpu6050_read_all(&g_dev, &s)) {
        printf("读取失败：I2C 无应答或总线恢复失败\n");
        return -1;
    }
    print_attitude(&s);
    return 0;
}

static int cmd_stream(const char *args) {
    if (strcmp(args, "on") == 0) {
        g_stream = true;
        g_last_stream_ms = board_tick_now();
    } else if (strcmp(args, "off") == 0) {
        g_stream = false;
    } else {
        printf("用法：stream on|off\n");
        return -1;
    }
    return 0;
}

static int cmd_rate(const char *args) {
    char *end = NULL;
    long hz = strtol(args, &end, 10);
    if (end == args || hz < 1 || hz > 100) {
        printf("用法：rate <1..100>\n");
        return -1;
    }
    g_rate_hz = (uint32_t)hz;
    return 0;
}

static const struct cli_command k_commands[] = {
    {"help", "帮助", cmd_help},
    {"id", "读 WHO_AM_I", cmd_id},
    {"read", "读一次数据", cmd_read},
    {"stream", "stream on|off", cmd_stream},
    {"rate", "rate <Hz>", cmd_rate},
};

int main(void) {
    board_init();
    iwdg_init();

    printf("\n7.1 裸机外设驱动：复位原因 %s\n", board_reset_cause_name());

    if (!mpu6050_init(&g_dev, board_i2c_bus(), MPU6050_ADDR7, &g_who)) {
        printf("MPU6050 初始化失败：WHO_AM_I = 0x%02X。检查接线与地址（AD0）。\n",
               (unsigned)g_who);
    } else {
        printf("MPU6050 就绪，WHO_AM_I = 0x%02X\n", (unsigned)g_who);
    }

    uint32_t next_sample = board_tick_now();
    uint32_t next_led = board_tick_now();
    char line[64];
    mpu6050_sample_t s = {0};

    for (;;) {
        uint32_t now = board_tick_now();

        /* 1. 周期采样，100 Hz */
        if ((int32_t)(now - next_sample) >= 0) {
            next_sample += SAMPLE_PERIOD_MS;
            if (!mpu6050_read_all(&g_dev, &s)) {
                if (g_stream) {
                    printf("采样失败\n");
                }
            }
        }

        /* 2. stream 输出按 rate 分频 */
        if (g_stream) {
            uint32_t period = 1000u / g_rate_hz;
            if ((uint32_t)(now - g_last_stream_ms) >= period) {
                g_last_stream_ms = now;
                print_attitude(&s);
            }
        }

        /* 3. 心跳 */
        if ((int32_t)(now - next_led) >= 0) {
            next_led += LED_PERIOD_MS;
            board_led_toggle();
        }

        /* 4. 命令行，随时响应 */
        if (board_console_try_read_line(line, (int)sizeof line)) {
            (void)cli_dispatch(k_commands, sizeof k_commands / sizeof k_commands[0],
                               line);
            printf("> ");
        }

        /* 5. 本轮所有工作都做完，喂狗 */
        iwdg_feed();
    }
}