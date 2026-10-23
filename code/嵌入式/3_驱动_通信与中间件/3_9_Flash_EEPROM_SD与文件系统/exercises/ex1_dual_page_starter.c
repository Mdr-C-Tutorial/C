/*
 * 习题 60391 初始代码：片内 Flash 双页配置存储与掉电恢复验证
 * 编译命令：gcc -std=c17 -Wall -Wextra ex1_dual_page_starter.c -o ex1_dual_page
 */
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define PAGE_SIZE 1024u
#define CFG_MAGIC 0x43464731UL

typedef struct {
    uint32_t magic;
    uint32_t seq;
    uint32_t baud_rate;
    int32_t  calib_mv;
    uint32_t crc32;
} config_rec_t;

static uint8_t g_flash[PAGE_SIZE * 2u];
static size_t  g_cutoff_halfwords = 0u;
static size_t  g_written_halfwords = 0u;

static void flash_erase_page(uint32_t page_idx) {
    memset(&g_flash[page_idx * PAGE_SIZE], 0xFF, PAGE_SIZE);
}

static bool flash_prog_halfword(uint32_t byte_offset, uint16_t hw) {
    if (g_cutoff_halfwords > 0u && g_written_halfwords >= g_cutoff_halfwords) {
        return false;
    }
    g_written_halfwords++;
    uint16_t cur = (uint16_t)g_flash[byte_offset] |
                   ((uint16_t)g_flash[byte_offset + 1u] << 8);
    uint16_t next = (uint16_t)(cur & hw);
    g_flash[byte_offset] = (uint8_t)(next & 0xFFu);
    g_flash[byte_offset + 1u] = (uint8_t)(next >> 8);
    return cur == 0xFFFFu || hw == 0x0000u;
}

static uint32_t crc32_calc(const void *data, size_t len) {
    const uint8_t *p = (const uint8_t *)data;
    uint32_t crc = 0xFFFFFFFFUL;
    for (size_t i = 0; i < len; ++i) {
        crc ^= (uint32_t)p[i];
        for (int b = 0; b < 8; ++b) {
            crc = (crc >> 1) ^ ((crc & 1UL) ? 0xEDB88320UL : 0UL);
        }
    }
    return ~crc;
}

/* TODO：补全 config_save 与 config_load，在两页间轮流写入并按序号与 CRC-32 选出最新有效记录 */
bool config_save(uint32_t baud_rate, int32_t calib_mv) {
    (void)flash_erase_page;
    (void)flash_prog_halfword;
    (void)crc32_calc;
    (void)baud_rate;
    (void)calib_mv;
    return false;
}

bool config_load(config_rec_t *out) {
    (void)out;
    return false;
}

int main(void) {
    memset(g_flash, 0xFF, sizeof(g_flash));
    printf("starter ready: sizeof(config_rec_t) = %u\n",
           (unsigned)sizeof(config_rec_t));
    return 0;
}
