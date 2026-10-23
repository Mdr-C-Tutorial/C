/*
 * demo.c：四种典型的函数栈帧，用于演示 -fstack-usage 的输出。
 *
 * 编译：arm-none-eabi-gcc -std=c11 -mcpu=cortex-m3 -mthumb -O2 -fstack-usage -c demo.c
 * 讲解见教程 6.1 节「固件架构、配置与资源预算」。
 */
#include <stdint.h>
#include <stdio.h>

extern void bus_write(uint8_t addr, const uint8_t *buf, uint32_t len);
extern int32_t sensor_read_raw(uint8_t reg);

static void frame_build(uint8_t *out, int32_t value) {
    out[0] = 0xA5u;
    out[1] = (uint8_t)(value & 0xFF);
    out[2] = (uint8_t)((value >> 8) & 0xFF);
}

static void sensor_publish(int32_t value) {
    uint8_t frame[32];
    frame_build(frame, value);
    bus_write(0x68u, frame, sizeof frame);
}

void sensor_task(void) {
    int32_t v = sensor_read_raw(0x3Bu);
    sensor_publish(v);
}

void report_task(void) {
    char line[48];
    int n = snprintf(line, sizeof line, "v=%ld", (long)sensor_read_raw(0x3Bu));
    bus_write(0x10u, (const uint8_t *)line, (uint32_t)n);
}

void log_task(unsigned len) {
    char buf[len];
    buf[0] = '\0';
    bus_write(0x10u, (const uint8_t *)buf, 1u);
}

void big_buffer_task(void) {
    uint8_t scratch[512];
    for (uint32_t i = 0; i < sizeof scratch; ++i) {
        scratch[i] = (uint8_t)i;
    }
    bus_write(0x20u, scratch, 4u);
}
