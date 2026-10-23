/*
 * test_journal.c：回滚日志的格式与回放。
 */
#include "file.h"
#include "journal.h"
#include "test.h"

#include <string.h>

#define JOURNAL_DB "test_journal.db"
#define JOURNAL_FILE "test_journal.db-journal"

/* 3 页的"数据库文件"，第 i 页的每个字节都是 i。 */
static db_file *make_db(void) {
    remove(JOURNAL_DB);
    db_file *f = NULL;
    CHECK_OK(file_create(JOURNAL_DB, &f));
    uint8_t data[DB_PAGE_SIZE];
    for (int i = 0; f != NULL && i < 3; i++) {
        memset(data, i, sizeof data);
        CHECK_OK(file_write_at(f, (long)i * DB_PAGE_SIZE, data, sizeof data));
    }
    return f;
}

/* 日志里存第 1、2 页的原始内容，分别是全 0xA1 和全 0xA2。 */
static void make_journal(void) {
    journal j;
    uint8_t data[DB_PAGE_SIZE];
    CHECK_OK(journal_create(&j, JOURNAL_FILE));
    memset(data, 0xA1, sizeof data);
    CHECK_OK(journal_append(&j, 1, data));
    memset(data, 0xA2, sizeof data);
    CHECK_OK(journal_append(&j, 2, data));
    CHECK_OK(journal_sync(&j));
    journal_close(&j);
}

/* 一页的每个字节都相同时返回这个字节，否则返回 -1。 */
static int page_byte(db_file *f, uint32_t pgno) {
    uint8_t data[DB_PAGE_SIZE];
    size_t n = 0;
    CHECK_OK(file_read_at(f, (long)pgno * DB_PAGE_SIZE, data, sizeof data, &n));
    if (n != sizeof data) {
        return -1;
    }
    for (size_t i = 1; i < n; i++) {
        if (data[i] != data[0]) {
            return -1;
        }
    }
    return data[0];
}

/* 直接改写日志文件里的字节，模拟磁盘上的损坏。 */
static void poke(long offset, const void *bytes, size_t size) {
    db_file *f = NULL;
    CHECK_OK(file_open(JOURNAL_FILE, &f));
    if (f != NULL) {
        CHECK_OK(file_write_at(f, offset, bytes, size));
        file_close(f);
    }
}

static void test_layout(void) {
    make_journal();
    FILE *fp = fopen(JOURNAL_FILE, "rb");
    CHECK(fp != NULL);
    if (fp == NULL) {
        return;
    }
    uint8_t head[JOURNAL_HEADER_SIZE + 4];
    CHECK(fread(head, 1, sizeof head, fp) == sizeof head);
    CHECK(fseek(fp, 0, SEEK_END) == 0);
    long size = ftell(fp);
    fclose(fp);
    CHECK(memcmp(head, "MDBJRNL1", 8) == 0);
    CHECK(get_u32(head + 8) == DB_PAGE_SIZE);
    CHECK(get_u32(head + JOURNAL_HEADER_SIZE) == 1); /* 第一条记录的页号 */
    CHECK(size == JOURNAL_HEADER_SIZE + 2 * JOURNAL_RECORD_SIZE);
}

static void test_playback_restores_pages(void) {
    db_file *dbf = make_db();
    make_journal();
    uint32_t restored = 0;
    CHECK_OK(journal_playback(JOURNAL_FILE, dbf, &restored));
    CHECK(restored == 2);
    CHECK(page_byte(dbf, 0) == 0);
    CHECK(page_byte(dbf, 1) == 0xA1);
    CHECK(page_byte(dbf, 2) == 0xA2);
    file_close(dbf);
}

/* 写第二条记录时崩溃：记录只写进一半，回放到它为止。 */
static void test_torn_record_is_ignored(void) {
    db_file *dbf = make_db();
    journal j;
    uint8_t data[DB_PAGE_SIZE];
    remove(JOURNAL_FILE);
    fault_arm(3, FAULT_RETURN_ERROR); /* 创建、写头、第一条记录 */
    CHECK_OK(journal_create(&j, JOURNAL_FILE));
    memset(data, 0xA1, sizeof data);
    CHECK_OK(journal_append(&j, 1, data));
    memset(data, 0xA2, sizeof data);
    CHECK_STATUS(journal_append(&j, 2, data), DB_IO_ERROR);
    CHECK(fault_crashed());
    journal_close(&j);
    fault_disarm();

    uint32_t restored = 0;
    CHECK_OK(journal_playback(JOURNAL_FILE, dbf, &restored));
    CHECK(restored == 1);
    CHECK(page_byte(dbf, 1) == 0xA1);
    CHECK(page_byte(dbf, 2) == 2); /* 没有被半条记录覆盖 */
    file_close(dbf);
}

/*
 * 第二条记录长度完整、内容却是坏的。断电后文件长度已经更新、数据块
 * 还没写进去时就会这样。只有校验和能识别这种记录。
 */
static void test_bad_checksum_stops_playback(void) {
    db_file *dbf = make_db();
    make_journal();
    const uint8_t garbage = 0x5A;
    poke(JOURNAL_HEADER_SIZE + JOURNAL_RECORD_SIZE + 4 + 100, &garbage, 1);
    uint32_t restored = 0;
    CHECK_OK(journal_playback(JOURNAL_FILE, dbf, &restored));
    CHECK(restored == 1);
    CHECK(page_byte(dbf, 2) == 2);
    file_close(dbf);
}

static void test_bad_header_restores_nothing(void) {
    db_file *dbf = make_db();
    make_journal();
    uint8_t size_field[4];
    put_u32(size_field, 1024);
    poke(8, size_field, sizeof size_field); /* 页大小被改，校验和对不上 */
    uint32_t restored = 7;
    CHECK_OK(journal_playback(JOURNAL_FILE, dbf, &restored));
    CHECK(restored == 0);
    CHECK(page_byte(dbf, 1) == 1 && page_byte(dbf, 2) == 2);
    file_close(dbf);
}

int main(void) {
    RUN_TEST(test_layout);
    RUN_TEST(test_playback_restores_pages);
    RUN_TEST(test_torn_record_is_ignored);
    RUN_TEST(test_bad_checksum_stops_playback);
    RUN_TEST(test_bad_header_restores_nothing);
    remove(JOURNAL_DB);
    remove(JOURNAL_FILE);
    return test_summary();
}
