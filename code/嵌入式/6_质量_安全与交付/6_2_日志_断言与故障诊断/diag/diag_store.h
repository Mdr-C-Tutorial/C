/*
 * diag_store.h：把诊断记录追加到外部 Flash 的诊断区。
 *
 * 存储通过一组简单的函数指针完成，主机测试用自己的模拟 Flash，
 * 固件里接 3.9 的 w25q 驱动。分区按跨页约定：
 * W25Q64 的 0x020000–0x02FFFF 是诊断与核心转储区。
 *
 * 讲解见教程 6.2 节「日志、断言与故障诊断」。
 */
#ifndef DIAG_STORE_H
#define DIAG_STORE_H

#include <stdbool.h>
#include <stdint.h>

#include "diag_record.h"

#define DIAG_STORE_BASE 0x020000u
#define DIAG_STORE_SIZE 0x010000u
#define DIAG_SLOT_SIZE 256u
#define DIAG_SLOT_COUNT (DIAG_STORE_SIZE / DIAG_SLOT_SIZE)
#define DIAG_RECORD_MAX ((uint32_t)sizeof(struct diag_record))

/* 外部 Flash 的最小接口：读、擦一个扇区、按页写 */
struct diag_flash_ops {
    void (*read)(uint32_t addr, void *buf, uint32_t len);
    bool (*erase_sector)(uint32_t addr);
    bool (*write)(uint32_t addr, const void *buf, uint32_t len);
};

/* 每个槽的头：魔数、序号、长度、CRC */
struct diag_slot_header {
    uint32_t magic;
    uint32_t seq;
    uint16_t len;
    uint16_t pad;
    uint32_t crc32;
};

#define DIAG_SLOT_MAGIC 0x534C4F54u /* "SLOT" */

/* 下一次要写的槽号；存进最后一槽旁边不方便，调用者自己记在 RAM 或片内 Flash */
struct diag_store {
    const struct diag_flash_ops *ops;
    uint32_t next_slot;
};

void diag_store_init(struct diag_store *st, const struct diag_flash_ops *ops,
                     uint32_t next_slot);

/* 追加一条记录；同一槽已有数据时擦掉整个扇区再写 */
bool diag_store_append(struct diag_store *st, const struct diag_record *rec);

/* 读出第 index 个槽里的记录；无效返回 false */
bool diag_store_read(const struct diag_store *st, uint32_t index,
                     struct diag_record *out);

/* 扫描所有槽，返回有效记录的条数 */
uint32_t diag_store_scan(const struct diag_store *st);

#endif /* DIAG_STORE_H */