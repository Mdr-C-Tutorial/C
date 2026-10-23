#include <stdio.h>
#include <string.h>

#include "sqlite3.h"

static int failures = 0;

#define CHECK(cond)                                                            \
    do {                                                                       \
        if (!(cond)) {                                                         \
            fprintf(stderr, "%s:%d: check failed: %s\n", __FILE__, __LINE__,   \
                    #cond);                                                    \
            failures += 1;                                                     \
        }                                                                      \
    } while (0)

static int exec_sql(sqlite3 *db, const char *sql) {
    char *errmsg = NULL;
    int rc = sqlite3_exec(db, sql, NULL, NULL, &errmsg);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "sql error: %s\n", errmsg);
        sqlite3_free(errmsg);
    }
    return rc;
}

static void create_schema(sqlite3 *db) {
    CHECK(exec_sql(db, "CREATE TABLE users("
                       "id INTEGER PRIMARY KEY, name TEXT NOT NULL, age INTEGER)") == SQLITE_OK);
}

static void insert_users(sqlite3 *db) {
    sqlite3_stmt *stmt = NULL;
    const char *sql = "INSERT INTO users(name, age) VALUES(?, ?)";
    CHECK(sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) == SQLITE_OK);
    const char *names[] = {"Alice", "Bob", "Charlie"};
    int ages[] = {25, 35, 40};
    for (int i = 0; i < 3; i++) {
        sqlite3_bind_text(stmt, 1, names[i], -1, SQLITE_STATIC);
        sqlite3_bind_int(stmt, 2, ages[i]);
        CHECK(sqlite3_step(stmt) == SQLITE_DONE);
        sqlite3_reset(stmt);
    }
    sqlite3_finalize(stmt);
}

static void check_query(sqlite3 *db) {
    sqlite3_stmt *stmt = NULL;
    const char *sql = "SELECT name FROM users WHERE age > ? ORDER BY age";
    CHECK(sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) == SQLITE_OK);
    sqlite3_bind_int(stmt, 1, 30);

    const char *expected[] = {"Bob", "Charlie"};
    int row = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        const unsigned char *name = sqlite3_column_text(stmt, 0);
        CHECK(row < 2);
        if (row < 2) {
            CHECK(strcmp((const char *)name, expected[row]) == 0);
        }
        row++;
    }
    CHECK(row == 2);
    sqlite3_finalize(stmt);
}

int main(void) {
    sqlite3 *db = NULL;
    CHECK(sqlite3_open(":memory:", &db) == SQLITE_OK);
    create_schema(db);
    insert_users(db);
    check_query(db);
    CHECK(sqlite3_close(db) == SQLITE_OK);
    if (failures == 0) {
        printf("all checks passed\n");
        return 0;
    }
    printf("%d check(s) failed\n", failures);
    return 1;
}
