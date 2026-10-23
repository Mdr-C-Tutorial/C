/*
 * test_pager.c：页缓存、页的分配、事务与热日志恢复。
 */
#include "file.h"
#include "pager.h"
#include "test.h"

#include <limits.h>
#include <string.h>

#define PAGER_DB "test_pager.db"

static void fill(uint8_t *data, int seed) {
    for (int i = 0; i < DB_PAGE_SIZE; i++) {
        data[i] = (uint8_t)(seed * 31 + i * 7);
    }
}

static bool filled_with(const uint8_t *data, int seed) {
    for (int i = 0; i < DB_PAGE_SIZE; i++) {
        if (data[i] != (uint8_t)(seed * 31 + i * 7)) {
            return false;
        }
    }
    return true;
}

static pager *open_pager(size_t frames) {
    pager *p = NULL;
    uint32_t recovered;
    CHECK_OK(pager_open(PAGER_DB, frames, &p, &recovered));
    return p;
}

/* 新建一个库，第 1 到 n 页的内容分别是 fill(1) 到 fill(n)。 */
static pager *create_pages(int n, size_t frames) {
    remove_db_files(PAGER_DB);
    pager *p = open_pager(frames);
    CHECK_OK(pager_begin(p));
    for (int i = 1; i <= n; i++) {
        page *pg;
        CHECK_OK(pager_allocate(p, &pg));
        CHECK(pg->pgno == (uint32_t)i);
        fill(pg->data, i);
        pager_unpin(pg);
    }
    CHECK_OK(pager_commit(p));
    return p;
}

/* 逐页核对：第 i 页的内容应当是 fill(i + delta)。 */
static void check_pages(pager *p, int n, int delta) {
    for (int i = 1; i <= n; i++) {
        page *pg;
        CHECK_OK(pager_get(p, (uint32_t)i, &pg));
        CHECK(filled_with(pg->data, i + delta));
        pager_unpin(pg);
    }
    CHECK(pager_pinned(p) == 0);
}

static void modify_pages(pager *p, int n, int delta) {
    for (int i = 1; i <= n; i++) {
        page *pg;
        CHECK_OK(pager_get(p, (uint32_t)i, &pg));
        CHECK_OK(pager_write(p, pg));
        fill(pg->data, i + delta);
        pager_unpin(pg);
    }
}

static void test_new_file_has_header(void) {
    remove_db_files(PAGER_DB);
    pager *p = open_pager(8);
    CHECK(pager_page_count(p) == 1);
    pager_close(p);

    FILE *fp = fopen(PAGER_DB, "rb");
    char magic[16] = {0};
    CHECK(fp != NULL);
    if (fp != NULL) {
        CHECK(fread(magic, 1, sizeof magic, fp) == sizeof magic);
        fclose(fp);
    }
    CHECK(memcmp(magic, "MdrDB format 1", 15) == 0);
}

static void test_commit_persists(void) {
    pager *p = create_pages(3, 8);
    pager_close(p);
    p = open_pager(8);
    CHECK(pager_page_count(p) == 4);
    check_pages(p, 3, 0);
    pager_close(p);
}

static void test_rollback_restores_pages_and_count(void) {
    pager *p = create_pages(3, 8);
    CHECK_OK(pager_begin(p));
    modify_pages(p, 3, 100);
    page *pg;
    CHECK_OK(pager_allocate(p, &pg));
    pager_unpin(pg);
    CHECK(pager_page_count(p) == 5);
    CHECK_OK(pager_rollback(p));
    CHECK(pager_page_count(p) == 4);
    check_pages(p, 3, 0);
    pager_close(p);
}

/* 页框比修改的页少：事务中途就要换出脏页，回滚和提交都要正确。 */
static void test_spill_then_rollback_or_commit(void) {
    pager *p = create_pages(20, PAGER_MIN_FRAMES);
    unsigned long spills = pager_get_stats(p).spills;
    CHECK_OK(pager_begin(p));
    modify_pages(p, 20, 50);
    CHECK(pager_get_stats(p).spills > spills);
    CHECK_OK(pager_rollback(p));
    check_pages(p, 20, 0);

    CHECK_OK(pager_begin(p));
    modify_pages(p, 20, 50);
    CHECK_OK(pager_commit(p));
    pager_close(p);
    p = open_pager(PAGER_MIN_FRAMES);
    check_pages(p, 20, 50);
    pager_close(p);
}

/*
 * 进程在提交前死去：用故障注入让之后的写操作全部失败，关闭时的回滚
 * 因此写不进文件。数据库文件里留着换出的新内容，日志里是旧内容。
 */
