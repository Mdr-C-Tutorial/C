/*
 * test_record.c：varint 与记录编码。
 */
#include "record.h"
#include "test.h"

#include <string.h>

static db_row make_row(uint32_t id, const char *name, const char *email) {
    db_row row;
    memset(&row, 0, sizeof row);
    row.id = id;
    snprintf(row.name, sizeof row.name, "%s", name);
    snprintf(row.email, sizeof row.email, "%s", email);
    return row;
}

static void test_varint_round_trip(void) {
    const uint32_t values[] = {0,       1,         127,       128,
                               300,     16383,     16384,     2097151,
                               2097152, 268435455, 268435456, UINT32_MAX};
    for (size_t i = 0; i < sizeof values / sizeof values[0]; i++) {
        uint8_t buf[VARINT_MAX_SIZE];
        uint32_t back = 0;
        size_t n = varint_encode(values[i], buf);
        CHECK(n >= 1 && n <= VARINT_MAX_SIZE);
        CHECK(varint_decode(buf, n, &back) == n);
        CHECK(back == values[i]);
    }
}

static void test_varint_bytes(void) {
    uint8_t buf[VARINT_MAX_SIZE];
    CHECK(varint_encode(5, buf) == 1 && buf[0] == 0x05);
    /* 300 = 10 0101100（二进制）：先放低 7 位并置最高位，再放高位。 */
    CHECK(varint_encode(300, buf) == 2 && buf[0] == 0xAC && buf[1] == 0x02);
}

static void test_varint_rejects_bad_input(void) {
    uint32_t v;
    const uint8_t truncated[] = {0x80, 0x80};
    CHECK(varint_decode(truncated, sizeof truncated, &v) == 0);
    const uint8_t too_big[] = {0xFF, 0xFF, 0xFF, 0xFF, 0x1F};
    CHECK(varint_decode(too_big, sizeof too_big, &v) == 0);
    const uint8_t max[] = {0xFF, 0xFF, 0xFF, 0xFF, 0x0F};
    CHECK(varint_decode(max, sizeof max, &v) == 5 && v == UINT32_MAX);
    CHECK(varint_decode(max, 0, &v) == 0);
}

static void test_record_layout(void) {
    db_row row = make_row(1, "alice", "a@b.c");
    uint8_t buf[RECORD_MAX_SIZE];
    size_t size;
    CHECK_OK(record_encode(&row, buf, &size));
    const uint8_t expected[] = {5, 'a', 'l', 'i', 'c', 'e',
                                5, 'a', '@', 'b', '.', 'c'};
    CHECK(size == sizeof expected);
    CHECK(memcmp(buf, expected, sizeof expected) == 0);
}

static void test_record_round_trip(void) {
    char name[DB_NAME_MAX + 1];
    char email[DB_EMAIL_MAX + 1];
    memset(name, 'n', DB_NAME_MAX);
    name[DB_NAME_MAX] = '\0';
    memset(email, 'e', DB_EMAIL_MAX);
    email[DB_EMAIL_MAX] = '\0';
    db_row longest = make_row(7, name, email);
    db_row empty = make_row(8, "", "");
    const db_row *rows[] = {&longest, &empty};

    for (size_t i = 0; i < 2; i++) {
        uint8_t buf[RECORD_MAX_SIZE];
        size_t size;
        db_row back;
        CHECK_OK(record_encode(rows[i], buf, &size));
        CHECK_OK(record_decode(buf, size, &back));
        CHECK(strcmp(back.name, rows[i]->name) == 0);
        CHECK(strcmp(back.email, rows[i]->email) == 0);
    }
    /* 最长的一行：1 + 32 + 2 + 255 字节。 */
    uint8_t buf[RECORD_MAX_SIZE];
    size_t size;
    CHECK_OK(record_encode(&longest, buf, &size));
    CHECK(size == 290);
}

static void test_record_rejects_unterminated_field(void) {
    db_row row = make_row(1, "x", "y");
    memset(row.name, 'x', sizeof row.name); /* 数组填满，没有 '\0' */
    uint8_t buf[RECORD_MAX_SIZE];
    size_t size;
    CHECK_STATUS(record_encode(&row, buf, &size), DB_TOO_LONG);
}

static void test_record_rejects_corruption(void) {
    db_row row;
    const uint8_t past_end[] = {10, 'a', 'b'};
    CHECK_STATUS(record_decode(past_end, sizeof past_end, &row), DB_CORRUPT);
    const uint8_t trailing[] = {1, 'a', 1, 'b', 0xEE};
    CHECK_STATUS(record_decode(trailing, sizeof trailing, &row), DB_CORRUPT);
    const uint8_t missing_email[] = {1, 'a'};
    CHECK_STATUS(record_decode(missing_email, sizeof missing_email, &row),
                 DB_CORRUPT);
    const uint8_t inner_nul[] = {2, 'a', 0, 0};
    CHECK_STATUS(record_decode(inner_nul, sizeof inner_nul, &row), DB_CORRUPT);
    uint8_t long_name[1 + 40 + 1];
    long_name[0] = 40; /* 超过 DB_NAME_MAX */
    memset(long_name + 1, 'n', 40);
    long_name[41] = 0;
    CHECK_STATUS(record_decode(long_name, sizeof long_name, &row), DB_CORRUPT);
    const uint8_t ok[] = {1, 'a', 0};
    CHECK_OK(record_decode(ok, sizeof ok, &row));
    CHECK(strcmp(row.name, "a") == 0 && row.email[0] == '\0');
}

int main(void) {
    RUN_TEST(test_varint_round_trip);
    RUN_TEST(test_varint_bytes);
    RUN_TEST(test_varint_rejects_bad_input);
    RUN_TEST(test_record_layout);
    RUN_TEST(test_record_round_trip);
    RUN_TEST(test_record_rejects_unterminated_field);
    RUN_TEST(test_record_rejects_corruption);
    return test_summary();
}
