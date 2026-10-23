#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <string.h>

#include "sqlite3.h"
#include "vfslog.h"

static int exec_sql(sqlite3 *db, const char *sql) {
    char *errmsg = NULL;
    int rc = sqlite3_exec(db, sql, NULL, NULL, &errmsg);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "sql error: %s\n", errmsg);
        sqlite3_free(errmsg);
    }
    return rc;
}

static int open_db(const char *path, sqlite3 **db) {
    return sqlite3_open_v2(path, db, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, "log");
}

int main(int argc, char **argv) {
    sqlite3 *db = NULL;
    const char *mode = (argc > 1) ? argv[1] : "rollback";
    const char *path = (argc > 2) ? argv[2] : "vfsdemo.db";
    int wal = (strcmp(mode, "wal") == 0);
    char sql[256];

    setvbuf(stdout, NULL, _IONBF, 0);
    if (vfslog_register("log", stdout, 0) != SQLITE_OK) {
        fprintf(stderr, "register failed\n");
        return 1;
    }
    if (open_db(path, &db) != SQLITE_OK) {
        fprintf(stderr, "open failed: %s\n", sqlite3_errmsg(db));
        return 1;
    }
    printf("=== mode: %s ===\n", wal ? "wal" : "rollback");
    if (wal) {
        snprintf(sql, sizeof sql, "PRAGMA journal_mode=WAL");
        exec_sql(db, sql);
    } else {
        snprintf(sql, sizeof sql, "PRAGMA journal_mode=DELETE");
        exec_sql(db, sql);
    }
    exec_sql(db, "CREATE TABLE IF NOT EXISTS items(id INTEGER PRIMARY KEY, name TEXT)");
    printf("--- one INSERT ---\n");
    exec_sql(db, "BEGIN");
    exec_sql(db, "INSERT INTO items(name) VALUES('alpha')");
    exec_sql(db, "COMMIT");
    printf("--- checkpoint / close ---\n");
    if (wal) {
        exec_sql(db, "PRAGMA wal_checkpoint(TRUNCATE)");
    }
    sqlite3_close(db);
    printf("=== reopened for reading ===\n");
    if (open_db(path, &db) != SQLITE_OK) {
        return 1;
    }
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, "SELECT count(*) FROM items", -1, &stmt, NULL) == SQLITE_OK) {
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            printf("rows = %d\n", sqlite3_column_int(stmt, 0));
        }
    }
    sqlite3_finalize(stmt);
    sqlite3_close(db);
    return 0;
}
