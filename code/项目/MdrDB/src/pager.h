/*
 * pager.h：页管理器（pager）。
 *
 * 上层只认页号，不碰文件。pager 负责三件事：
 *   1. 页缓存：固定数量的页框（frame），按 LRU 换出；
 *   2. 页的分配：数据库文件只增不减，新页总是追加在末尾；
 *   3. 事务：配合回滚日志，让一个事务的修改要么全部生效，要么全部撤销。
 *
 * 使用规则：
 *   pager_get 返回的页被"钉住"（pin），用完必须 pager_unpin；
 *   修改页的内容之前必须先调用 pager_write；
 *   pager_write 和 pager_allocate 只能在 pager_begin 之后调用。
 */
#ifndef MDRDB_PAGER_H
#define MDRDB_PAGER_H

#include "format.h"
#include "mdrdb.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define PAGER_DEFAULT_FRAMES 64

/*
 * B+ 树同时钉住的页不超过 2 个，分配新页时还要临时钉住文件头。
 * 再留一个可以换出的页框，最少需要 4 个。
 */
#define PAGER_MIN_FRAMES 4

typedef struct page {
    uint32_t pgno;
    int pins;           /* 钉住计数，大于 0 时不能换出 */
    bool in_use;        /* 页框里是否装着一页 */
    bool dirty;         /* 内容改过，还没写回文件 */
    uint64_t last_used; /* 最近一次被取用的时刻，用于 LRU */
    uint8_t data[DB_PAGE_SIZE];
} page;

typedef struct pager_stats {
    unsigned long hits;      /* 缓存命中 */
    unsigned long misses;    /* 缓存未命中，从文件读入 */
    unsigned long spills;    /* 事务中途被换出、提前写回文件的脏页 */
    unsigned long journaled; /* 写进回滚日志的页 */
} pager_stats;

typedef struct pager pager;

/* *recovered 返回打开时从热日志恢复的页数。 */
db_status pager_open(const char *path, size_t frames, pager **out,
                     uint32_t *recovered);
void pager_close(pager *p);

db_status pager_get(pager *p, uint32_t pgno, page **out);
void pager_unpin(page *pg);
db_status pager_write(pager *p, page *pg);
db_status pager_allocate(pager *p, page **out);

db_status pager_begin(pager *p);
db_status pager_commit(pager *p);
db_status pager_rollback(pager *p);

bool pager_in_transaction(const pager *p);
uint32_t pager_page_count(const pager *p);
size_t pager_frame_count(const pager *p);
size_t pager_pinned(const pager *p);
pager_stats pager_get_stats(const pager *p);

#endif
