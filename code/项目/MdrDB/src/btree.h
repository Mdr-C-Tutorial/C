/*
 * btree.h：B+ 树索引。
 *
 * 键是 32 位无符号整数，负载是不超过 BTREE_MAX_PAYLOAD 字节的任意数据。
 * 所有负载都存在叶子里，叶子按键的顺序用 link 串成链表；内部节点只存
 * 分隔键和孩子页号，负责把查找引向正确的叶子。
 *
 * 根固定在第 1 页。整个数据库只有这一棵树，所以函数直接接收 pager。
 * 修改树的函数要在 pager_begin 之后调用。
 */
#ifndef MDRDB_BTREE_H
#define MDRDB_BTREE_H

#include "mdrdb.h"
#include "node.h"
#include "pager.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#define BTREE_ROOT 1
#define BTREE_MAX_DEPTH 16

/*
 * 负载上限：一个单元格（连同指针）不超过页内可用空间的 1/4。这样
 * 一个叶子至少放得下 4 个单元格，分裂后的两半也一定都装得下。
 */
#define BTREE_MAX_PAYLOAD                                                      \
    ((DB_PAGE_SIZE - NODE_HEADER_SIZE) / 4 - CELL_POINTER_SIZE -               \
     LEAF_CELL_HEADER_SIZE)

/*
 * 游标：指向某个叶子里的某个单元格。游标不钉住页面，每一步都重新取页，
 * 所以遍历期间不能修改树。
 */
typedef struct btree_cursor {
    pager *pager;
    uint32_t leaf;
    int index;
    uint32_t key; /* 当前单元格的键，用来确认每走一步键都变大 */
    bool valid;   /* false 表示已经走过最后一个键 */
} btree_cursor;

typedef struct btree_stats {
    uint32_t keys;
    uint32_t leaves;
    uint32_t internals;
    uint32_t height;
} btree_stats;

db_status btree_create(pager *p);

/* payload 必须能容纳 BTREE_MAX_PAYLOAD 字节。 */
db_status btree_find(pager *p, uint32_t key, uint8_t *payload, size_t *size);
db_status btree_insert(pager *p, uint32_t key, const uint8_t *payload,
                       size_t size);
db_status btree_delete(pager *p, uint32_t key);

/* 把游标定位到第一个不小于 key 的键。 */
db_status btree_seek(pager *p, uint32_t key, btree_cursor *c);
db_status btree_next(btree_cursor *c);
db_status btree_read(const btree_cursor *c, uint32_t *key, uint8_t *payload,
                     size_t *size);

/* 检查树的全部不变式；失败时 message 说明原因。 */
db_status btree_check(pager *p, btree_stats *stats, char *message,
                      size_t message_size);
db_status btree_print(pager *p, FILE *out);

#endif
