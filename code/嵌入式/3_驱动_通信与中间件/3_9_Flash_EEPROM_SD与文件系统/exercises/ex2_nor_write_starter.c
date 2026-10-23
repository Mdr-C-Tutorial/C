/*
 * 习题 60392 初始代码：W25Q64 跨页拆分写入函数 nor_write
 * 编译命令：gcc -std=c17 -Wall -Wextra ex2_nor_write_starter.c -o ex2_nor_write
 */
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define NOR_PAGE_SIZE   256u
#define NOR_SECTOR_SIZE 4096u

static uint8_t g_nor[NOR_SECTOR_SIZE];
static bool    g_wel = false;
static size_t  g_page_prog_calls = 0u;

static void mock_write_enable(void) {
    g_wel = true;
}

/* 模拟 W25Q64 的 0x02 页编程：若跨越 256 字节页边界，高地址保持不变而低 8 位回绕到页首 */
static void mock_page_program(uint32_t addr, const uint8_t *buf, size_t len) {
    if (!g_wel || len == 0u || len > NOR_PAGE_SIZE) {
        return;
    }
    uint32_t page_base = addr & ~(NOR_PAGE_SIZE - 1u);
    uint32_t offset = addr & (NOR_PAGE_SIZE - 1u);
    for (size_t i = 0; i < len; ++i) {
        g_nor[page_base + ((offset + i) & (NOR_PAGE_SIZE - 1u))] &= buf[i];
    }
    g_wel = false; /* 编程完成后 WEL 自动清零 */
    g_page_prog_calls++;
}

static void mock_wait_busy(void) {
}

/* TODO：实现 nor_write，将任意起始地址 addr 与长度 len 按 256 字节页边界拆分写入 */
void nor_write(uint32_t addr, const void *buf, size_t len) {
    (void)mock_write_enable;
    (void)mock_page_program;
    (void)mock_wait_busy;
    (void)addr;
    (void)buf;
    (void)len;
}

int main(void) {
    memset(g_nor, 0xFF, sizeof(g_nor));
    uint8_t pattern[600];
    for (size_t i = 0; i < sizeof(pattern); ++i) {
        pattern[i] = (uint8_t)((i * 37u + 13u) & 0xFFu);
    }
    nor_write(250u, pattern, sizeof(pattern));
    printf("page_prog_calls = %u\n", (unsigned)g_page_prog_calls);
    return 0;
}
