/*
 * log_fw.c：把日志放进一个最小固件，用来量编译期级别对体积的影响。
 *
 * 构建（在 log/ 下）：
 *   arm-none-eabi-gcc -std=c11 -mcpu=cortex-m3 -mthumb -O2 -DSTM32F103xB ^
 *       -ffunction-sections -fdata-sections -DLOG_LEVEL=3 -I. ^
 *       -nostartfiles --specs=nano.specs --specs=nosys.specs -Wl,--gc-sections ^
 *       -T ../../../common/stm32f103c8.ld ^
 *       log_fw.c log.c ../../../common/startup_stm32f103.c -o log_fw.elf
 *
 * 只改 -DLOG_LEVEL，用 arm-none-eabi-size 比较 text 列。
 * 讲解见教程 6.2 节「日志、断言与故障诊断」。
 */
#include <stddef.h>
#include <stdint.h>

#define LOG_MODULE_TAG "i2c"
#include "log.h"

/* 目标上由串口驱动提供；这里只统计体积，不真的输出 */
uint32_t log_tick_ms(void) {
    return 42u;
}

void log_write(const char *s, size_t len) {
    (void)s;
    (void)len;
}

int main(void) {
    LOG_INF("init mpu6050 at addr 0x%02X", 0x68);
    LOG_DBG("who_am_i = 0x%02X", 0x68);
    LOG_WRN("retry %u", 2u);
    LOG_ERR("nack at reg 0x%02X", 0x75);
    return 0;
}