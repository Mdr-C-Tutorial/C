/*
 * test_node.c：节点页面的布局。只操作内存里的一页，不涉及文件。
 */
#include "node.h"
#include "test.h"

#include <string.h>

static void insert_leaf(uint8_t *node, uint32_t key, const char *payload) {
    uint8_t cell[LEAF_CELL_HEADER_SIZE + 128];
    size_t size =
        leaf_cell_build(cell, key, (const uint8_t *)payload, strlen(payload));
    bool found;
    int i = node_search(node, key, &found);
    CHECK(!found);
    CHECK(node_has_room(node, size));
    node_insert_cell(node, i, cell, size);
}

static bool payload_is(const uint8_t *node, int i, const char *expected) {
    size_t size;
    const uint8_t *p = leaf_payload(node, i, &size);
    return size == strlen(expected) && memcmp(p, expected, size) == 0;
}

static void test_empty_leaf(void) {
    uint8_t node[DB_PAGE_SIZE];
    node_init(node, NODE_LEAF);
    CHECK(node_is_leaf(node));
    CHECK(node_cell_count(node) == 0);
    CHECK(node_link(node) == 0);
    CHECK(node_free_space(node) == DB_PAGE_SIZE - NODE_HEADER_SIZE);
    CHECK(node_validate(node));
}

static void test_cells_sorted_by_pointer_not_by_address(void) {
    uint8_t node[DB_PAGE_SIZE];
    node_init(node, NODE_LEAF);
    insert_leaf(node, 30, "ccc");
    insert_leaf(node, 10, "a");
    insert_leaf(node, 20, "bb");
    CHECK(node_cell_count(node) == 3);
    CHECK(node_key(node, 0) == 10);
    CHECK(node_key(node, 1) == 20);
    CHECK(node_key(node, 2) == 30);
    CHECK(payload_is(node, 0, "a") && payload_is(node, 2, "ccc"));
    /* 先插入的 30 紧贴页尾，后插入的单元格依次往前放。 */
    CHECK(node_cell(node, 2) == node + DB_PAGE_SIZE - 9);
    CHECK(node_cell(node, 0) == node + DB_PAGE_SIZE - 9 - 7);
    CHECK(node_validate(node));
}

static void test_search(void) {
    uint8_t node[DB_PAGE_SIZE];
    node_init(node, NODE_LEAF);
    insert_leaf(node, 10, "a");
    insert_leaf(node, 20, "b");
    bool found;
    CHECK(node_search(node, 5, &found) == 0 && !found);
    CHECK(node_search(node, 10, &found) == 0 && found);
    CHECK(node_search(node, 15, &found) == 1 && !found);
    CHECK(node_search(node, 20, &found) == 1 && found);
    CHECK(node_search(node, 99, &found) == 2 && !found);
}

static void test_remove_compacts_content(void) {
    uint8_t node[DB_PAGE_SIZE];
    node_init(node, NODE_LEAF);
    insert_leaf(node, 10, "aaaa");
    insert_leaf(node, 20, "bb");
    insert_leaf(node, 30, "cccccc");
    size_t before = node_free_space(node);
    node_remove_cell(node, 1);
    CHECK(node_cell_count(node) == 2);
    CHECK(node_key(node, 0) == 10 && node_key(node, 1) == 30);
    CHECK(payload_is(node, 0, "aaaa") && payload_is(node, 1, "cccccc"));
    /* 收回单元格本身的 8 字节和一个 2 字节的指针。 */
    CHECK(node_free_space(node) == before + LEAF_CELL_HEADER_SIZE + 2 + 2);
    CHECK(node_validate(node)); /* 内容区仍然紧凑 */
    node_remove_cell(node, 0);
    node_remove_cell(node, 0);
    CHECK(node_cell_count(node) == 0);
    CHECK(node_free_space(node) == DB_PAGE_SIZE - NODE_HEADER_SIZE);
}

static void test_fill_until_full(void) {
    uint8_t node[DB_PAGE_SIZE];
    char payload[101];
    memset(payload, 'p', 100);
    payload[100] = '\0';
    node_init(node, NODE_LEAF);
    uint32_t key = 0;
    uint8_t cell[LEAF_CELL_HEADER_SIZE + 100];
    size_t size = leaf_cell_build(cell, 0, (const uint8_t *)payload, 100);
    while (node_has_room(node, size)) {
        insert_leaf(node, key++, payload);
    }
    /* 每个单元格 106 字节，加指针 108 字节：(4096 - 12) / 108 = 37。 */
    CHECK(node_cell_count(node) == 37);
    CHECK(node_validate(node));
}

static void test_internal_children(void) {
    uint8_t node[DB_PAGE_SIZE];
    uint8_t cell[INTERNAL_CELL_SIZE];
    node_init(node, NODE_INTERNAL);
    node_set_link(node, 9);
    internal_cell_build(cell, 10, 2);
    node_insert_cell(node, 0, cell, sizeof cell);
    internal_cell_build(cell, 20, 3);
    node_insert_cell(node, 1, cell, sizeof cell);
    CHECK(!node_is_leaf(node));
    CHECK(internal_child(node, 0) == 2);
    CHECK(internal_child(node, 1) == 3);
    CHECK(internal_child(node, 2) == 9); /* 最右孩子 */
    internal_set_child(node, 2, 7);
    CHECK(node_link(node) == 7);
    internal_set_child(node, 0, 5);
    CHECK(internal_child(node, 0) == 5 && node_key(node, 0) == 10);
    CHECK(node_validate(node));
}

static void test_validate_rejects_garbage(void) {
    uint8_t node[DB_PAGE_SIZE];

    node_init(node, NODE_LEAF);
    node[0] = 7; /* 未知的节点类型 */
    CHECK(!node_validate(node));

    node_init(node, NODE_LEAF);
    put_u16(node + 2, 3000); /* 指针数组比整页还长 */
    CHECK(!node_validate(node));

    node_init(node, NODE_LEAF);
    insert_leaf(node, 1, "abc");
    put_u16(node + NODE_HEADER_SIZE, DB_PAGE_SIZE - 2); /* 单元格越过页尾 */
    CHECK(!node_validate(node));

    node_init(node, NODE_LEAF);
    insert_leaf(node, 1, "abc");
    put_u16(node + DB_PAGE_SIZE - 9 + 4, 500); /* 负载长度不对 */
    CHECK(!node_validate(node));
}

int main(void) {
    RUN_TEST(test_empty_leaf);
    RUN_TEST(test_cells_sorted_by_pointer_not_by_address);
    RUN_TEST(test_search);
    RUN_TEST(test_remove_compacts_content);
    RUN_TEST(test_fill_until_full);
    RUN_TEST(test_internal_children);
    RUN_TEST(test_validate_rejects_garbage);
    return test_summary();
}
