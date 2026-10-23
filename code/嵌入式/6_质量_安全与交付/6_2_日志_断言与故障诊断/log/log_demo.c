/*
 * log_demo.c：主机上演示日志的分级过滤与模块标签。
 *
 * 编译：gcc -std=c17 -Wall -Wextra -I. log.c log_net.c log_demo.c -o log_demo
 * 用 -DLOG_LEVEL=... 重新编译，观察哪些级别的语句彻底消失。
 *
 * 讲解见教程 6.2 节「日志、断言与故障诊断」。
 */
#include <stdint.h>
#include <stdio.h>

#include "log.h"

static uint32_t g_ms;

uint32_t log_tick_ms(void) {
    return g_ms;
}

void log_write(const char *s, size_t len) {
    fwrite(s, 1, len, stdout);
}

/* 另一个模块，自己的标签在 log_net.c 里定义 */
void net_poll(void);

int main(void) {
    g_ms = 12;
    LOG_INF("boot ok, version %d.%d", 1, 4);
    g_ms = 130;
    LOG_DBG("adc raw = %d", 2048);
    g_ms = 251;
    LOG_WRN("sensor not responding, retry %d", 1);
    g_ms = 400;
    LOG_ERR("i2c bus error 0x%02X", 0x18u);
    g_ms = 512;
    net_poll();
    return 0;
}