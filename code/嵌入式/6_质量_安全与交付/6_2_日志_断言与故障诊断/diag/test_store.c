/*
 * test_diag_store.c：用模拟 Flash 测试诊断记录的追加与掉电安全。
 *
 * 模拟的 NOR Flash 遵守两条物理规律：擦除把整扇区置 0xFF，写入只能把 1 变成 0。
 * 还可以在第 N 次写入时断电，验证半截记录不会被当成有效记录。
 *
 * 编译：gcc -std=c17 -Wall -Wextra -I. diag_record.c diag_store.c test_store.c -o test_store
 *
 * 讲解见教程 6.2 节「日志、断言与故障诊断」。
 */
#include <stdio.h>
#include <string.h>

#include "diag_record.h"
#include "diag_store.h"

/* 只模拟记录区那一部分：0x10000 字节，从 0 开始编号 */
static uint8_t g_flash[DIAG_STORE_SIZE];
static uint32_t g_write_count;
static uint32_t g_fail_at_write; /* 0 表示不断电 */
static uint32_t g_erase_count;

static int g_failures;

#define CHECK(cond)                                                          \
    do {                                                                     \
        if (!(cond)) {                                                       \
            printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);           \
            g_failures++;                                                    \
        }                                                                    \
    } while (0)

static void sim_read(uint32_t addr, void *buf, uint32_t len) {
    memcpy(buf, &g_flash[addr - DIAG_STORE_BASE], len);
}

static bool sim_erase_sector(uint32_t addr) {
    g_erase_count++;
    memset(&g_flash[addr - DIAG_STORE_BASE], 0xFF, 4096u);
    return true;
}

static bool sim_write(uint32_t addr, const void *buf, uint32_t len) {
    g_write_count++;
    if (g_fail_at_write != 0u && g_write_count >= g_fail_at_write) {
        return false;
    }
    const uint8_t *src = (const uint8_t *)buf;
    uint8_t *dst = &g_flash[addr - DIAG_STORE_BASE];
    for (uint32_t i = 0; i < len; ++i) {
        dst[i] &= src[i];
    }
    return true;
}

static const struct diag_flash_ops g_ops = {
    .read = sim_read,
    .erase_sector = sim_erase_sector,
    .write = sim_write,
};

static struct diag_record make_record(uint32_t seq, int16_t reason) {
    struct diag_record rec;
    struct diag_frame frame = {0};
    memset(&rec, 0, sizeof rec);
    diag_record_fill(&rec, (enum diag_reason)reason, &frame, "sensor", seq * 10u);
    rec.seq = seq;
    /* 改过 seq 要重算 CRC */
    rec.crc32 = diag_crc32(&rec, offsetof(struct diag_record, crc32));
    return rec;
}

static void reset_flash(void) {
    memset(g_flash, 0xFF, sizeof g_flash);
    g_write_count = 0;
    g_fail_at_write = 0;
    g_erase_count = 0;
}

static void test_append_and_read_back(void) {
    reset_flash();
    struct diag_store st;
    diag_store_init(&st, &g_ops, 0u);

    struct diag_record rec = make_record(1u, DIAG_REASON_HARDFAULT);
    CHECK(diag_store_append(&st, &rec));
    CHECK(st.next_slot == 1u);
    CHECK(g_erase_count == 1u);

    struct diag_record out;
    CHECK(diag_store_read(&st, 0u, &out));
    CHECK(out.seq == 1u);
    CHECK(out.reason == DIAG_REASON_HARDFAULT);
    CHECK(strcmp(out.task, "sensor") == 0);
    printf("test_append_and_read_back: 通过\n");
}

static void test_wrap_across_sectors(void) {
    reset_flash();
    struct diag_store st;
    diag_store_init(&st, &g_ops, 0u);

    /* 16 个槽一扇区；写满一扇区后再写，应该擦第二个扇区 */
    for (uint32_t i = 1u; i <= 20u; ++i) {
        struct diag_record rec = make_record(i, DIAG_REASON_ASSERT);
        CHECK(diag_store_append(&st, &rec));
    }
    CHECK(g_erase_count == 2u);
    CHECK(st.next_slot == 20u);

    /* 20 条都还在，后续写入不会破坏前面的扇区 */
    CHECK(diag_store_scan(&st) == 20u);
    struct diag_record out;
    CHECK(diag_store_read(&st, 19u, &out));
    CHECK(out.seq == 20u);
    printf("test_wrap_across_sectors: 通过\n");
}

static void test_power_loss_before_header(void) {
    reset_flash();
    struct diag_store st;
    diag_store_init(&st, &g_ops, 0u);

    /* 第一次写把数据写进槽 0 后断电，头没写 */
    struct diag_record rec = make_record(1u, DIAG_REASON_HARDFAULT);
    g_fail_at_write = 2u; /* 第 2 次写（写头）失败 */
    CHECK(!diag_store_append(&st, &rec));
    g_fail_at_write = 0;

    /* 重启后这一槽无效 */
    struct diag_record out;
    CHECK(!diag_store_read(&st, 0u, &out));

    /* 槽 0 是扇区开头，再次追加时整扇区擦掉重写，半截数据随之消失 */
    diag_store_init(&st, &g_ops, 0u);
    uint32_t before = g_erase_count;
    rec = make_record(2u, DIAG_REASON_ASSERT);
    CHECK(diag_store_append(&st, &rec));
    CHECK(g_erase_count == before + 1u);
    CHECK(st.next_slot == 1u);
    CHECK(diag_store_read(&st, 0u, &out));
    CHECK(out.seq == 2u);
    CHECK(out.reason == DIAG_REASON_ASSERT);
    printf("test_power_loss_before_header: 通过\n");
}

static void test_storage_full_wraps(void) {
    reset_flash();
    struct diag_store st;
    diag_store_init(&st, &g_ops, DIAG_SLOT_COUNT - 1u);

    struct diag_record rec = make_record(1u, DIAG_REASON_HANG);
    CHECK(diag_store_append(&st, &rec));
    CHECK(st.next_slot == 0u); /* 回绕到开头 */
    struct diag_record out;
    CHECK(diag_store_read(&st, DIAG_SLOT_COUNT - 1u, &out));
    CHECK(out.reason == DIAG_REASON_HANG);
    printf("test_storage_full_wraps: 通过\n");
}

int main(void) {
    printf("sizeof(diag_record) = %u\n", (unsigned)sizeof(struct diag_record));
    test_append_and_read_back();
    test_wrap_across_sectors();
    test_power_loss_before_header();
    test_storage_full_wraps();

    if (g_failures == 0) {
        printf("全部测试通过\n");
        return 0;
    }
    printf("%d 项检查失败\n", g_failures);
    return 1;
}