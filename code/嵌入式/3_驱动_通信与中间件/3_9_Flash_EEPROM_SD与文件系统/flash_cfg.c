#include "flash_cfg.h"
#include <string.h>

static uint32_t slot_offset(size_t slot) {
    return (uint32_t)(slot * sizeof(cfg_record_t));
}

uint32_t crc32_calc(const void *data, size_t len) {
    const uint8_t *p = (const uint8_t *)data;
    uint32_t crc = 0xFFFFFFFFUL;
    for (size_t i = 0; i < len; ++i) {
        crc ^= (uint32_t)p[i];
        for (int b = 0; b < 8; ++b) {
            if ((crc & 1UL) != 0UL) {
                crc = (crc >> 1) ^ 0xEDB88320UL;
            } else {
                crc >>= 1;
            }
        }
    }
    return ~crc;
}

bool cfg_record_verify(const cfg_record_t *rec) {
    if (rec->magic != CFG_MAGIC || rec->version != CFG_VERSION) {
        return false;
    }
    uint32_t expected = crc32_calc(rec, offsetof(cfg_record_t, crc32));
    return rec->crc32 == expected;
}

/* 扫描整页：找出最后一个校验通过的槽，以及第一个还没写过的空槽。
 * 断电留下的半条记录魔数已写、CRC 不对，扫描时跳过，但它占住的槽也不会被再用。 */
static bool scan_page(const flash_ops_t *ops, size_t *last_valid,
                      size_t *first_free) {
    bool found = false;
    size_t last = 0u;

    *first_free = CFG_SLOT_COUNT;
    for (size_t i = 0u; i < CFG_SLOT_COUNT; ++i) {
        cfg_record_t rec;
        ops->read_bytes(slot_offset(i), &rec, sizeof(rec));
        if (rec.magic == 0xFFFFFFFFUL) {
            *first_free = i;
            break;
        }
        if (cfg_record_verify(&rec)) {
            found = true;
            last = i;
        }
    }
    *last_valid = last;
    return found;
}

static bool write_record(const flash_ops_t *ops, uint32_t offset,
                         const cfg_record_t *rec) {
    const uint8_t *raw = (const uint8_t *)rec;
    for (size_t i = 0; i < sizeof(*rec); i += 2u) {
        uint16_t hw = (uint16_t)raw[i] | ((uint16_t)raw[i + 1u] << 8);
        if (!ops->program_halfword(offset + (uint32_t)i, hw)) {
            return false;
        }
    }
    cfg_record_t verify_buf;
    ops->read_bytes(offset, &verify_buf, sizeof(verify_buf));
    return memcmp(&verify_buf, rec, sizeof(*rec)) == 0;
}

bool flash_cfg_load(const flash_ops_t *ops, cfg_record_t *out) {
    size_t last_valid = 0u;
    size_t first_free = 0u;

    if (!scan_page(ops, &last_valid, &first_free)) {
        return false;
    }
    ops->read_bytes(slot_offset(last_valid), out, sizeof(*out));
    return cfg_record_verify(out);
}

bool flash_cfg_save(const flash_ops_t *ops, cfg_record_t *io_rec) {
    size_t last_valid = 0u;
    size_t first_free = 0u;
    uint32_t next_seq = 1u;

    bool found = scan_page(ops, &last_valid, &first_free);
    if (found) {
        cfg_record_t last;
        ops->read_bytes(slot_offset(last_valid), &last, sizeof(last));
        next_seq = last.seq + 1u;
    }

    /* 槽用完了：整页擦一次，从第 0 槽重新开始，序号继续递增 */
    if (first_free >= CFG_SLOT_COUNT) {
        if (!ops->erase_page(0u)) {
            return false;
        }
        first_free = 0u;
    }

    io_rec->magic = CFG_MAGIC;
    io_rec->version = CFG_VERSION;
    io_rec->reserved = 0u;
    io_rec->seq = next_seq;
    io_rec->crc32 = crc32_calc(io_rec, offsetof(cfg_record_t, crc32));

    return write_record(ops, slot_offset(first_free), io_rec);
}