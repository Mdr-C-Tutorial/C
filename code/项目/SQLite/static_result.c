#include <stdio.h>
#include <string.h>
#include "sqlite3.h"

static void rev_s_static(sqlite3_context *ctx, int argc, sqlite3_value **argv) {
    (void)argc;
    const unsigned char *src = sqlite3_value_text(argv[0]);
    int n = sqlite3_value_bytes(argv[0]);
    char buf[64];
    if (src == NULL || n >= (int)sizeof buf) { sqlite3_result_null(ctx); return; }
    for (int i = 0; i < n; i++) buf[i] = (char)src[n - 1 - i];
    buf[n] = '\0';
    sqlite3_result_text(ctx, buf, n, SQLITE_STATIC);
}

static void rev_c_copy(sqlite3_context *ctx, int argc, sqlite3_value **argv) {
    (void)argc;
    const unsigned char *src = sqlite3_value_text(argv[0]);
    int n = sqlite3_value_bytes(argv[0]);
    char *buf = sqlite3_malloc(n + 1);
    if (src == NULL) { sqlite3_result_null(ctx); return; }
    if (buf == NULL) { sqlite3_result_error_nomem(ctx); return; }
    for (int i = 0; i < n; i++) buf[i] = (char)src[n - 1 - i];
    buf[n] = '\0';
    sqlite3_result_text(ctx, buf, n, sqlite3_free);
}

static void run(sqlite3 *db, const char *tag, const char *sql) {
    sqlite3_stmt *st = NULL;
    printf("%-6s ", tag);
    sqlite3_prepare_v2(db, sql, -1, &st, 0);
    while (sqlite3_step(st) == SQLITE_ROW) {
        for (int i = 0; i < sqlite3_column_count(st); i++) {
            printf("[%s]", sqlite3_column_text(st, i));
        }
        printf(" ");
    }
    printf("\n");
    sqlite3_finalize(st);
}

int main(void) {
    sqlite3 *db = NULL;
    sqlite3_open(":memory:", &db);
    sqlite3_create_function_v2(db, "rev_s", 1, SQLITE_UTF8, 0, rev_s_static, 0, 0, 0);
    sqlite3_create_function_v2(db, "rev_c", 1, SQLITE_UTF8, 0, rev_c_copy, 0, 0, 0);
    run(db, "static", "SELECT rev_s('Alice'), rev_s('Bob')");
    run(db, "copy",   "SELECT rev_c('Alice'), rev_c('Bob')");
    sqlite3_close(db);
    return 0;
}
