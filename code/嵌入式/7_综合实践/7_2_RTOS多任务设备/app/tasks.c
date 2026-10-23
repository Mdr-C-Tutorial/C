/*
 * tasks.c：姿态记录仪的任务集。
 *
 * 数据流：采样任务（100 Hz）→ q_sample → 处理任务（滑动平均、算倾角）
 *         → q_record（每 10 个样本一条）→ 记录任务（CSV 写入 littlefs）
 *         → q_latest（长度 1，xQueueOverwrite）→ 命令行查询
 *
 * 每个任务在循环里调用 monitor_check_in()，监视任务每秒检查一次计数有没有增长。
 * 看门狗在监视任务里喂：只要有一个任务卡住，喂狗就停下。
 */
#include "recorder.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "FreeRTOS.h"
#include "queue.h"
#include "semphr.h"
#include "task.h"

#include "log.h"

/* hang 命令让记录任务卡住的时长；监视任务每秒检查一次，2.5 秒能报一到两次 */
#define HANG_MS 2500u

static QueueHandle_t g_q_sample;
static QueueHandle_t g_q_record;
static QueueHandle_t g_q_latest;

static volatile uint32_t g_check_in[5];

volatile uint32_t g_sample_count;
volatile uint32_t g_sample_dropped;
volatile uint32_t g_record_count;
volatile bool g_record_hung;

/* 每个任务在循环里打卡；监视任务读它 */
void monitor_check_in(int task_index) {
    if (task_index >= 0 && task_index < 5) {
        ++g_check_in[task_index];
    }
}

int monitor_task_count(void) {
    return 5;
}

static void sample_task(void *arg) {
    (void)arg;
    TickType_t last = xTaskGetTickCount();

    for (;;) {
        int16_t ax = 0;
        int16_t ay = 0;
        int16_t az = 0;

        if (sensor_read_raw(&ax, &ay, &az)) {
            sample_t s = {
                .tick = (uint32_t)xTaskGetTickCount(),
                .ax = ax,
                .ay = ay,
                .az = az,
                .pitch = 0,
                .roll = 0,
            };
            ++g_sample_count;
            if (xQueueSend(g_q_sample, &s, 0) != pdTRUE) {
                ++g_sample_dropped; /* 处理任务来不及，丢最旧的采样不阻塞 */
            }
        }
        monitor_check_in(0);
        (void)xTaskDelayUntil(&last, pdMS_TO_TICKS(10));
    }
}

/* 4 点滑动平均，去掉单次采样的尖峰 */
static void smooth(int16_t *hist, int16_t v) {
    hist[3] = hist[2];
    hist[2] = hist[1];
    hist[1] = hist[0];
    hist[0] = v;
}

static int16_t mean4(const int16_t *hist) {
    return (int16_t)(((int32_t)hist[0] + hist[1] + hist[2] + hist[3]) / 4);
}

static void process_task(void *arg) {
    (void)arg;
    int16_t hx[4] = {0};
    int16_t hy[4] = {0};
    int16_t hz[4] = {0};
    uint32_t n = 0;
    sample_t latest = {0};

    for (;;) {
        sample_t s;
        if (xQueueReceive(g_q_sample, &s, portMAX_DELAY) != pdTRUE) {
            continue;
        }

        smooth(hx, s.ax);
        smooth(hy, s.ay);
        smooth(hz, s.az);
        latest = s;
        latest.ax = mean4(hx);
        latest.ay = mean4(hy);
        latest.az = mean4(hz);

        /* 倾角只按记录频率算：F103 没有 FPU，atan2f 是软件浮点 */
        if (++n % RECORD_EVERY_N == 0) {
            float ax = (float)latest.ax / 1000.0f;
            float ay = (float)latest.ay / 1000.0f;
            float az = (float)latest.az / 1000.0f;
            latest.pitch =
                (int16_t)(atan2f(ax, sqrtf(ay * ay + az * az)) * 5729.578f);
            latest.roll = (int16_t)(atan2f(ay, sqrtf(ax * ax + az * az)) * 5729.578f);
            (void)xQueueSend(g_q_record, &latest, 0);
        }

        /* 长度 1 的队列：覆盖写，命令行随时取到最新值 */
        (void)xQueueOverwrite(g_q_latest, &latest);
        monitor_check_in(1);
    }
}

static void record_task(void *arg) {
    (void)arg;
    char line[80];

    for (;;) {
        sample_t s;
        if (xQueueReceive(g_q_record, &s, portMAX_DELAY) != pdTRUE) {
            continue;
        }

        if (g_record_hung) {
            /* 故障注入用：卡在这里，监视任务应当发现 */
            vTaskDelay(pdMS_TO_TICKS(HANG_MS));
            g_record_hung = false;
        }

        snprintf(line, sizeof line, "%lu,%d,%d,%d,%d,%d\n", (unsigned long)s.tick, s.ax,
                 s.ay, s.az, s.pitch, s.roll);
        if (log_append(line) == 0) {
            ++g_record_count;
        }
        monitor_check_in(2);
    }
}

/*
 * 监视任务每秒检查一次周期任务的打卡计数。命令任务与监视任务自己不在
 * 检查范围内：前者可能长时间等输入，后者就是检查者。
 */
static const char *const k_watched[] = {"sample", "process", "record"};

static void monitor_task(void *arg) {
    (void)arg;
    uint32_t last[3];

    for (int i = 0; i < 3; ++i) {
        last[i] = g_check_in[i];
    }

    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(1000));

        bool all_alive = true;
        for (int i = 0; i < 3; ++i) {
            if (g_check_in[i] == last[i]) {
                all_alive = false;
                recorder_console_printf("[监视] 任务 %s 超过 1 秒没有打卡\n",
                                        k_watched[i]);
            }
            last[i] = g_check_in[i];
        }
        if (all_alive) {
            board_watchdog_feed();
        }
        monitor_check_in(4);
    }
}

void recorder_start(void) {
    if (log_init() != 0) {
        recorder_console_printf("日志文件系统挂载失败，记录功能不可用\n");
    }

    g_q_sample = xQueueCreate(SAMPLE_QUEUE_LEN, sizeof(sample_t));
    g_q_record = xQueueCreate(RECORD_QUEUE_LEN, sizeof(sample_t));
    g_q_latest = xQueueCreate(1, sizeof(sample_t));

    xTaskCreate(sample_task, "sample", 256, NULL, PRIO_SAMPLE, NULL);
    xTaskCreate(process_task, "process", 320, NULL, PRIO_SAMPLE - 1, NULL);
    xTaskCreate(record_task, "record", 384, NULL, PRIO_RECORD, NULL);
    xTaskCreate(console_task, "console", 384, NULL, PRIO_CONSOLE, NULL);
    xTaskCreate(monitor_task, "monitor", 256, NULL, PRIO_MONITOR, NULL);
}