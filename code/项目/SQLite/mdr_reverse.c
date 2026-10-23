#include <stdio.h>
#include <string.h>

#include "sqlite3.h"

/* mdr_reverse(text)：按字节倒序返回参数。 */
static void mdr_reverse(sqlite3_context *ctx, int argc, sqlite3_value **argv) {
    (void)argc;
    const unsigned char *src = sqlite3_value_text(argv[0]);
    int n = sqlite3_value_bytes(argv[0]);
    char *buf;

    if (src == NULL) {
        sqlite3_result_null(ctx);
        return;
    }
    buf = sqlite3_malloc(n + 1);
    if (buf == NULL) {
        sqlite3_result_error_nomem(ctx);
        return;
    }
    for (int i = 0; i < n; i++) {
        buf[i] = (char)src[n - 1 - i];
    }
    buf[n] = '\0';
    sqlite3_result_text(ctx, buf, n, sqlite3_free);
}

static void exec_sql(sqlite3 *db, const char *sql) {
    char *errmsg = NULL;
    if (sqlite3_exec(db, sql, NULL, NULL, &errmsg) != SQLITE_OK) {
        fprintf(stderr, "sql error: %s\n", errmsg);
        sqlite3_free(errmsg);
    }
}

static void print_explain(sqlite3 *db, const char *sql) {
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        fprintf(stderr, "prepare failed: %s\n", sqlite3_errmsg(db));
        return;
    }
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        printf("%-3d %-12s %-4d %-4d %-4d %s\n",
               sqlite3_column_int(stmt, 0), sqlite3_column_text(stmt, 1),
               sqlite3_column_int(stmt, 2), sqlite3_column_int(stmt, 3),
               sqlite3_column_int(stmt, 4),
               sqlite3_column_text(stmt, 5) ? (const char *)sqlite3_column_text(stmt, 5) : "");
    }
    sqlite3_finalize(stmt);
}

static void print_rows(sqlite3 *db, const char *sql) {
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        fprintf(stderr, "prepare failed: %s\n", sqlite3_errmsg(db));
        return;
    }
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        printf("%s\n", sqlite3_column_text(stmt, 0));
    }
    sqlite3_finalize(stmt);
}

int main(int argc, char **argv) {
    sqlite3 *db = NULL;
    if (sqlite3_open(":memory:", &db) != SQLITE_OK) {
        fprintf(stderr, "open failed\n");
        return 1;
    }
    if (sqlite3_create_function_v2(db, "mdr_reverse", 1,
                                   SQLITE_UTF8 | SQLITE_DETERMINISTIC, NULL,
                                   mdr_reverse, NULL, NULL, NULL) != SQLITE_OK) {
        fprintf(stderr, "register failed: %s\n", sqlite3_errmsg(db));
        sqlite3_close(db);
        return 1;
    }
    exec_sql(db, "CREATE TABLE users(id INTEGER PRIMARY KEY, name TEXT, age INTEGER)");
    exec_sql(db, "INSERT INTO users(name, age) VALUES"
                 "('Alice', 25), ('Bob', 35), ('Charlie', 40)");

    if (argc > 1 && strcmp(argv[1], "explain") == 0) {
        print_explain(db, "EXPLAIN SELECT mdr_reverse(name) FROM users");
    } else {
        print_rows(db, "SELECT mdr_reverse(name) FROM users ORDER BY age");
        print_rows(db, "SELECT mdr_reverse(name || '!') FROM users WHERE age > 30 ORDER BY age");
        print_rows(db, "SELECT mdr_reverse(NULL) IS NULL");
    }
    sqlite3_close(db);
    return 0;
}
