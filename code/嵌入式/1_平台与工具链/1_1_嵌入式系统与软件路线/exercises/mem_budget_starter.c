#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#define FLASH_LIMIT_BYTES (64u * 1024u)
#define SRAM_LIMIT_BYTES  (20u * 1024u)

struct module_size {
    const char *name;
    uint32_t text_bytes;
    uint32_t data_bytes;
    uint32_t bss_bytes;
};

int main(void) {
    const struct module_size modules[] = {
        {"startup", 512, 0, 64},
        {"bsp_drv", 6400, 128, 512},
        {"middleware", 24576, 256, 8192},
        {"app", 12288, 64, 4096},
    };
    const uint32_t reserved_stack = 2048;
    const size_t count = sizeof(modules) / sizeof(modules[0]);

    // TODO: 汇总所有模块的 Flash 占用（text + data）与 SRAM 占用（data + bss + reserved_stack），
    // 打印总量、占 STM32F103C8T6 容量的百分比，并判断是否超出上限。
    (void)reserved_stack;
    (void)count;

    return 0;
}
