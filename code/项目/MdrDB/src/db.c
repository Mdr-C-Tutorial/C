/*
 * db.c：查询接口。
 *
 * 这一层把一行数据编码成记录交给 B+ 树，并决定事务从哪里开始、到哪里
 * 结束：用户没有用 db_begin 开启事务时，每条修改语句自成一个事务
 * （自动提交）。
 */
#include "mdrdb.h"

#include "btree.h"
#include "pager.h"
#include "record.h"

#include <inttypes.h>
#include <stdlib.h>
#include <string.h>

struct db {
    pager *pager;
    char *path;
    bool explicit_txn; /* 用户用 db_begin 开启了事务 */
    uint32_t recovered_pages;
};

const char *db_status_message(db_status status) {
    switch (status) {
    case DB_OK:
        return "ok";
    case DB_NOT_FOUND:
        return "not found";
    case DB_DUPLICATE:
        return "duplicate key";
    case DB_TOO_LONG:
        return "value too long";
    case DB_MISUSE:
        return "misuse";
    case DB_CACHE_FULL:
        return "page cache is full";
    case DB_FULL:
        return "database file is full";
    case DB_NO_MEMORY:
        return "out of memory";
    case DB_IO_ERROR:
        return "I/O error";
    case DB_CORRUPT:
        return "database file is corrupt";
    }
    return "unknown status";
}

/*
 * 这几种错误在修改任何页之前就能查出来：插入前先找过键，删除前也先
 * 找过键。语句失败但数据库原封未动，事务可以照常继续。
 */
static bool is_clean_failure(db_status rc) {
    return rc == DB_DUPLICATE || rc == DB_NOT_FOUND || rc == DB_TOO_LONG;
}

/*
 * 结束一条修改语句，rc 是语句本身的结果。其他错误可能发生在修改到
 * 一半的时候（比如分裂途中读页失败），树的状态无法保证，只能把整个
 * 事务回滚。自动提交时提交本身也可能失败，同样回滚：提交点之前日志
 * 一直在磁盘上。
 */
static db_status finish_statement(db *d, bool autocommit, db_status rc) {
    if (rc == DB_OK || is_clean_failure(rc)) {
        if (!autocommit) {
            return rc;
        }
        db_status commit_rc = pager_commit(d->pager);
        if (commit_rc == DB_OK) {
            return rc;
        }
        rc = commit_rc;
    }
    (void)pager_rollback(d->pager);
    d->explicit_txn = false;
    return rc;
}

static char *copy_string(const char *s) {
    size_t len = strlen(s) + 1;
    char *copy = malloc(len);
    if (copy != NULL) {
        memcpy(copy, s, len);
    }
    return copy;
}

db_status db_open(const char *path, size_t cache_pages, db **out) {
    *out = NULL;
    db *d = calloc(1, sizeof *d);
    if (d == NULL) {
        return DB_NO_MEMORY;
    }
    d->path = copy_string(path);
    if (d->path == NULL) {
        free(d);
        return DB_NO_MEMORY;
    }
    size_t frames = cache_pages == 0 ? PAGER_DEFAULT_FRAMES : cache_pages;
    db_status rc = pager_open(path, frames, &d->pager, &d->recovered_pages);
    if (rc == DB_OK && pager_page_count(d->pager) == 1) {
        /* 新文件只有文件头。建根页也走事务：中途崩溃，下次打开会重建。 */
        rc = pager_begin(d->pager);
        if (rc == DB_OK) {
            rc = finish_statement(d, true, btree_create(d->pager));
        }
    }
    if (rc != DB_OK) {
        db_close(d);
        return rc;
    }
    *out = d;
    return DB_OK;
}

void db_close(db *d) {
    if (d == NULL) {
        return;
    }
    pager_close(d->pager); /* 没提交的事务在这里回滚 */
    free(d->path);
    free(d);
}

uint32_t db_recovered_pages(const db *d) { return d->recovered_pages; }

db_status db_insert(db *d, const db_row *row) {
    uint8_t record[RECORD_MAX_SIZE];
    size_t size;
    db_status rc = record_encode(row, record, &size);
    if (rc != DB_OK) {
        return rc;
    }
    bool autocommit = !d->explicit_txn;
    if (autocommit) {
        rc = pager_begin(d->pager);
        if (rc != DB_OK) {
            return rc;
        }
    }
    rc = btree_insert(d->pager, row->id, record, size);
    return finish_statement(d, autocommit, rc);
}

db_status db_delete(db *d, uint32_t id) {
    bool autocommit = !d->explicit_txn;
    if (autocommit) {
        db_status rc = pager_begin(d->pager);
        if (rc != DB_OK) {
            return rc;
        }
    }
    return finish_statement(d, autocommit, btree_delete(d->pager, id));
}

