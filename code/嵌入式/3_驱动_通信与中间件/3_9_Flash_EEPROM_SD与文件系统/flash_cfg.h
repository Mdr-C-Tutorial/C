#ifndef FLASH_CFG_H
#define FLASH_CFG_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define CFG_MAGIC     0x43464731UL /* "CFG1" */
#define CFG_VERSION   1u
#define CFG_PAGE_SIZE 1024u

typedef struct {
    uint32_t magic;        /* 魔数 0x43464731 */
    uint16_t version;      /* 记录格式版本 */
    uint16_t reserved;     /* 显式对齐填充，置 0 */
    uint32_t seq;          /* 单调递增序号 */
    uint16_t node_addr;    /* 设备地址 */
    uint16_t report_sec;   /* 上报周期（秒） */
    int32_t  calib_offset; /* 零点校准偏移（mV） */
    int32_t  calib_gain;   /* 增益系数（千分比） */
    uint32_t crc32;        /* 前 24 字节的 CRC-32 */
} cfg_record_t;

_Static_assert(sizeof(cfg_record_t) == 28, "cfg_record_t size must be 28 bytes");

/* 记录在页内按槽追加：一页 1024 字节，每条记录 28 字节，共 36 槽 */
#define CFG_SLOT_COUNT (CFG_PAGE_SIZE / sizeof(cfg_record_t))

typedef struct {
    bool (*erase_page)(uint32_t page_offset);
    bool (*program_halfword)(uint32_t offset, uint16_t halfword);
    void (*read_bytes)(uint32_t offset, void *buf, size_t len);
} flash_ops_t;

uint32_t crc32_calc(const void *data, size_t len);
bool cfg_record_verify(const cfg_record_t *rec);
bool flash_cfg_load(const flash_ops_t *ops, cfg_record_t *out);
bool flash_cfg_save(const flash_ops_t *ops, cfg_record_t *io_rec);

#endif /* FLASH_CFG_H */
