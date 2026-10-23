#ifndef RECORDER_H
#define RECORDER_H

#include <stdbool.h>
#include <stdint.h>

#include "FreeRTOS.h"
#include "queue.h"
#include "task.h"

/* 一个采样：时刻、三轴加速度（mg）与由处理任务填上的倾角（0.01°） */
typedef struct {
    uint32_t tick;
    int16_t ax;
    int16_t ay;
    int16_t az;
    int16_t pitch;
    int16_t roll;
} sample_t;

/* 任务优先级：数值越大越高（FreeRTOS） */
#define PRIO_SAMPLE 4
#define PRIO_RECORD 3
#define PRIO_CONSOLE 2
#define PRIO_MONITOR 1

#define SAMPLE_QUEUE_LEN 8
#define RECORD_QUEUE_LEN 4

/* 记录任务每收到一条样本追加一行，10 Hz 由采样任务分频得到 */
#define RECORD_EVERY_N 10

/* 记录仪的运行计数，status 命令读取（单写多读，32 位在 M3 上单条指令读） */
extern volatile uint32_t g_sample_count;
extern volatile uint32_t g_sample_dropped;
extern volatile uint32_t g_record_count;
extern volatile bool g_record_hung;

/* 健康监视：每个任务在自己的循环里打卡，监视任务检查计数是否在动 */
void monitor_check_in(int task_index);
int monitor_task_count(void);

/* 控制台输出（模拟器上是临界区里的 vprintf，目标板上是 USART1） */
void recorder_console_printf(const char *fmt, ...);

/* 看门狗：模拟器上什么都不做，目标板上喂 IWDG */
void board_watchdog_feed(void);

/* 读一个样本：模拟器上查表，目标板上走 MPU6050 */
bool sensor_read_raw(int16_t *ax, int16_t *ay, int16_t *az);

void recorder_start(void);

/* console_task.c 提供 */
void console_task(void *arg);

#endif /* RECORDER_H */