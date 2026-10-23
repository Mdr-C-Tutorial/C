/*
 * diag_record.c：诊断记录的填充与校验。
 *
 * 记录先写内容、最后写 CRC，读取方只在 CRC 对上时才相信它。
 * 讲解见教程 6.2 节「日志、断言与故障诊断」。
 */
#include <string.h>

#include "diag_record.h"

uint32_t diag_crc32(const void *data, size_t len) {
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

bool diag_record_valid(const struct diag_record *rec) {
    if (rec->magic != DIAG_MAGIC || rec->version != DIAG_VERSION) {
        return false;
    }
    return rec->crc32 == diag_crc32(rec, offsetof(struct diag_record, crc32));
}

void diag_crumb_push(struct diag_record *rec, uint16_t code, uint16_t arg,
                     uint32_t tick_ms) {
    struct diag_crumb *c = &rec->crumbs[rec->crumb_head];
    c->tick_ms = tick_ms;
    c->code = code;
    c->arg = arg;
    rec->crumb_head = (uint8_t)((rec->crumb_head + 1u) % DIAG_CRUMB_MAX);
    if (rec->crumb_count < DIAG_CRUMB_MAX) {
        rec->crumb_count++;
    }
}

void diag_record_fill(struct diag_record *rec, enum diag_reason reason,
                      const struct diag_frame *frame, const char *task,
                      uint32_t uptime_ms) {
    rec->magic = DIAG_MAGIC;
    rec->version = DIAG_VERSION;
    rec->reason = (uint16_t)reason;
    rec->seq++;
    if (frame != NULL) {
        rec->frame = *frame;
    }
    memset(rec->task, 0, sizeof rec->task);
    if (task != NULL) {
        strncpy(rec->task, task, sizeof rec->task - 1u);
    }
    rec->uptime_ms = uptime_ms;
    rec->crc32 = diag_crc32(rec, offsetof(struct diag_record, crc32));
}