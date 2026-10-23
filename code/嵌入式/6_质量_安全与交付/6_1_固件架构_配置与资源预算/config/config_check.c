/*
 * config_check.c：只检查配置头文件的组合，不产生有意义的固件。
 *
 * 构建系统按板子定义 BOARD_HEADER，编译这个文件就等于把配置检查跑一遍。
 * 讲解见教程 6.1 节「固件架构、配置与资源预算」。
 */
#include <stdint.h>

#include "app_config.h"

/* 让配置值真正进入代码，否则宏定义错了也没人发现 */
const uint32_t g_console_buf_size = APP_LOG_BUF_SIZE;
const uint32_t g_cmd_line_max = APP_LINE_MAX;
const char g_board_name[] = BOARD_NAME;

int main(void) {
    return (int)(g_console_buf_size + g_cmd_line_max);
}