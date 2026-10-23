/*
 * log_net.c：第二个模块，用 -DLOG_MODULE_TAG 之外的另一种方式指定标签。
 *
 * 同一份 log.h 在多个模块里使用，每个模块在自己的源文件开头定义
 * LOG_MODULE_TAG，日志行里就能看出是谁写的。
 *
 * 讲解见教程 6.2 节「日志、断言与故障诊断」。
 */
#include <stdint.h>

#define LOG_MODULE_TAG "net"
#include "log.h"

void net_poll(void) {
    LOG_DBG("rx ring level %u", 3u);
    LOG_INF("link up, dhcp lease 192.168.1.42");
}