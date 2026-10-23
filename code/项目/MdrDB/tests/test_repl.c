/*
 * test_repl.c：端到端测试。把一段命令脚本喂给 repl_run，逐字比较输出。
 */
#include "file.h"
#include "repl.h"
#include "test.h"

#include <stdlib.h>
#include <string.h>

#define REPL_DB "test_repl.db"
#define REPL_IN "test_repl.in"
#define REPL_OUT "test_repl.out"

static const char script[] = "insert 1 alice alice@example.com\n"
                             "insert 2 bob bob@example.com\n"
                             "insert 2 bob bob@example.com\n"
                             "select\n"
                             "select 2\n"
                             "select 7\n"
                             "select 1 1\n"
                             "delete 1\n"
                             "delete 1\n"
                             "begin\n"
                             "insert 3 carol carol@example.com\n"
                             "select\n"
                             "rollback\n"
                             "select\n"
                             "commit\n"
                             "insert x y z\n"
                             "insert 4 dave\n"
                             "update 4\n"
                             ".tree\n"
                             ".page 1\n"
                             ".check\n"
                             ".exit\n"
                             "select\n";

static const char expected[] =
    "mdrdb> insert 1 alice alice@example.com\n"
    "ok\n"
    "mdrdb> insert 2 bob bob@example.com\n"
    "ok\n"
    "mdrdb> insert 2 bob bob@example.com\n"
    "error: duplicate key\n"
    "mdrdb> select\n"
    "(1, alice, alice@example.com)\n"
    "(2, bob, bob@example.com)\n"
    "(2 rows)\n"
    "mdrdb> select 2\n"
    "(2, bob, bob@example.com)\n"
    "(1 row)\n"
    "mdrdb> select 7\n"
    "(0 rows)\n"
    "mdrdb> select 1 1\n"
    "(1, alice, alice@example.com)\n"
    "(1 row)\n"
    "mdrdb> delete 1\n"
    "ok\n"
    "mdrdb> delete 1\n"
    "error: not found\n"
    "mdrdb> begin\n"
    "ok\n"
    "mdrdb> insert 3 carol carol@example.com\n"
    "ok\n"
    "mdrdb> select\n"
    "(2, bob, bob@example.com)\n"
    "(3, carol, carol@example.com)\n"
    "(2 rows)\n"
    "mdrdb> rollback\n"
    "ok\n"
    "mdrdb> select\n"
    "(2, bob, bob@example.com)\n"
    "(1 row)\n"
    "mdrdb> commit\n"
    "error: no transaction is open\n"
    "mdrdb> insert x y z\n"
    "error: id must be a number from 0 to 4294967295\n"
    "mdrdb> insert 4 dave\n"
    "usage: insert <id> <name> <email>\n"
    "mdrdb> update 4\n"
    "error: unknown command 'update', try .help\n"
    "mdrdb> .tree\n"
    "leaf 1: 1 key (2..2)\n"
    "mdrdb> .page 1\n"
    "page 1: leaf\n"
    "  cells 1, free bytes 4056, content start 4070, next leaf 0\n"
    "  cell 0: offset 4070, size 26, key 2\n"
    "mdrdb> .check\n"
    "ok: rows 1, leaf pages 1, internal pages 0, height 1\n"
    "mdrdb> .exit\n";

static bool write_file(const char *path, const char *text) {
    FILE *fp = fopen(path, "wb");
    if (fp == NULL) {
        return false;
    }
    size_t len = strlen(text);
    bool ok = fwrite(text, 1, len, fp) == len;
    return fclose(fp) == 0 && ok;
}

static char *read_file(const char *path) {
    FILE *fp = fopen(path, "rb");
    if (fp == NULL) {
        return NULL;
    }
    char *text = NULL;
    long size = -1;
    if (fseek(fp, 0, SEEK_END) == 0) {
        size = ftell(fp);
    }
    if (size >= 0 && fseek(fp, 0, SEEK_SET) == 0) {
        text = malloc((size_t)size + 1);
    }
    if (text != NULL) {
        size_t n = fread(text, 1, (size_t)size, fp);
        text[n] = '\0';
    }
    fclose(fp);
    return text;
}

static void test_session(void) {
    remove_db_files(REPL_DB);
    CHECK(write_file(REPL_IN, script));
    db *d = NULL;
    CHECK_OK(db_open(REPL_DB, 0, &d));
    FILE *in = fopen(REPL_IN, "rb");
    FILE *out = fopen(REPL_OUT, "wb");
    CHECK(in != NULL && out != NULL);
    if (d != NULL && in != NULL && out != NULL) {
        repl_run(d, in, out, true);
    }
    if (in != NULL) {
        fclose(in);
    }
    if (out != NULL) {
        fclose(out);
    }
    db_close(d);

    char *got = read_file(REPL_OUT);
    CHECK(got != NULL && strcmp(got, expected) == 0);
    if (got != NULL && strcmp(got, expected) != 0) {
        fprintf(stderr, "--- expected:\n%s--- got:\n%s", expected, got);
    }
    free(got);
}

/* 会话里提交的修改在重新打开后仍然在，回滚的修改不在。 */
static void test_data_survives_reopen(void) {
    db *d = NULL;
    CHECK_OK(db_open(REPL_DB, 0, &d));
    if (d == NULL) {
        return;
    }
    db_row row;
    CHECK_OK(db_find(d, 2, &row));
    CHECK(strcmp(row.name, "bob") == 0);
    CHECK_STATUS(db_find(d, 1, &row), DB_NOT_FOUND);
    CHECK_STATUS(db_find(d, 3, &row), DB_NOT_FOUND);
    db_close(d);
}

/* 提交时写文件出错：事务被回滚，事务里的修改都没了，要告诉用户。 */
static void test_failed_commit_is_reported(void) {
    remove_db_files(REPL_DB);
    db *d = NULL;
    CHECK_OK(db_open(REPL_DB, 0, &d));
    FILE *out = fopen(REPL_OUT, "wb");
    CHECK(d != NULL && out != NULL);
    if (d != NULL && out != NULL) {
        char begin[] = "begin";
        char insert[] = "insert 5 eve eve@example.com";
        char commit[] = "commit";
        repl_execute(d, begin, out);
        repl_execute(d, insert, out);
        fault_arm(0, FAULT_RETURN_ERROR); /* 提交时的第一次写就失败 */
        repl_execute(d, commit, out);
        fault_disarm();
    }
    if (out != NULL) {
        fclose(out);
    }
    db_close(d);

    char *got = read_file(REPL_OUT);
    CHECK(got != NULL && strcmp(got, "ok\nok\nerror: I/O error\n"
                                     "the transaction was rolled back\n") == 0);
    free(got);
    d = NULL;
    CHECK_OK(db_open(REPL_DB, 0, &d));
    if (d != NULL) {
        db_row row;
        CHECK_STATUS(db_find(d, 5, &row), DB_NOT_FOUND);
        db_close(d);
    }
}

int main(void) {
    RUN_TEST(test_session);
    RUN_TEST(test_data_survives_reopen);
    RUN_TEST(test_failed_commit_is_reported);
    remove_db_files(REPL_DB);
    remove(REPL_IN);
    remove(REPL_OUT);
    return test_summary();
}
