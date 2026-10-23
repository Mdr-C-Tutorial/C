/*
 * test_btree.c：B+ 树的插入、分裂、查找、删除和遍历。
 *
 * 负载取 200～999 字节，一个叶子只放得下几个单元格，几千个键就能让
 * 树长到三层，内部节点的分裂也会被覆盖到。
 */
#include "btree.h"
#include "test.h"

#include <stdlib.h>
#include <string.h>

#define BTREE_DB "test_btree.db"
#define MANY_KEYS 3000

/* 每个键的负载由键算出，读回时可以逐字节核对。 */
static size_t make_payload(uint32_t key, uint8_t *out) {
    size_t size = 200 + (key * 7919u) % 800;
    for (size_t i = 0; i < size; i++) {
        out[i] = (uint8_t)(key * 31u + i);
    }
    return size;
}

static pager *open_tree(size_t frames) {
    remove_db_files(BTREE_DB);
    pager *p = NULL;
    uint32_t recovered;
    CHECK_OK(pager_open(BTREE_DB, frames, &p, &recovered));
    CHECK_OK(pager_begin(p));
    CHECK_OK(btree_create(p));
    CHECK_OK(pager_commit(p));
    return p;
}

static uint32_t check_tree(pager *p, uint32_t expected_keys) {
    btree_stats st;
    char message[128];
    db_status rc = btree_check(p, &st, message, sizeof message);
    CHECK(rc == DB_OK);
    if (rc != DB_OK) {
        fprintf(stderr, "  btree_check: %s\n", message);
        return 0;
    }
    CHECK(st.keys == expected_keys);
    CHECK(pager_pinned(p) == 0); /* 没有忘记 unpin 的页 */
    return st.height;
}

static void insert_keys(pager *p, const uint32_t *keys, size_t n) {
    CHECK_OK(pager_begin(p));
    for (size_t i = 0; i < n; i++) {
        uint8_t payload[BTREE_MAX_PAYLOAD];
        size_t size = make_payload(keys[i], payload);
        CHECK_OK(btree_insert(p, keys[i], payload, size));
    }
    CHECK_OK(pager_commit(p));
}

static void check_find(pager *p, uint32_t key, bool present) {
    uint8_t got[BTREE_MAX_PAYLOAD];
    uint8_t expected[BTREE_MAX_PAYLOAD];
    size_t size = 0;
    db_status rc = btree_find(p, key, got, &size);
    if (!present) {
        CHECK(rc == DB_NOT_FOUND);
        return;
    }
    CHECK(rc == DB_OK);
    size_t expected_size = make_payload(key, expected);
    CHECK(rc != DB_OK ||
          (size == expected_size && memcmp(got, expected, size) == 0));
}

/* 用线性同余生成器做 Fisher-Yates 洗牌：结果固定，失败可以复现。 */
static void shuffle(uint32_t *keys, size_t n, uint32_t seed) {
    for (size_t i = n - 1; i > 0; i--) {
        seed = seed * 1103515245u + 12345u;
        size_t j = (seed >> 8) % (i + 1);
        uint32_t t = keys[i];
        keys[i] = keys[j];
        keys[j] = t;
    }
}

static uint32_t *key_range(size_t n, uint32_t step) {
    uint32_t *keys = malloc(n * sizeof *keys);
    CHECK(keys != NULL);
    for (size_t i = 0; keys != NULL && i < n; i++) {
        keys[i] = (uint32_t)(i + 1) * step;
    }
    return keys;
}

static void test_root_grows(void) {
    pager *p = open_tree(16);
    uint32_t keys[8];
    for (uint32_t i = 0; i < 8; i++) {
        keys[i] = i + 1;
    }
    insert_keys(p, keys, 3);
    CHECK(check_tree(p, 3) == 1); /* 只有根这一个叶子 */
    insert_keys(p, keys + 3, 5);
    CHECK(check_tree(p, 8) == 2); /* 根分裂，树长了一层 */
    page *root;
    CHECK_OK(pager_get(p, BTREE_ROOT, &root));
    CHECK(!node_is_leaf(root->data)); /* 根仍在第 1 页，变成了内部节点 */
    pager_unpin(root);
    for (uint32_t k = 1; k <= 8; k++) {
        check_find(p, k, true);
    }
    pager_close(p);
}

static void test_ascending_and_descending(void) {
    uint32_t *keys = key_range(MANY_KEYS, 1);
    if (keys == NULL) {
        return;
    }
    pager *p = open_tree(64);
    insert_keys(p, keys, MANY_KEYS);
    CHECK(check_tree(p, MANY_KEYS) >= 3);
    check_find(p, 0, false);
    check_find(p, 1, true);
    check_find(p, MANY_KEYS, true);
    check_find(p, MANY_KEYS + 1, false);
    pager_close(p);

    for (size_t i = 0; i < MANY_KEYS / 2; i++) {
        uint32_t t = keys[i];
        keys[i] = keys[MANY_KEYS - 1 - i];
        keys[MANY_KEYS - 1 - i] = t;
    }
    p = open_tree(64);
    insert_keys(p, keys, MANY_KEYS);
    CHECK(check_tree(p, MANY_KEYS) >= 3);
    for (uint32_t k = 1; k <= MANY_KEYS; k += 97) {
        check_find(p, k, true);
    }
    pager_close(p);
    free(keys);
}