db_status db_find(db *d, uint32_t id, db_row *row) {
    uint8_t payload[BTREE_MAX_PAYLOAD];
    size_t size;
    db_status rc = btree_find(d->pager, id, payload, &size);
    if (rc != DB_OK) {
        return rc;
    }
    row->id = id;
    return record_decode(payload, size, row);
}

db_status db_scan(db *d, uint32_t first, uint32_t last, db_row_fn fn,
                  void *ctx) {
    btree_cursor c;
    db_status rc = btree_seek(d->pager, first, &c);
    while (rc == DB_OK && c.valid) {
        uint8_t payload[BTREE_MAX_PAYLOAD];
        size_t size;
        db_row row;
        rc = btree_read(&c, &row.id, payload, &size);
        if (rc != DB_OK || row.id > last) {
            break;
        }
        rc = record_decode(payload, size, &row);
        if (rc != DB_OK || !fn(&row, ctx)) {
            break;
        }
        rc = btree_next(&c);
    }
    return rc;
}

db_status db_begin(db *d) {
    if (d->explicit_txn) {
        return DB_MISUSE;
    }
    db_status rc = pager_begin(d->pager);
    if (rc == DB_OK) {
        d->explicit_txn = true;
    }
    return rc;
}

db_status db_commit(db *d) {
    if (!d->explicit_txn) {
        return DB_MISUSE;
    }
    d->explicit_txn = false;
    db_status rc = pager_commit(d->pager);
    if (rc != DB_OK) {
        (void)pager_rollback(d->pager);
    }
    return rc;
}

db_status db_rollback(db *d) {
    if (!d->explicit_txn) {
        return DB_MISUSE;
    }
    d->explicit_txn = false;
    return pager_rollback(d->pager);
}

bool db_in_transaction(const db *d) { return d->explicit_txn; }

db_status db_check(db *d, db_check_result *result) {
    memset(result, 0, sizeof *result);
    btree_stats stats;
    db_status rc =
        btree_check(d->pager, &stats, result->message, sizeof result->message);
    if (rc != DB_OK) {
        if (result->message[0] == '\0') {
            snprintf(result->message, sizeof result->message, "%s",
                     db_status_message(rc));
        }
        return rc;
    }
    result->rows = stats.keys;
    result->leaf_pages = stats.leaves;
    result->internal_pages = stats.internals;
    result->height = stats.height;

    /* 树的结构没问题，再逐行确认记录都能解码。 */
    btree_cursor c;
    rc = btree_seek(d->pager, 0, &c);
    while (rc == DB_OK && c.valid) {
        uint8_t payload[BTREE_MAX_PAYLOAD];
        size_t size;
        db_row row;
        rc = btree_read(&c, &row.id, payload, &size);
        if (rc == DB_OK && record_decode(payload, size, &row) != DB_OK) {
            snprintf(result->message, sizeof result->message,
                     "row %" PRIu32 ": record cannot be decoded", row.id);
            return DB_CORRUPT;
        }
        if (rc == DB_OK) {
            rc = btree_next(&c);
        }
    }
    if (rc != DB_OK) {
        snprintf(result->message, sizeof result->message, "%s",
                 db_status_message(rc));
    }
    return rc;
}

db_status db_print_tree(db *d, FILE *out) { return btree_print(d->pager, out); }

db_status db_print_page(db *d, uint32_t pgno, FILE *out) {
    if (pgno >= pager_page_count(d->pager)) {
        return DB_NOT_FOUND;
    }
    if (pgno == 0) {
        fprintf(out,
                "page 0: file header\n  page size %d, page count %" PRIu32 "\n",
                DB_PAGE_SIZE, pager_page_count(d->pager));
        return DB_OK;
    }
    page *pg;
    db_status rc = pager_get(d->pager, pgno, &pg);
    if (rc != DB_OK) {
        return rc;
    }
    if (node_validate(pg->data)) {
        node_print(pg->data, pgno, out);
    } else {
        rc = DB_CORRUPT;
    }
    pager_unpin(pg);
    return rc;
}

void db_print_info(db *d, FILE *out) {
    pager_stats st = pager_get_stats(d->pager);
    fprintf(out, "file: %s\n", d->path);
    fprintf(out, "page size: %d, page count: %" PRIu32 "\n", DB_PAGE_SIZE,
            pager_page_count(d->pager));
    fprintf(out, "cache: %lu frames, hits %lu, misses %lu, spills %lu\n",
            (unsigned long)pager_frame_count(d->pager), st.hits, st.misses,
            st.spills);
    fprintf(out,
            "journal: pages saved %lu, pages restored at open %" PRIu32 "\n",
            st.journaled, d->recovered_pages);
    fprintf(out, "transaction: %s\n", d->explicit_txn ? "open" : "none");
}
