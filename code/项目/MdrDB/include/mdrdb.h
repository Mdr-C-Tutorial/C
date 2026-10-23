/*
 * mdrdb.h：MdrDB 的公共接口。
 *
 * MdrDB 是一个教学用的单文件数据库：整个数据库存放在一个文件里，
 * 只有一张模式固定的表 (id, name, email)，id 是主键。
 * 命令行程序和测试都只通过这个头文件使用数据库。
 */
#ifndef MDRDB_H
#define MDRDB_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#define DB_NAME_MAX 32   /* name 最多 32 字节 */
#define DB_EMAIL_MAX 255 /* email 最多 255 字节 */

typedef struct db_row {
    uint32_t id;
    char name[DB_NAME_MAX + 1];
    char email[DB_EMAIL_MAX + 1];
} db_row;

/* 所有可能失败的函数都返回 db_status。 */
typedef enum db_status {
    DB_OK = 0,
    DB_NOT_FOUND,  /* 键不存在 */
    DB_DUPLICATE,  /* 键已存在 */
    DB_TOO_LONG,   /* 字段或负载超过长度上限 */
    DB_MISUSE,     /* 调用顺序不对，例如没有 begin 就 commit */
    DB_CACHE_FULL, /* 页缓存里的页都被钉住，腾不出页框 */
    DB_FULL,       /* 数据库文件达到大小上限 */
    DB_NO_MEMORY,
    DB_IO_ERROR,
    DB_CORRUPT /* 文件内容不符合格式 */
} db_status;

typedef struct db db;

/* db_scan 的回调。返回 false 提前结束遍历。回调里不能修改数据库。 */
typedef bool (*db_row_fn)(const db_row *row, void *ctx);

typedef struct db_check_result {
    uint32_t rows;
    uint32_t leaf_pages;
    uint32_t internal_pages;
    uint32_t height;
    char message[128]; /* 检查不通过时，说明发现了什么问题 */
} db_check_result;

const char *db_status_message(db_status status);

/*
 * 打开数据库文件，文件不存在就新建。cache_pages 是页缓存的页框数，
 * 0 表示使用默认值。上次有事务没提交就中断时，打开前先用回滚日志恢复。
 */
db_status db_open(const char *path, size_t cache_pages, db **out);
void db_close(db *d);
uint32_t db_recovered_pages(const db *d);

/* 没有用 db_begin 开启事务时，每次修改自成一个事务，返回前已经提交。 */
db_status db_insert(db *d, const db_row *row);
db_status db_delete(db *d, uint32_t id);
db_status db_find(db *d, uint32_t id, db_row *row);

/* 按 id 升序访问 first <= id <= last 的每一行。 */
db_status db_scan(db *d, uint32_t first, uint32_t last, db_row_fn fn,
                  void *ctx);

db_status db_begin(db *d);
db_status db_commit(db *d);
db_status db_rollback(db *d);
bool db_in_transaction(const db *d);

/* 观察数据库内部：一致性检查、打印树的形状、打印一页、打印统计信息。 */
db_status db_check(db *d, db_check_result *result);
db_status db_print_tree(db *d, FILE *out);
db_status db_print_page(db *d, uint32_t pgno, FILE *out);
void db_print_info(db *d, FILE *out);

#endif
