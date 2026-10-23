/*
 * format.h：磁盘格式的公共定义。
 *
 * 文件里的多字节整数一律按大端序（高位字节在前）存放。同一个文件在
 * 任何机器上都是同一串字节，用 xxd 查看时数字也按书写顺序排列。
 * 读写时逐字节拼装，不把结构体直接写进文件：结构体里有填充字节，
 * 布局还会随编译器和平台变化。
 */
#ifndef MDRDB_FORMAT_H
#define MDRDB_FORMAT_H

#include <stdint.h>

#define DB_PAGE_SIZE 4096

/*
 * 页数上限。文件偏移用 long 表示（fseek 的参数类型），Windows 上
 * long 只有 32 位，文件不能超过 2 GiB：524288 页 × 4096 字节 = 2^31。
 */
#define DB_MAX_PAGES 524288u

static inline uint16_t get_u16(const uint8_t *p) {
    return (uint16_t)((p[0] << 8) | p[1]);
}

static inline void put_u16(uint8_t *p, uint16_t v) {
    p[0] = (uint8_t)(v >> 8);
    p[1] = (uint8_t)v;
}

/* 先转成 uint32_t 再移位：uint8_t 会提升为 int，左移 24 位可能溢出。 */
static inline uint32_t get_u32(const uint8_t *p) {
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] << 8) | (uint32_t)p[3];
}

static inline void put_u32(uint8_t *p, uint32_t v) {
    p[0] = (uint8_t)(v >> 24);
    p[1] = (uint8_t)(v >> 16);
    p[2] = (uint8_t)(v >> 8);
    p[3] = (uint8_t)v;
}

#endif
