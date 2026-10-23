/*
 * main.c：mdrdb 命令行程序。
 *
 * 用法：mdrdb [--echo] [--cache 页框数] 数据库文件
 */
#include "mdrdb.h"
#include "repl.h"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void print_usage(void) {
    fputs("usage: mdrdb [--echo] [--cache frames] file\n", stderr);
}

static bool parse_frames(const char *s, size_t *frames) {
    char *end;
    unsigned long value = strtoul(s, &end, 10);
    if (*s < '0' || *s > '9' || *end != '\0' || value == 0 ||
        value > 1000000ul) {
        return false;
    }
    *frames = (size_t)value;
    return true;
}

int main(int argc, char *argv[]) {
    const char *path = NULL;
    bool echo = false;
    size_t frames = 0;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--echo") == 0) {
            echo = true;
        } else if (strcmp(argv[i], "--cache") == 0 && i + 1 < argc) {
            if (!parse_frames(argv[++i], &frames)) {
                print_usage();
                return EXIT_FAILURE;
            }
        } else if (argv[i][0] == '-' || path != NULL) {
            print_usage();
            return EXIT_FAILURE;
        } else {
            path = argv[i];
        }
    }
    if (path == NULL) {
        print_usage();
        return EXIT_FAILURE;
    }

    db *d;
    db_status rc = db_open(path, frames, &d);
    if (rc != DB_OK) {
        fprintf(stderr, "mdrdb: cannot open %s: %s\n", path,
                db_status_message(rc));
        return EXIT_FAILURE;
    }
    uint32_t restored = db_recovered_pages(d);
    if (restored > 0) {
        printf("recovery: %" PRIu32 " page%s restored from the journal\n",
               restored, restored == 1 ? "" : "s");
    }
    repl_run(d, stdin, stdout, echo);
    if (db_in_transaction(d)) {
        puts("the open transaction was rolled back");
    }
    db_close(d); /* 没有提交的事务在这里回滚 */
    return EXIT_SUCCESS;
}
