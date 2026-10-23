/*
 * supervisor.c：各任务打卡 + 监视任务 + 用打印代替的看门狗。
 *
 * 每个被监视的任务持有自己的“最后打卡时刻”。监视任务每 100 ms 检查一轮，
 * 全部按时打卡才“喂狗”（打印一行），有一个超时就停止喂狗并记录任务名。
 */
#include <stdint.h>
#include <string.h>

#include "FreeRTOS.h"
#include "task.h"
#include "rtos_console.h"

#define CHECK_PERIOD_MS 100
#define TIMEOUT_MS      300
#define WATCHED_COUNT   3

struct checkpoint {
    const char *name;
    volatile TickType_t last;
};

static struct checkpoint g_watched[WATCHED_COUNT] = {
    { .name = "sampler" },
    { .name = "logger" },
    { .name = "comm" },
};

static volatile int g_feed_count;
static volatile int g_starved;
static char g_stuck_name[16];

static void punch(struct checkpoint *cp) {
    cp->last = xTaskGetTickCount();
}

static void sampler_task(void *param) {
    (void)param;
    for (;;) {
        punch(&g_watched[0]);
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

static void logger_task(void *param) {
    (void)param;
    for (;;) {
        punch(&g_watched[1]);
        vTaskDelay(pdMS_TO_TICKS(80));
    }
}

/* comm 任务在第 2 次运行后“卡住”：不再打卡，也不再让出 CPU。 */
static void comm_task(void *param) {
    (void)param;
    for (int i = 0; i < 2; ++i) {
        punch(&g_watched[2]);
        vTaskDelay(pdMS_TO_TICKS(60));
    }
    console_printf("[comm] stuck: no more punches (tick %lu)\n",
                   (unsigned long)xTaskGetTickCount());
    for (;;) {
        /* 模拟死循环：真正的固件里可能是等一个永远不会来的标志 */
        vTaskSuspend(NULL);
    }
}

static void supervisor_task(void *param) {
    (void)param;
    TickType_t last_wake = xTaskGetTickCount();

    for (;;) {
        TickType_t now = xTaskGetTickCount();
        const char *stuck = NULL;

        for (int i = 0; i < WATCHED_COUNT; ++i) {
            TickType_t elapsed = now - g_watched[i].last;
            if (elapsed > pdMS_TO_TICKS(TIMEOUT_MS)) {
                stuck = g_watched[i].name;
                break;
            }
        }

        if (stuck == NULL) {
            g_feed_count++;
            /* 喂狗：真实固件里是写 IWDG->KR = 0xAAAA */
        } else if (!g_starved) {
            g_starved = 1;
            strncpy(g_stuck_name, stuck, sizeof(g_stuck_name) - 1);
            console_printf("[watchdog] stop feeding: '%s' missed deadline at tick %lu\n",
                           g_stuck_name, (unsigned long)now);
        }

        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(CHECK_PERIOD_MS));
    }
}

static void report_task(void *param) {
    (void)param;

    vTaskDelay(pdMS_TO_TICKS(1200));
    console_printf("[main] fed %d times, starved=%d, stuck=%s\n",
                   g_feed_count, g_starved,
                   g_starved ? g_stuck_name : "none");
    sim_finish();
}

int main(void) {
    console_init();
    TickType_t now = xTaskGetTickCount();
    for (int i = 0; i < WATCHED_COUNT; ++i) {
        g_watched[i].last = now;
    }

    xTaskCreate(sampler_task, "sampler", 256, NULL, 2, NULL);
    xTaskCreate(logger_task, "logger", 256, NULL, 2, NULL);
    xTaskCreate(comm_task, "comm", 256, NULL, 2, NULL);
    xTaskCreate(supervisor_task, "supervisor", 512, NULL, 4, NULL);
    xTaskCreate(report_task, "report", 512, NULL, 3, NULL);

    vTaskStartScheduler();
    return 0;
}
