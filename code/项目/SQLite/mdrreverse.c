#include <stdio.h>
#include <string.h>

#include "sqlite3ext.h"
SQLITE_EXTENSION_INIT1

static void rev(sqlite3_context *ctx, int argc, sqlite3_value **argv) {
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

#ifdef _WIN32
__declspec(dllexport)
#endif
int sqlite3_mdrreverse_init(sqlite3 *db, char **pzErrMsg,
                            const sqlite3_api_routines *pApi) {
    SQLITE_EXTENSION_INIT2(pApi);
    (void)pzErrMsg;
    return sqlite3_create_function_v2(db, "mdr_reverse", 1,
                                      SQLITE_UTF8 | SQLITE_DETERMINISTIC, NULL,
                                      rev, NULL, NULL, NULL);
}