static void test_random_insert_and_delete(void) {
    /* 键取 3 的倍数，中间留出空隙，用来检查不存在的键。 */
    uint32_t *keys = key_range(MANY_KEYS, 3);
    if (keys == NULL) {
        return;
    }
    shuffle(keys, MANY_KEYS, 2024);
    pager *p = open_tree(64);
    insert_keys(p, keys, MANY_KEYS);
    CHECK(check_tree(p, MANY_KEYS) >= 3);
    for (uint32_t k = 1; k <= 3 * MANY_KEYS; k++) {
        check_find(p, k, k % 3 == 0);
    }

    /* 删掉洗牌后的前一半，再逐个核对。 */
    CHECK_OK(pager_begin(p));
    for (size_t i = 0; i < MANY_KEYS / 2; i++) {
        CHECK_OK(btree_delete(p, keys[i]));
    }
    CHECK_STATUS(btree_delete(p, keys[0]), DB_NOT_FOUND);
    CHECK_OK(pager_commit(p));
    check_tree(p, MANY_KEYS - MANY_KEYS / 2);
    for (size_t i = 0; i < MANY_KEYS; i++) {
        check_find(p, keys[i], i >= MANY_KEYS / 2);
    }

    /* 全部删光后，叶子都空了，树依然可以继续使用。 */
    CHECK_OK(pager_begin(p));
    for (size_t i = MANY_KEYS / 2; i < MANY_KEYS; i++) {
        CHECK_OK(btree_delete(p, keys[i]));
    }
    CHECK_OK(pager_commit(p));
    check_tree(p, 0);
    insert_keys(p, keys, 100);
    check_tree(p, 100);
    for (size_t i = 0; i < 100; i++) {
        check_find(p, keys[i], true);
    }
    pager_close(p);
    free(keys);
}

static void test_duplicate_and_oversized(void) {
    pager *p = open_tree(16);
    uint8_t payload[BTREE_MAX_PAYLOAD + 1];
    memset(payload, 'x', sizeof payload);
    CHECK_OK(pager_begin(p));
    CHECK_OK(btree_insert(p, 7, payload, 10));
    CHECK_STATUS(btree_insert(p, 7, payload, 20), DB_DUPLICATE);
    CHECK_OK(btree_insert(p, 8, payload, BTREE_MAX_PAYLOAD));
    CHECK_STATUS(btree_insert(p, 9, payload, BTREE_MAX_PAYLOAD + 1),
                 DB_TOO_LONG);
    CHECK_OK(pager_commit(p));
    uint8_t got[BTREE_MAX_PAYLOAD];
    size_t size;
    CHECK_OK(btree_find(p, 7, got, &size));
    CHECK(size == 10); /* 重复插入没有覆盖原值 */
    check_tree(p, 2);
    pager_close(p);
}

static void test_cursor_scan(void) {
    uint32_t *keys = key_range(500, 2); /* 2, 4, ..., 1000 */
    if (keys == NULL) {
        return;
    }
    shuffle(keys, 500, 7);
    pager *p = open_tree(16);
    insert_keys(p, keys, 500);

    btree_cursor c;
    uint32_t expected = 2;
    size_t seen = 0;
    CHECK_OK(btree_seek(p, 0, &c));
    while (c.valid) {
        uint32_t key;
        uint8_t payload[BTREE_MAX_PAYLOAD];
        size_t size;
        CHECK_OK(btree_read(&c, &key, payload, &size));
        CHECK(key == expected);
        expected += 2;
        seen++;
        CHECK_OK(btree_next(&c));
    }
    CHECK(seen == 500);

    /* 定位到第一个不小于 501 的键，也就是 502。 */
    uint32_t key;
    uint8_t payload[BTREE_MAX_PAYLOAD];
    size_t size;
    CHECK_OK(btree_seek(p, 501, &c));
    CHECK(c.valid);
    CHECK_OK(btree_read(&c, &key, payload, &size));
    CHECK(key == 502);
    CHECK_OK(btree_seek(p, 1001, &c));
    CHECK(!c.valid);
    pager_close(p);
    free(keys);
}

