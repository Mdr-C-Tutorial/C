/*
 * node.h：B+ 树节点在页面里的布局。每个节点占一整页：
 *
 *   偏移 0，1 字节：节点类型，1 = 叶子，2 = 内部节点
 *   偏移 1，1 字节：保留，写 0
 *   偏移 2，2 字节：单元格数量
 *   偏移 4，2 字节：单元格内容区的起点
 *   偏移 6，2 字节：保留，写 0
 *   偏移 8，4 字节：叶子：右边相邻叶子的页号（0 表示没有）；
 *                   内部节点：最右孩子的页号
 *   偏移 12 起：单元格指针数组，每项 2 字节，按键升序排列
 *
 * 单元格（cell）从页尾往前存放，指针数组从页头往后增长，两者之间是
 * 空闲空间。单元格在页里的物理位置是任意的，顺序只由指针数组决定。
 *
 *   叶子单元格：键（4）、负载长度（2）、负载
 *   内部单元格：键（4）、孩子页号（4）
 *
 * 两种单元格都以键开头。这个模块只处理一页之内的事，不碰 pager。
 */
#ifndef MDRDB_NODE_H
#define MDRDB_NODE_H

#include "format.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#define NODE_HEADER_SIZE 12
#define CELL_POINTER_SIZE 2
#define LEAF_CELL_HEADER_SIZE 6
#define INTERNAL_CELL_SIZE 8

/* 一页最多能放的单元格数（按负载为空的叶子单元格计）。 */
#define NODE_MAX_CELLS                                                         \
    ((DB_PAGE_SIZE - NODE_HEADER_SIZE) /                                       \
     (CELL_POINTER_SIZE + LEAF_CELL_HEADER_SIZE))

typedef enum node_kind { NODE_LEAF = 1, NODE_INTERNAL = 2 } node_kind;

void node_init(uint8_t *node, node_kind kind);
bool node_validate(const uint8_t *node);

bool node_is_leaf(const uint8_t *node);
int node_cell_count(const uint8_t *node);
uint32_t node_link(const uint8_t *node);
void node_set_link(uint8_t *node, uint32_t pgno);
size_t node_free_space(const uint8_t *node);
bool node_has_room(const uint8_t *node, size_t cell_size);

const uint8_t *node_cell(const uint8_t *node, int i);
size_t node_cell_size(const uint8_t *node, int i);
uint32_t node_key(const uint8_t *node, int i);

/* 二分查找第一个键不小于 key 的位置；*found 表示该位置的键恰好等于 key。 */
int node_search(const uint8_t *node, uint32_t key, bool *found);

/* 把单元格插到第 i 个位置。调用前必须用 node_has_room 确认放得下。 */
void node_insert_cell(uint8_t *node, int i, const uint8_t *cell, size_t size);
void node_remove_cell(uint8_t *node, int i);

size_t leaf_cell_build(uint8_t *cell, uint32_t key, const uint8_t *payload,
                       size_t size);
const uint8_t *leaf_payload(const uint8_t *node, int i, size_t *size);

void internal_cell_build(uint8_t *cell, uint32_t key, uint32_t child);
/* 第 i 个孩子；i 等于单元格数量时是最右孩子。 */
uint32_t internal_child(const uint8_t *node, int i);
void internal_set_child(uint8_t *node, int i, uint32_t child);

void node_print(const uint8_t *node, uint32_t pgno, FILE *out);

#endif
