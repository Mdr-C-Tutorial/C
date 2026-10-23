/*
 * test_crash.c：模拟崩溃，检查恢复。
 *
 * 先不注入故障跑一遍工作负载，数出它一共有 N 次写操作。然后对每个
 * k = 0, 1, ..., N 从头再来一次：建好初始数据库，让第 k + 1 次写操作
 * 崩溃，再重新打开数据库。恢复之后，数据库必须通过一致性检查，内容
 * 恰好等于崩溃前最后一次成功提交后的状态。
 */
#include "file.h"
#include "test.h"

#include <inttypes.h>
#include <string.h>

#define CRASH_DB "test_crash.db"
#define CACHE_FRAMES 4 /* 页框很少，事务中途就要换出脏页 */
#define MODEL_KEYS 128
#define INITIAL_LAST 99 /* 初始数据：奇数 id 1, 3, ..., 99，共 50 行 */

typedef enum op_kind {
    OP_INSERT,
    OP_DELETE,
    OP_BEGIN,
    OP_COMMIT,
    OP_ROLLBACK
} op_kind;

typedef struct op {
    op_kind kind;
    uint32_t id;
} op;

/*
 * 偶数 id 插进已有的叶子，引起分裂；初始数据所在的页都要先进日志。
 * 按事务分组排列，不交给 clang-format 重排。
 */
/* clang-format off */
static const op workload[] = {
    /* 自动提交的插入和删除 */
    {OP_INSERT, 2},   {OP_INSERT, 4},   {OP_INSERT, 6},   {OP_DELETE, 9},
    {OP_INSERT, 100}, {OP_INSERT, 102},
    /* 跨多个叶子的显式事务，最后提交 */
    {OP_BEGIN, 0},    {OP_INSERT, 10},  {OP_INSERT, 12},  {OP_INSERT, 14},
    {OP_INSERT, 16},  {OP_INSERT, 18},  {OP_INSERT, 20},  {OP_INSERT, 22},
    {OP_INSERT, 24},  {OP_INSERT, 26},  {OP_INSERT, 28},  {OP_INSERT, 30},
    {OP_INSERT, 32},  {OP_INSERT, 34},  {OP_INSERT, 36},  {OP_INSERT, 38},
    {OP_INSERT, 40},  {OP_INSERT, 42},  {OP_INSERT, 44},  {OP_INSERT, 46},
    {OP_INSERT, 48},  {OP_DELETE, 51},  {OP_DELETE, 53},  {OP_COMMIT, 0},
    /* 被回滚的显式事务 */
    {OP_BEGIN, 0},    {OP_INSERT, 60},  {OP_DELETE, 61},  {OP_INSERT, 62},
    {OP_ROLLBACK, 0},
    /* 回滚之后，自动提交照常工作 */
    {OP_DELETE, 1},   {OP_INSERT, 104},
};
/* clang-format on */

#define WORKLOAD_SIZE (sizeof workload / sizeof workload[0])

/* 模型：哪些 id 应该存在。和数据库并行维护，用来判断恢复得对不对。 */
typedef struct model {
    bool present[MODEL_KEYS];
} model;

/* 邮箱故意很长，一个叶子只放得下十几行，几条语句就会引起分裂。 */
static db_row row_for(uint32_t id) {
    db_row row;
    memset(&row, 0, sizeof row);
    row.id = id;
    snprintf(row.name, sizeof row.name, "user%" PRIu32, id);
    memset(row.email, 'a' + (int)(id % 26), 200);
    row.email[200] = '\0';
    return row;
}

static db *create_initial(model *m) {
    remove_db_files(CRASH_DB);
    memset(m, 0, sizeof *m);
    db *d = NULL;
    CHECK_OK(db_open(CRASH_DB, CACHE_FRAMES, &d));
    if (d == NULL) {
        return NULL;
    }
    CHECK_OK(db_begin(d));
    for (uint32_t id = 1; id <= INITIAL_LAST; id += 2) {
        db_row row = row_for(id);
        CHECK_OK(db_insert(d, &row));
        m->present[id] = true;
    }
    CHECK_OK(db_commit(d));
    return d;
}

