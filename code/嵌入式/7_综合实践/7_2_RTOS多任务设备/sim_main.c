/*
 * sim_main.c：模拟器上的板级实现。
 *
 * 传感器用查表：一张正弦表循环，三个轴带固定相位差，读数与真实 MPU6050
 * 同一量纲（mg）。块设备用 littlefs 自带的 lfs_rambd，64 个 4 KB 块，
 * 缓冲区静态分配。
 *
 * 命令行输入有两种来源：给了脚本文件时（命令行参数或环境变量 MDR_SCRIPT），
 * 每行是「等待毫秒 命令」，调度器启动前一次读进内存；否则读标准输入。
 * 脚本在任务里不再碰 stdio —— Windows 移植层下多个任务争用 C 运行库的锁
 * 会偶发卡死，预读进内存就避开了这件事。
 */
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <windows.h>

#include "FreeRTOS.h"
#include "queue.h"
#include "task.h"

#include "board.h"
#include "lfs.h"
#include "lfs_rambd.h"
#include "log.h"
#include "recorder.h"

#define SIM_BLOCK_COUNT 64
#define SIM_BLOCK_SIZE 4096

#define SCRIPT_MAX_LINES 16
#define SCRIPT_LINE_MAX 64

static uint8_t g_rambd_buffer[SIM_BLOCK_SIZE * SIM_BLOCK_COUNT];
static struct lfs_rambd_config g_rambd_cfg;
static lfs_rambd_t g_rambd;

static char g_script_lines[SCRIPT_MAX_LINES][SCRIPT_LINE_MAX];
static int g_script_count;
static int g_script_pos;

bool sensor_read_raw(int16_t *ax, int16_t *ay, int16_t *az) {
    static const int16_t table[8] = {0, 383, 707, 924, 1000, 924, 707, 383};
    static uint32_t i;

    /* ±1 g 的正弦表，单位 mg；z 轴叠加 1 g 重力 */
    *ax = (int16_t)(table[i % 8] / 2);
    *ay = (int16_t)(table[(i + 2) % 8] / 3);
    *az = (int16_t)(1000 - table[(i + 4) % 8] / 4);
    ++i;
    return true;
}

void board_watchdog_feed(void) {
    /* 模拟器没有看门狗 */
}

void board_lfs_attach(struct lfs_config *cfg) {
    cfg->context = &g_rambd;
}

int board_lfs_read(const struct lfs_config *cfg, lfs_block_t block, lfs_off_t off,
                   void *buffer, lfs_size_t size) {
    return lfs_rambd_read(cfg, block, off, buffer, size);
}

int board_lfs_prog(const struct lfs_config *cfg, lfs_block_t block, lfs_off_t off,
                   const void *buffer, lfs_size_t size) {
    return lfs_rambd_prog(cfg, block, off, buffer, size);
}

int board_lfs_erase(const struct lfs_config *cfg, lfs_block_t block) {
    return lfs_rambd_erase(cfg, block);
}

int board_lfs_sync(const struct lfs_config *cfg) {
    return lfs_rambd_sync(cfg);
}

void board_exit(void) {
    /* 从任务里结束进程。exit() 会跑清理函数，别的任务可能正持有 C 运行库
     * 的锁，Windows 上会卡住；ExitProcess 直接终止所有线程。 */
    fflush(stdout);
    ExitProcess(0);
}

bool board_console_read_line(char *buf, size_t max, TickType_t wait) {
    if (g_script_count == 0) {
        (void)wait;
        if (fgets(buf, (int)max, stdin) == NULL) {
            /* 没有输入就睡一小会儿，别把 CPU 占满 */
            vTaskDelay(pdMS_TO_TICKS(10));
            return false;
        }
        buf[strcspn(buf, "\r\n")] = '\0';
        return true;
    }

    /* 脚本模式：每行 "等待毫秒 命令"，先等这一行要求的毫秒数再返回命令 */
    if (g_script_pos >= g_script_count) {
        board_exit(); /* 脚本跑完，退出 */
    }

    char line[SCRIPT_LINE_MAX];
    memcpy(line, g_script_lines[g_script_pos++], sizeof line);

    char *end = NULL;
    long ms = strtol(line, &end, 10);
    if (end == line) {
        ms = 0;
        end = line;
    }
    while (*end == ' ') {
        ++end;
    }
    snprintf(buf, max, "%s", end);

    vTaskDelay(pdMS_TO_TICKS(ms));
    return true;
}

/* 调度器启动前调用：把脚本读进内存 */
static void script_load(const char *path) {
    FILE *fp = fopen(path, "r");
    char line[SCRIPT_LINE_MAX];

    if (fp == NULL) {
        printf("打不开脚本 %s\n", path);
        return;
    }
    while (g_script_count < SCRIPT_MAX_LINES && fgets(line, sizeof line, fp) != NULL) {
        line[strcspn(line, "\r\n")] = '\0';
        if (line[0] == '\0') {
            continue;
        }
        snprintf(g_script_lines[g_script_count], SCRIPT_LINE_MAX, "%s", line);
        ++g_script_count;
    }
    fclose(fp);
}

/* 模拟器上的控制台：整行输出放在临界区里，避免多个任务各打印半行 */
void recorder_console_printf(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    taskENTER_CRITICAL();
    vprintf(fmt, args);
    fflush(stdout);
    taskEXIT_CRITICAL();
    va_end(args);
}

/* 栈溢出与堆分配失败：模拟器上直接报错退出，方便测试发现问题 */
void vApplicationStackOverflowHook(TaskHandle_t task, char *name) {
    (void)task;
    printf("\n[致命] 任务 %s 栈溢出\n", name);
    fflush(stdout);
    ExitProcess(1);
}

void vApplicationMallocFailedHook(void) {
    printf("\n[致命] 堆分配失败\n");
    fflush(stdout);
    ExitProcess(1);
}

int main(int argc, char **argv) {
    const char *script = (argc > 1) ? argv[1] : getenv("MDR_SCRIPT");
    if (script != NULL) {
        script_load(script);
    }

    g_rambd_cfg = (struct lfs_rambd_config){
        .read_size = 16,
        .prog_size = 16,
        .erase_size = SIM_BLOCK_SIZE,
        .erase_count = SIM_BLOCK_COUNT,
        .buffer = g_rambd_buffer,
    };
    g_rambd.buffer = g_rambd_buffer;
    g_rambd.cfg = &g_rambd_cfg;
    /* 擦除后的 Flash 读出来是 0xFF，lfs_rambd 不会自动初始化缓冲区 */
    memset(g_rambd_buffer, 0xFF, sizeof g_rambd_buffer);

    /* 模拟器每次启动都是空白 RAM：先格式化，省掉首次挂载失败的提示 */
    (void)log_format();

    recorder_console_printf("姿态记录仪：模拟器构建\n");
    recorder_start();
    vTaskStartScheduler();
    return 0;
}