/* 页框只有 4 个：分裂期间页被不断换出、读回，结果仍然正确。 */
static void test_tiny_cache(void) {
    uint32_t *keys = key_range(1000, 1);
    if (keys == NULL) {
        return;
    }
    shuffle(keys, 1000, 99);
    pager *p = open_tree(PAGER_MIN_FRAMES);
    insert_keys(p, keys, 1000);
    CHECK(pager_get_stats(p).spills > 0);
    check_tree(p, 1000);
    CHECK_OK(pager_begin(p));
    for (size_t i = 0; i < 1000; i += 2) {
        CHECK_OK(btree_delete(p, keys[i]));
    }
    CHECK_OK(pager_commit(p));
    check_tree(p, 500);
    for (size_t i = 0; i < 1000; i++) {
        check_find(p, keys[i], i % 2 == 1);
    }
    pager_close(p);
    free(keys);
}

/* 以下两个测试模拟损坏的文件：直接改写页面，绕过 B+ 树的插入逻辑。 */

/* 把根页换成 node，再提交。 */
static void replace_root(pager *p, const uint8_t *node) {
    page *root;
    CHECK_OK(pager_begin(p));
    CHECK_OK(pager_get(p, BTREE_ROOT, &root));
    CHECK_OK(pager_write(p, root));
    memcpy(root->data, node, DB_PAGE_SIZE);
    pager_unpin(root);
    CHECK_OK(pager_commit(p));
}

/*
 * 单元格的负载超过 BTREE_MAX_PAYLOAD，页内布局却完全正确。插入键 2
 * 时叶子要分裂，若放过这样的页，分裂出的一半装不下，会写到页外。
 */
static void test_rejects_oversized_payload(void) {
    static uint8_t bytes[3074];
    uint8_t cell[LEAF_CELL_HEADER_SIZE + sizeof bytes];
    uint8_t node[DB_PAGE_SIZE];
    memset(bytes, 'x', sizeof bytes);
    node_init(node, NODE_LEAF);
    size_t size = leaf_cell_build(cell, 3, bytes, sizeof bytes);
    node_insert_cell(node, 0, cell, size);
    size = leaf_cell_build(cell, 1, bytes, 994);
    node_insert_cell(node, 0, cell, size);
    CHECK(node_validate(node) && node_free_space(node) == 0);

    pager *p = open_tree(16);
    replace_root(p, node);
    uint8_t payload[BTREE_MAX_PAYLOAD];
    memset(payload, 'y', sizeof payload);
    CHECK_OK(pager_begin(p));
    CHECK_STATUS(btree_insert(p, 2, payload, 260), DB_CORRUPT);
    CHECK_OK(pager_rollback(p));
    size_t got;
    CHECK_STATUS(btree_find(p, 1, payload, &got), DB_CORRUPT);

    btree_stats st;
    char message[128];
    CHECK_STATUS(btree_check(p, &st, message, sizeof message), DB_CORRUPT);
    CHECK(strcmp(message, "page 1: payload is too long") == 0);
    CHECK(pager_pinned(p) == 0);
    pager_close(p);
}

/* 最后一个叶子指回第一个叶子：遍历必须报告损坏，不能绕着环走下去。 */
static void test_scan_detects_leaf_cycle(void) {
    uint32_t *keys = key_range(40, 1);
    if (keys == NULL) {
        return;
    }
    pager *p = open_tree(16);
    insert_keys(p, keys, 40);
    btree_cursor c;
    CHECK_OK(btree_seek(p, 0, &c));
    uint32_t first_leaf = c.leaf;
    CHECK_OK(btree_seek(p, 40, &c));
    uint32_t last_leaf = c.leaf;
    CHECK(first_leaf != last_leaf);

    page *pg;
    CHECK_OK(pager_begin(p));
    CHECK_OK(pager_get(p, last_leaf, &pg));
    CHECK_OK(pager_write(p, pg));
    node_set_link(pg->data, first_leaf);
    pager_unpin(pg);
    CHECK_OK(pager_commit(p));

    int steps = 0;
    db_status rc = btree_seek(p, 0, &c);
    while (rc == DB_OK && c.valid && steps < 1000) {
        rc = btree_next(&c);
        steps++;
    }
    CHECK(rc == DB_CORRUPT);
    CHECK(steps == 40); /* 走完 40 个键，回到键 1 时发现 */
    btree_stats st;
    char message[128];
    CHECK_STATUS(btree_check(p, &st, message, sizeof message), DB_CORRUPT);
    pager_close(p);
    free(keys);
}

int main(void) {
    RUN_TEST(test_root_grows);
    RUN_TEST(test_ascending_and_descending);
    RUN_TEST(test_random_insert_and_delete);
    RUN_TEST(test_duplicate_and_oversized);
    RUN_TEST(test_cursor_scan);
    RUN_TEST(test_tiny_cache);
    RUN_TEST(test_rejects_oversized_payload);
    RUN_TEST(test_scan_detects_leaf_cycle);
    remove_db_files(BTREE_DB);
    return test_summary();
}
