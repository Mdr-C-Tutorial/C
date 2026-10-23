/*
 * fw.c：一条四层调用链的示例固件，用来演示栈扫描还原调用链。
 *
 * 调用关系：main → app_tick → sensor_poll → protocol_write。
 * 每层都有局部对象，用 -O2 编译后仍有各自的栈帧。
 *
 * 构建（在 crashfw/ 下）：
 *   arm-none-eabi-gcc -std=c11 -mcpu=cortex-m3 -mthumb -O2 -g ^
 *       -ffunction-sections -fdata-sections -fstack-usage -nostartfiles ^
 *       --specs=nano.specs --specs=nosys.specs -Wl,--gc-sections ^
 *       -T ../../../common/stm32f103c8.ld fw.c ../../../common/startup_stm32f103.c ^
 *       -o fw.elf
 *
 * 讲解见教程 6.2 节「日志、断言与故障诊断」。
 */
#include <stdint.h>

volatile uint32_t g_sink = 0x11223344u;

static void bus_send(const uint8_t *buf, uint32_t len) {
    for (uint32_t i = 0; i < len; ++i) {
        g_sink += buf[i];
    }
}

__attribute__((noinline)) void protocol_write(uint32_t value) {
    uint8_t local[24];
    for (uint32_t i = 0; i < sizeof local; ++i) {
        local[i] = (uint8_t)(value >> ((i & 3u) * 8u));
    }
    bus_send(local, 4u);
}

__attribute__((noinline)) void sensor_poll(void) {
    uint32_t raw = g_sink;
    protocol_write(raw);
    g_sink ^= 1u;
}

__attribute__((noinline)) void app_tick(void) {
    sensor_poll();
    g_sink += 2u;
}

int main(void) {
    for (;;) {
        app_tick();
    }
}