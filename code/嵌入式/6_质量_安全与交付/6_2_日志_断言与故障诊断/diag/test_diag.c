/*
 * test_diag_record.c：诊断记录的主机测试。
 *
 * 编译：gcc -std=c17 -Wall -Wextra -I. diag_record.c test_diag.c -o test_diag
 * 讲解见教程 6.2 节「日志、断言与故障诊断」。
 */
#include <stdio.h>
#include <string.h>

#include "diag_record.h"

static int g_failures;

#define CHECK(cond)                                                          \
    do {                                                                     \
        if (!(cond)) {                                                       \
            printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);           \
            g_failures++;                                                    \
        }                                                                    \
    } while (0)

static void test_crc_known_value(void) {
    /* "123456789" 的 CRC-32 是标准校验值 0xCBF43926 */
    CHECK(diag_crc32("123456789", 9) == 0xCBF43926UL);
    printf("test_crc_known_value: 通过\n");
}

static void test_empty_record_invalid(void) {
    struct diag_record rec;
    memset(&rec, 0, sizeof rec);
    CHECK(!diag_record_valid(&rec));
    printf("test_empty_record_invalid: 通过\n");
}

static void test_fill_and_valid(void) {
    struct diag_record rec;
    struct diag_frame frame = {
        .r0 = 0,
        .r1 = 0,
        .r2 = 0,
        .r3 = 0,
        .r12 = 0,
        .lr = 0x08000121u,
        .pc = 0x08000247u,
        .xpsr = 0x61000000u,
    };
    memset(&rec, 0, sizeof rec);
    diag_record_fill(&rec, DIAG_REASON_HARDFAULT, &frame, "sensor", 12345u);

    CHECK(diag_record_valid(&rec));
    CHECK(rec.reason == DIAG_REASON_HARDFAULT);
    CHECK(rec.seq == 1u);
    CHECK(strcmp(rec.task, "sensor") == 0);
    CHECK(rec.frame.pc == 0x08000247u);
    printf("test_fill_and_valid: 通过\n");
}

static void test_corruption_detected(void) {
    struct diag_record rec;
    struct diag_frame frame = {0};
    memset(&rec, 0, sizeof rec);
    diag_record_fill(&rec, DIAG_REASON_ASSERT, &frame, "main", 100u);
    CHECK(diag_record_valid(&rec));

    /* 改掉一个字段，CRC 对不上 */
    rec.frame.lr ^= 1u;
    CHECK(!diag_record_valid(&rec));
    /* 改魔数同样无效 */
    rec.frame.lr ^= 1u;
    CHECK(diag_record_valid(&rec));
    rec.magic = 0;
    CHECK(!diag_record_valid(&rec));
    printf("test_corruption_detected: 通过\n");
}

static void test_crumbs_wrap(void) {
    struct diag_record rec;
    struct diag_frame frame = {0};
    memset(&rec, 0, sizeof rec);

    for (uint16_t i = 0; i < DIAG_CRUMB_MAX; ++i) {
        diag_crumb_push(&rec, (uint16_t)(0x100u + i), i, i * 10u);
    }
    CHECK(rec.crumb_count == DIAG_CRUMB_MAX);
    CHECK(rec.crumb_head == 0u);

    /* 再推一条：最老的一条被覆盖，头指针回到 1 */
    diag_crumb_push(&rec, 0x2FFu, 0, 999u);
    CHECK(rec.crumb_count == DIAG_CRUMB_MAX);
    CHECK(rec.crumb_head == 1u);
    CHECK(rec.crumbs[0].code == 0x2FFu);
    CHECK(rec.crumbs[1].code == 0x101u);

    diag_record_fill(&rec, DIAG_REASON_HANG, &frame, "idle", 500u);
    CHECK(diag_record_valid(&rec));
    CHECK(rec.crumb_count == DIAG_CRUMB_MAX);
    printf("test_crumbs_wrap: 通过\n");
}

static void test_task_name_truncated(void) {
    struct diag_record rec;
    memset(&rec, 0, sizeof rec);
    diag_record_fill(&rec, DIAG_REASON_WATCHDOG, NULL,
                     "a_very_long_task_name", 1u);
    CHECK(strlen(rec.task) == DIAG_TASK_NAME_MAX - 1u);
    CHECK(diag_record_valid(&rec));
    printf("test_task_name_truncated: 通过\n");
}

int main(void) {
    test_crc_known_value();
    test_empty_record_invalid();
    test_fill_and_valid();
    test_corruption_detected();
    test_crumbs_wrap();
    test_task_name_truncated();

    if (g_failures == 0) {
        printf("全部测试通过\n");
        return 0;
    }
    printf("%d 项检查失败\n", g_failures);
    return 1;
}