static void test_hot_journal_recovery(void) {
    pager *p = create_pages(20, PAGER_MIN_FRAMES);
    CHECK_OK(pager_begin(p));
    modify_pages(p, 20, 50);
    fault_arm(0, FAULT_RETURN_ERROR);
    pager_close(p);
    CHECK(fault_crashed());
    fault_disarm();

    char journal[64];
    snprintf(journal, sizeof journal, "%s-journal", PAGER_DB);
    CHECK(file_exists(journal));
    uint32_t recovered = 0;
    CHECK_OK(pager_open(PAGER_DB, 8, &p, &recovered));
    CHECK(recovered == 20);
    CHECK(!file_exists(journal));
    check_pages(p, 20, 0);
    pager_close(p);
}

static void test_misuse(void) {
    pager *p = create_pages(1, 8);
    page *pg;
    CHECK_OK(pager_get(p, 1, &pg));
    CHECK_STATUS(pager_write(p, pg), DB_MISUSE); /* 没有事务 */
    pager_unpin(pg);
    CHECK_STATUS(pager_allocate(p, &pg), DB_MISUSE);
    CHECK_STATUS(pager_commit(p), DB_MISUSE);
    CHECK_STATUS(pager_rollback(p), DB_MISUSE);
    CHECK_OK(pager_begin(p));
    CHECK_STATUS(pager_begin(p), DB_MISUSE);
    CHECK_OK(pager_rollback(p));
    CHECK_STATUS(pager_get(p, 2, &pg), DB_CORRUPT); /* 超出页数 */
    pager_close(p);
}

static void test_pins_and_cache_full(void) {
    pager *p = create_pages(5, PAGER_MIN_FRAMES);
    page *held[PAGER_MIN_FRAMES];
    for (int i = 0; i < PAGER_MIN_FRAMES; i++) {
        CHECK_OK(pager_get(p, (uint32_t)i + 1, &held[i]));
    }
    page *again;
    CHECK_OK(pager_get(p, 1, &again)); /* 已在缓存中：同一个页框 */
    CHECK(again == held[0] && again->pins == 2);
    pager_unpin(again);
    page *extra;
    CHECK_STATUS(pager_get(p, 5, &extra), DB_CACHE_FULL);
    for (int i = 0; i < PAGER_MIN_FRAMES; i++) {
        pager_unpin(held[i]);
    }
    CHECK_OK(pager_get(p, 5, &extra));
    pager_unpin(extra);
    CHECK(pager_pinned(p) == 0);
    pager_close(p);
}

static void write_bytes(const char *path, const void *data, size_t size) {
    FILE *fp = fopen(path, "wb");
    CHECK(fp != NULL);
    if (fp != NULL) {
        CHECK(fwrite(data, 1, size, fp) == size);
        fclose(fp);
    }
}

static void test_rejects_foreign_files(void) {
    pager *p = NULL;
    uint32_t recovered;
    remove_db_files(PAGER_DB);
    write_bytes(PAGER_DB, "hello", 5);
    CHECK_STATUS(pager_open(PAGER_DB, 8, &p, &recovered), DB_CORRUPT);
    write_bytes(PAGER_DB, "MdrDB format 2", 14); /* 不是新库文件头的开头 */
    CHECK_STATUS(pager_open(PAGER_DB, 8, &p, &recovered), DB_CORRUPT);

    uint8_t page0[DB_PAGE_SIZE] = {0};
    memcpy(page0, "MdrDB format 1", 15);
    put_u32(page0 + 16, 1024); /* 页大小不对 */
    put_u32(page0 + 20, 1);
    write_bytes(PAGER_DB, page0, sizeof page0);
    CHECK_STATUS(pager_open(PAGER_DB, 8, &p, &recovered), DB_CORRUPT);
    remove_db_files(PAGER_DB);
}

/* 新建时文件头只写进一半就中断了：已有的字节与新库的文件头一致，重写即可。 */
static void test_half_written_header(void) {
    uint8_t page0[DB_PAGE_SIZE] = {0};
    memcpy(page0, "MdrDB format 1", 15);
    put_u32(page0 + 16, DB_PAGE_SIZE);
    put_u32(page0 + 20, 1);
    remove_db_files(PAGER_DB);
    write_bytes(PAGER_DB, page0, DB_PAGE_SIZE / 2);
    pager *p = open_pager(8);
    CHECK(p != NULL && pager_page_count(p) == 1);
    pager_close(p);

    FILE *fp = fopen(PAGER_DB, "rb");
    CHECK(fp != NULL);
    if (fp != NULL) {
        CHECK(fseek(fp, 0, SEEK_END) == 0 && ftell(fp) == DB_PAGE_SIZE);
        fclose(fp);
    }
    remove_db_files(PAGER_DB);
}

int main(void) {
    RUN_TEST(test_new_file_has_header);
    RUN_TEST(test_commit_persists);
    RUN_TEST(test_rollback_restores_pages_and_count);
    RUN_TEST(test_spill_then_rollback_or_commit);
    RUN_TEST(test_hot_journal_recovery);
    RUN_TEST(test_misuse);
    RUN_TEST(test_pins_and_cache_full);
    RUN_TEST(test_rejects_foreign_files);
    RUN_TEST(test_half_written_header);
    remove_db_files(PAGER_DB);
    return test_summary();
}
