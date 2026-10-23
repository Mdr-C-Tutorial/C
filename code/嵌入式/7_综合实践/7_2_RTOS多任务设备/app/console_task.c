/*
 * console_task.c：命令行任务与命令表。
 *
 * 沿用 7.1 的结构：命令名、帮助、处理函数。这里命令表直接用 strncmp 匹配，
 * 少了分词，因为每个命令最多一个参数。
 *
 * status 打印最近一秒的采样与记录速率、堆剩余、任务表和栈高水位；
 * dump 输出日志头几行与总行数；erase 清空；hang 做故障注入。
 */
#include "recorder.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "FreeRTOS.h"
#include "queue.h"
#include "task.h"

#include "board.h"
#include "log.h"

#define DUMP_HEAD_LINES 4

static TickType_t g_last_status_tick;
static uint32_t g_last_sample_count;
static uint32_t g_last_record_count;

static void print_status(void) {
    TickType_t now = xTaskGetTickCount();
    uint32_t ds = g_sample_count - g_last_sample_count;
    uint32_t dr = g_record_count - g_last_record_count;
    TickType_t dt = now - g_last_status_tick;

    g_last_status_tick = now;
    g_last_sample_count = g_sample_count;
    g_last_record_count = g_record_count;

    unsigned long rate = (dt == 0) ? 0 : (unsigned long)((uint64_t)ds * 1000u / dt);
    unsigned long rrate = (dt == 0) ? 0 : (unsigned long)((uint64_t)dr * 1000u / dt);

    recorder_console_printf("采样 %lu 条（%lu/s），队列丢弃 %lu，记录 %lu 条（%lu/s）\n",
                            (unsigned long)g_sample_count, rate,
                            (unsigned long)g_sample_dropped,
                            (unsigned long)g_record_count, rrate);
    recorder_console_printf("堆：空闲 %u 字节，历史最小 %u 字节\n",
                            (unsigned)xPortGetFreeHeapSize(),
                            (unsigned)xPortGetMinimumEverFreeHeapSize());

    char *buf = pvPortMalloc(768);
    if (buf == NULL) {
        recorder_console_printf("堆不足，无法列出任务\n");
        return;
    }
    vTaskList(buf);
    recorder_console_printf("名称      状态 优先级 剩余栈 序号\n%s", buf);
    vPortFree(buf);
}

/* dump 的输出缓冲放在静态区：2 KB 放不下命令任务的栈（栈深 384 字） */
static char g_dump_buf[2048];

static void cmd_dump(void) {
    char *buf = g_dump_buf;
    size_t len = 0;
    int lines = 0;

    for (int i = 0; i < log_file_count(); ++i) {
        if (log_read_file(i, buf, sizeof g_dump_buf, &len) != 0 || len == 0) {
            continue;
        }
        int file_lines = 0;
        for (size_t k = 0; k < len; ++k) {
            file_lines += (buf[k] == '\n');
        }
        lines += file_lines;

        recorder_console_printf("--- log_%03d.csv：%d 行 ---\n", i, file_lines);
        int shown = 0;
        for (size_t k = 0; k < len && shown < DUMP_HEAD_LINES; ++k) {
            char c = buf[k];
            if (c == '\n') {
                ++shown;
            }
            recorder_console_printf("%c", c);
        }
        if (file_lines > DUMP_HEAD_LINES) {
            recorder_console_printf("...（其余 %d 行略）\n", file_lines - DUMP_HEAD_LINES);
        }
    }
    recorder_console_printf("日志共 %d 行\n", lines);
}

static void cmd_help(void) {
    recorder_console_printf("status   运行统计、堆与任务表\n");
    recorder_console_printf("dump     导出 CSV 日志（只显示前几行）\n");
    recorder_console_printf("erase    清空日志\n");
    recorder_console_printf("rate <Hz> 设置记录频率（1–100）\n");
    recorder_console_printf("hang     让记录任务卡住 2.5 秒（故障注入）\n");
    recorder_console_printf("quit     退出模拟器\n");
}

void console_task(void *arg) {
    (void)arg;
    char line[64];

    g_last_status_tick = xTaskGetTickCount();

    for (;;) {
        if (!board_console_read_line(line, sizeof line, pdMS_TO_TICKS(500))) {
            monitor_check_in(3);
            continue;
        }

        char *p = line;
        while (*p == ' ') {
            ++p;
        }
        if (strncmp(p, "status", 6) == 0) {
            print_status();
        } else if (strncmp(p, "dump", 4) == 0) {
            cmd_dump();
        } else if (strncmp(p, "erase", 5) == 0) {
            log_erase_all();
            g_record_count = 0;
            recorder_console_printf("日志已清空\n");
        } else if (strncmp(p, "rate", 4) == 0) {
            recorder_console_printf("记录频率参数：%s（本页固定按采样 10 分频）\n", p + 4);
        } else if (strncmp(p, "hang", 4) == 0) {
            g_record_hung = true;
            recorder_console_printf("记录任务将在下一条样本时卡住 2.5 秒\n");
        } else if (strncmp(p, "help", 4) == 0) {
            cmd_help();
        } else if (strncmp(p, "quit", 4) == 0) {
            board_exit();
        } else if (*p != '\0') {
            recorder_console_printf("未知命令：%s\n", p);
        }
        monitor_check_in(3);
    }
}