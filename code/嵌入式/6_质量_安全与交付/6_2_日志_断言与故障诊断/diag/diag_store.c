/*
 * diag_store.c：诊断记录的追加存储。
 *
 * 每个槽 256 字节：16 字节头加最多 208 字节的记录。先写记录，最后写头，
 * 所以写到一半断电只会留下一个头无效的槽。进入新扇区时整扇区擦除一次，
 * 与 NOR Flash 的擦除单位一致。
 *
 * 讲解见教程 6.2 节「日志、断言与故障诊断」。
 */
#include <string.h>

#include "diag_store.h"

#define DIAG_SLOTS_PER_SECTOR (4096u / DIAG_SLOT_SIZE)

static uint32_t slot_addr(uint32_t index) {
    return DIAG_STORE_BASE + index * DIAG_SLOT_SIZE;
}

void diag_store_init(struct diag_store *st, const struct diag_flash_ops *ops,
                     uint32_t next_slot) {
    st->ops = ops;
    st->next_slot = next_slot % DIAG_SLOT_COUNT;
}

static bool slot_erased(const struct diag_store *st, uint32_t index) {
    uint32_t magic = 0;
    st->ops->read(slot_addr(index), &magic, sizeof magic);
    return magic == 0xFFFFFFFFu;
}

bool diag_store_append(struct diag_store *st, const struct diag_record *rec) {
    uint32_t index = st->next_slot;
    uint32_t base;

    /* 进入新扇区：整扇区擦掉一次 */
    if ((index % DIAG_SLOTS_PER_SECTOR) == 0u) {
        if (!st->ops->erase_sector(slot_addr(index))) {
            return false;
        }
    } else if (!slot_erased(st, index)) {
        /* 槽里还有旧数据（只在外部改动过记录区时出现），跳到下一扇区开头 */
        index += DIAG_SLOTS_PER_SECTOR - (index % DIAG_SLOTS_PER_SECTOR);
        index %= DIAG_SLOT_COUNT;
        if (!st->ops->erase_sector(slot_addr(index))) {
            return false;
        }
    }

    base = slot_addr(index);
    struct diag_slot_header hdr = {
        .magic = DIAG_SLOT_MAGIC,
        .seq = rec->seq,
        .len = (uint16_t)sizeof *rec,
        .pad = 0xFFFFu,
        .crc32 = diag_crc32(rec, sizeof *rec),
    };

    /* 先写记录，再写头；头写完这一槽才算有效 */
    if (!st->ops->write(base + (uint32_t)sizeof hdr, rec, sizeof *rec)) {
        return false;
    }
    if (!st->ops->write(base, &hdr, sizeof hdr)) {
        return false;
    }

    st->next_slot = (index + 1u) % DIAG_SLOT_COUNT;
    return true;
}

static bool read_record(const struct diag_store *st, uint32_t index,
                        struct diag_record *out) {
    uint32_t base = slot_addr(index);
    struct diag_slot_header hdr;

    st->ops->read(base, &hdr, sizeof hdr);
    if (hdr.magic != DIAG_SLOT_MAGIC || hdr.len != sizeof *out) {
        return false;
    }
    st->ops->read(base + (uint32_t)sizeof hdr, out, sizeof *out);
    return hdr.crc32 == diag_crc32(out, sizeof *out);
}

bool diag_store_read(const struct diag_store *st, uint32_t index,
                     struct diag_record *out) {
    if (index >= DIAG_SLOT_COUNT) {
        return false;
    }
    return read_record(st, index, out);
}

uint32_t diag_store_scan(const struct diag_store *st) {
    uint32_t count = 0;
    struct diag_record rec;
    for (uint32_t i = 0; i < DIAG_SLOT_COUNT; ++i) {
        if (read_record(st, i, &rec)) {
            count++;
        }
    }
    return count;
}