static db_status apply(db *d, const op *o) {
    db_row row = row_for(o->id);
    switch (o->kind) {
    case OP_INSERT:
        return db_insert(d, &row);
    case OP_DELETE:
        return db_delete(d, o->id);
    case OP_BEGIN:
        return db_begin(d);
    case OP_COMMIT:
        return db_commit(d);
    case OP_ROLLBACK:
        return db_rollback(d);
    }
    return DB_MISUSE;
}

/* 语句成功后更新模型：事务里的修改记在 pending，提交时才算数。 */
static void update_model(const op *o, model *committed, model *pending,
                         bool *in_txn) {
    switch (o->kind) {
    case OP_INSERT:
        pending->present[o->id] = true;
        break;
    case OP_DELETE:
        pending->present[o->id] = false;
        break;
    case OP_BEGIN:
        *in_txn = true;
        break;
    case OP_COMMIT:
        *in_txn = false;
        break;
    case OP_ROLLBACK:
        *in_txn = false;
        *pending = *committed;
        break;
    }
    if (!*in_txn) {
        *committed = *pending;
    }
}

/* 恢复后的数据库必须结构完好，内容与模型完全一致。 */
static bool matches(db *d, const model *m, long k) {
    db_check_result r;
    if (db_check(d, &r) != DB_OK) {
        fprintf(stderr, "  crash point %ld: %s\n", k, r.message);
        return false;
    }
    uint32_t expected_rows = 0;
    for (uint32_t id = 0; id < MODEL_KEYS; id++) {
        db_row row;
        db_status rc = db_find(d, id, &row);
        if (!m->present[id]) {
            if (rc != DB_NOT_FOUND) {
                fprintf(stderr,
                        "  crash point %ld: row %" PRIu32 " should not exist\n",
                        k, id);
                return false;
            }
            continue;
        }
        expected_rows++;
        db_row want = row_for(id);
        if (rc != DB_OK || strcmp(row.name, want.name) != 0 ||
            strcmp(row.email, want.email) != 0) {
            fprintf(stderr, "  crash point %ld: row %" PRIu32 " is wrong\n", k,
                    id);
            return false;
        }
    }
    if (r.rows != expected_rows) {
        fprintf(stderr,
                "  crash point %ld: %" PRIu32 " rows, expected %" PRIu32 "\n",
                k, r.rows, expected_rows);
        return false;
    }
    return true;
}

static long count_workload_writes(void) {
    model m;
    db *d = create_initial(&m);
    if (d == NULL) {
        return 0;
    }
    long before = fault_write_count();
    for (size_t i = 0; i < WORKLOAD_SIZE; i++) {
        CHECK_OK(apply(d, &workload[i]));
    }
    long writes = fault_write_count() - before;
    db_close(d);
    return writes;
}

static void test_crash_at_every_write(void) {
    long total = count_workload_writes();
    CHECK(total > 0);
    long wrong = 0;
    for (long k = 0; k <= total; k++) {
        model committed;
        db *d = create_initial(&committed);
        if (d == NULL) {
            return;
        }
        model pending = committed;
        bool in_txn = false;

        fault_arm(k, FAULT_RETURN_ERROR);
        for (size_t i = 0; i < WORKLOAD_SIZE; i++) {
            db_status rc = apply(d, &workload[i]);
            if (fault_crashed()) {
                break;
            }
            CHECK_OK(rc);
            update_model(&workload[i], &committed, &pending, &in_txn);
        }
        CHECK(fault_crashed() == (k < total));
        /* 进程已经"死了"：关闭时的回滚写不进文件，磁盘停在崩溃那一刻。 */
        db_close(d);
        fault_disarm();

        d = NULL;
        CHECK_OK(db_open(CRASH_DB, CACHE_FRAMES, &d));
        if (d == NULL) {
            return;
        }
        if (!matches(d, &committed, k)) {
            wrong++;
        }
        db_row extra = row_for(MODEL_KEYS - 1); /* 恢复后还能正常写入 */
        CHECK_OK(db_insert(d, &extra));
        db_close(d);
    }
    CHECK(wrong == 0);
    printf("     %ld crash points, %ld recovered incorrectly\n", total + 1,
           wrong);
}

