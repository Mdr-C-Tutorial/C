/*
 * diag_record.h：掉电不丢的诊断记录与面包屑。
 *
 * 一条记录包含魔数、版本、序号、完整异常栈帧、故障状态寄存器、
 * 出故障时的任务名、运行时间以及最近若干条面包屑，末尾用 CRC-32 校验。
 *
 * 讲解见教程 6.2 节「日志、断言与故障诊断」。
 */
#ifndef DIAG_RECORD_H
#define DIAG_RECORD_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define DIAG_MAGIC 0x44494147u /* "DIAG" */
#define DIAG_VERSION 2u
#define DIAG_CRUMB_MAX 16u
#define DIAG_TASK_NAME_MAX 8u

/* 记录里的故障类别 */
enum diag_reason {
    DIAG_REASON_NONE = 0,
    DIAG_REASON_HARDFAULT = 1,
    DIAG_REASON_ASSERT = 2,
    DIAG_REASON_STACK_OVERFLOW = 3,
    DIAG_REASON_HANG = 4,
    DIAG_REASON_WATCHDOG = 5,
};

/* 异常入栈的 8 个寄存器，顺序由 Cortex-M 硬件固定 */
struct diag_frame {
    uint32_t r0;
    uint32_t r1;
    uint32_t r2;
    uint32_t r3;
    uint32_t r12;
    uint32_t lr;
    uint32_t pc;
    uint32_t xpsr;
};

/* 面包屑：故障之前发生的事件，环形覆盖 */
struct diag_crumb {
    uint32_t tick_ms;
    uint16_t code;
    uint16_t arg;
};

struct diag_record {
    uint32_t magic;
    uint16_t version;
    uint16_t reason; /* enum diag_reason */
    uint32_t seq;
    struct diag_frame frame;
    uint32_t cfsr;
    uint32_t hfsr;
    uint32_t bfar;
    uint32_t mmfar;
    char task[DIAG_TASK_NAME_MAX];
    uint32_t uptime_ms;
    uint8_t crumb_count;
    uint8_t crumb_head;
    uint16_t reserved;
    struct diag_crumb crumbs[DIAG_CRUMB_MAX];
    uint32_t crc32;
};

uint32_t diag_crc32(const void *data, size_t len);

/* 判断记录是否完整：魔数、版本与 CRC 都对才算有效 */
bool diag_record_valid(const struct diag_record *rec);

/* 面包屑环形缓冲 */
void diag_crumb_push(struct diag_record *rec, uint16_t code, uint16_t arg,
                     uint32_t tick_ms);

/* 记录一次故障：填好记录后提升序号并重算 CRC */
void diag_record_fill(struct diag_record *rec, enum diag_reason reason,
                      const struct diag_frame *frame, const char *task,
                      uint32_t uptime_ms);

#endif /* DIAG_RECORD_H */