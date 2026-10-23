/*
 * test_cli.c：命令行分词与分派的测试。
 *
 * 编译：gcc -std=c17 -Wall -Wextra test_cli.c ../app/cli.c -I../app
 */
#include <stdio.h>
#include <string.h>

#include "cli.h"

#define CHECK(cond)                                                          \
    do {                                                                     \
        if (!(cond)) {                                                       \
            printf("    FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);       \
            ++g_failures;                                                    \
        }                                                                    \
    } while (0)

static int g_failures;
static char g_last_args[64];

static int cmd_echo(const char *args) {
    snprintf(g_last_args, sizeof g_last_args, "%s", args);
    return 0;
}

static const struct cli_command k_table[] = {
    {"echo", "回显", cmd_echo},
};

int main(void) {
    char buf[64];
    char *argv[4];

    printf("[1] split by runs of spaces and tabs\n");
    snprintf(buf, sizeof buf, "  rate \t 25  ");
    CHECK(cli_split(buf, argv, 4) == 2);
    CHECK(strcmp(argv[0], "rate") == 0);
    CHECK(strcmp(argv[1], "25") == 0);
    printf("    argv[0]=%s argv[1]=%s\n", argv[0], argv[1]);

    printf("[2] empty line\n");
    snprintf(buf, sizeof buf, "   \r\n");
    CHECK(cli_split(buf, argv, 4) == 0);

    printf("[3] dispatch trims trailing whitespace from args\n");
    snprintf(buf, sizeof buf, "echo hello \t");
    CHECK(cli_dispatch(k_table, 1, buf) == 0);
    CHECK(strcmp(g_last_args, "hello") == 0);
    printf("    args=%s\n", g_last_args);

    printf("[4] unknown command returns -1\n");
    snprintf(buf, sizeof buf, "frobnicate");
    CHECK(cli_dispatch(k_table, 1, buf) == -1);

    printf("[5] two words are not split into two args\n");
    snprintf(buf, sizeof buf, "echo a b");
    CHECK(cli_dispatch(k_table, 1, buf) == 0);
    CHECK(strcmp(g_last_args, "a b") == 0);
    printf("    args=%s\n", g_last_args);

    if (g_failures == 0) {
        printf("\nall tests passed\n");
        return 0;
    }
    printf("\n%d failure(s)\n", g_failures);
    return 1;
}