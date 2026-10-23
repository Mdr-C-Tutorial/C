/*
 * log.h：分级日志的宏，级别在编译期过滤。
 *
 * LOG_LEVEL 为 0 时所有日志调用完全不产生代码。默认取 LOG_LEVEL_INF。
 * 讲解见教程 6.2 节「日志、断言与故障诊断」。
 */
#ifndef LOG_H
#define LOG_H

#include <stddef.h>
#include <stdint.h>

#define LOG_LEVEL_NONE 0
#define LOG_LEVEL_ERR 1
#define LOG_LEVEL_WRN 2
#define LOG_LEVEL_INF 3
#define LOG_LEVEL_DBG 4

#ifndef LOG_LEVEL
#define LOG_LEVEL LOG_LEVEL_INF
#endif

/* 模块标签：每个模块在自己的源文件开头定义 LOG_MODULE_TAG 再包含 log.h */
#ifndef LOG_MODULE_TAG
#define LOG_MODULE_TAG "app"
#endif

/* 平台提供：把一行日志写到目标通道（固件里是 UART，测试时是 stdout） */
void log_write(const char *s, size_t len);

/* 平台提供：毫秒计数，用于时间戳 */
uint32_t log_tick_ms(void);

void log_emit(char level_char, const char *tag, const char *fmt, ...);

#if LOG_LEVEL >= LOG_LEVEL_ERR
#define LOG_ERR(...) log_emit('E', LOG_MODULE_TAG, __VA_ARGS__)
#else
#define LOG_ERR(...) ((void)0)
#endif

#if LOG_LEVEL >= LOG_LEVEL_WRN
#define LOG_WRN(...) log_emit('W', LOG_MODULE_TAG, __VA_ARGS__)
#else
#define LOG_WRN(...) ((void)0)
#endif

#if LOG_LEVEL >= LOG_LEVEL_INF
#define LOG_INF(...) log_emit('I', LOG_MODULE_TAG, __VA_ARGS__)
#else
#define LOG_INF(...) ((void)0)
#endif

#if LOG_LEVEL >= LOG_LEVEL_DBG
#define LOG_DBG(...) log_emit('D', LOG_MODULE_TAG, __VA_ARGS__)
#else
#define LOG_DBG(...) ((void)0)
#endif

#endif /* LOG_H */