/* 大事务：把偶数 id 2～98 全部插进去，再删掉一部分奇数 id。 */
static void big_transaction(db *d) {
    CHECK_OK(db_begin(d));
    for (uint32_t id = 2; id < INITIAL_LAST; id += 2) {
        db_row row = row_for(id);
        CHECK_OK(db_insert(d, &row));
    }
    for (uint32_t id = 1; id <= INITIAL_LAST; id += 4) {
        CHECK_OK(db_delete(d, id));
    }
}

/*
 * 先制造最大的热日志：大事务提交到最后一步（删除日志）时崩溃，数据库
 * 文件里已全是新内容。再让恢复本身在每一次写入时崩溃。回放是幂等的，
 * 不管恢复被打断多少次，最终都回到事务开始前的状态。
 */
static void test_crash_during_recovery(void) {
    model initial;
    db *d = create_initial(&initial);
    if (d == NULL) {
        return;
    }
    big_transaction(d);
    long before = fault_write_count();
    CHECK_OK(db_commit(d));
    long commit_writes = fault_write_count() - before;
    db_close(d);

    d = create_initial(&initial);
    if (d == NULL) {
        return;
    }
    big_transaction(d);
    fault_arm(commit_writes - 1, FAULT_RETURN_ERROR); /* 删除日志时崩溃 */
    CHECK(db_commit(d) != DB_OK);
    CHECK(fault_crashed());
    db_close(d);
    fault_disarm();

    long interrupted = 0;
    for (long j = 0; j < 1000; j++) {
        fault_arm(j, FAULT_RETURN_ERROR);
        d = NULL;
        db_status rc = db_open(CRASH_DB, CACHE_FRAMES, &d);
        bool crashed = fault_crashed();
        fault_disarm();
        if (!crashed) {
            CHECK_OK(rc);
            CHECK(d != NULL && db_recovered_pages(d) > 0);
            db_close(d);
            break;
        }
        CHECK(rc != DB_OK);
        db_close(d);
        interrupted++;
    }
    CHECK(interrupted > 0);

    d = NULL;
    CHECK_OK(db_open(CRASH_DB, CACHE_FRAMES, &d));
    if (d != NULL) {
        CHECK(db_recovered_pages(d) == 0); /* 日志已经删掉了 */
        CHECK(matches(d, &initial, -1));
        db_close(d);
    }
    printf("     recovery interrupted %ld times, then completed\n",
           interrupted);
}

/*
 * 新建数据库同样要经得起崩溃：在 db_open 新建文件的每一次写操作处
 * 崩溃，再次打开时都必须得到一个完好的空库。文件头只写进一半的情形
 * 也在其中。
 */
static void test_crash_while_creating(void) {
    remove_db_files(CRASH_DB);
    db *d = NULL;
    long before = fault_write_count();
    CHECK_OK(db_open(CRASH_DB, CACHE_FRAMES, &d));
    long total = fault_write_count() - before;
    db_close(d);
    CHECK(total > 0);

    model empty;
    memset(&empty, 0, sizeof empty);
    long wrong = 0;
    for (long k = 0; k < total; k++) {
        remove_db_files(CRASH_DB);
        fault_arm(k, FAULT_RETURN_ERROR);
        d = NULL;
        CHECK(db_open(CRASH_DB, CACHE_FRAMES, &d) != DB_OK);
        CHECK(fault_crashed());
        fault_disarm();

        d = NULL;
        CHECK_OK(db_open(CRASH_DB, CACHE_FRAMES, &d));
        if (d == NULL || !matches(d, &empty, k)) {
            wrong++;
        }
        db_close(d);
    }
    CHECK(wrong == 0);
    printf("     %ld crash points while creating, %ld recovered incorrectly\n",
           total, wrong);
}

int main(void) {
    RUN_TEST(test_crash_at_every_write);
    RUN_TEST(test_crash_during_recovery);
    RUN_TEST(test_crash_while_creating);
    remove_db_files(CRASH_DB);
    return test_summary();
}
