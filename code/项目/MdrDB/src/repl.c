/*
 * repl.c：命令的解析与执行。
 *
 * 命令表把命令名、参数个数和处理函数放在一起：加一条命令只需加一行，
 * .help 的内容也从表里生成。
 */
#include "repl.h"

#include "file.h"

#include <inttypes.h>
#include <string.h>

#define MAX_WORDS 8
#define FAULT_MAX_WRITES 1000000u

typedef void command_fn(db *d, char **argv, FILE *out);

typedef struct command {
    const char *name;
    int min_args;
    int max_args;
    const char *usage;
    const char *help;
    command_fn *run; /* NULL 表示退出 */
} command;

/* 解析十进制无符号整数：整个单词都必须是数字，且不超过 UINT32_MAX。 */
static bool parse_u32(const char *s, uint32_t *out) {
    uint64_t value = 0;
    if (*s == '\0') {
        return false;
    }
    for (; *s != '\0'; s++) {
        if (*s < '0' || *s > '9') {
            return false;
        }
        value = value * 10 + (uint64_t)(*s - '0');
        if (value > UINT32_MAX) {
            return false;
        }
    }
    *out = (uint32_t)value;
    return true;
}

static bool parse_id(const char *s, uint32_t *id, FILE *out) {
    if (parse_u32(s, id)) {
        return true;
    }
    fprintf(out, "error: id must be a number from 0 to %" PRIu32 "\n",
            UINT32_MAX);
    return false;
}

static void report(db_status rc, FILE *out) {
    if (rc == DB_OK) {
        fputs("ok\n", out);
    } else {
        fprintf(out, "error: %s\n", db_status_message(rc));
    }
}

/* 修改失败时如果整个事务被回滚了，要告诉用户之前的修改也没了。 */
static void report_write(db *d, bool was_in_txn, db_status rc, FILE *out) {
    report(rc, out);
    if (rc != DB_OK && was_in_txn && !db_in_transaction(d)) {
        fputs("the transaction was rolled back\n", out);
    }
}

static void cmd_insert(db *d, char **argv, FILE *out) {
    db_row row;
    if (!parse_id(argv[1], &row.id, out)) {
        return;
    }
    size_t name_len = strlen(argv[2]);
    size_t email_len = strlen(argv[3]);
    if (name_len > DB_NAME_MAX) {
        fprintf(out, "error: name is longer than %d bytes\n", DB_NAME_MAX);
        return;
    }
    if (email_len > DB_EMAIL_MAX) {
        fprintf(out, "error: email is longer than %d bytes\n", DB_EMAIL_MAX);
        return;
    }
    memcpy(row.name, argv[2], name_len + 1);
    memcpy(row.email, argv[3], email_len + 1);
    bool was_in_txn = db_in_transaction(d);
    report_write(d, was_in_txn, db_insert(d, &row), out);
}

typedef struct select_state {
    FILE *out;
    unsigned long rows;
} select_state;

static bool print_row(const db_row *row, void *ctx) {
    select_state *st = ctx;
    fprintf(st->out, "(%" PRIu32 ", %s, %s)\n", row->id, row->name, row->email);
    st->rows++;
    return true;
}

/* select：全部行；select <id>：按主键点查；select <a> <b>：范围查询。 */
static void cmd_select(db *d, char **argv, FILE *out) {
    select_state st = {out, 0};
    uint32_t first = 0;
    uint32_t last = UINT32_MAX;
    db_status rc = DB_OK;
    if (argv[1] != NULL && argv[2] == NULL) {
        /* 点查：从根下降到叶子，在叶子里二分查找，找到就停。 */
        db_row row;
        if (!parse_id(argv[1], &first, out)) {
            return;
        }
        rc = db_find(d, first, &row);
        if (rc == DB_OK) {
            print_row(&row, &st);
        } else if (rc == DB_NOT_FOUND) {
            rc = DB_OK;
        }
    } else {
        if (argv[1] != NULL && (!parse_id(argv[1], &first, out) ||
                                !parse_id(argv[2], &last, out))) {
            return;
        }
        /* 范围查询：游标定位到第一个不小于 first 的键，再沿叶子链向右。 */
        rc = db_scan(d, first, last, print_row, &st);
    }
    if (rc != DB_OK) {
        report(rc, out);
        return;
    }
    fprintf(out, "(%lu row%s)\n", st.rows, st.rows == 1 ? "" : "s");
}

static void cmd_delete(db *d, char **argv, FILE *out) {
    uint32_t id;
    if (!parse_id(argv[1], &id, out)) {
        return;
    }
    bool was_in_txn = db_in_transaction(d);
    report_write(d, was_in_txn, db_delete(d, id), out);
}

static void cmd_begin(db *d, char **argv, FILE *out) {
    (void)argv;
    if (db_in_transaction(d)) {
        fputs("error: a transaction is already open\n", out);
        return;
    }
    report(db_begin(d), out);
}

static void cmd_commit(db *d, char **argv, FILE *out) {
    (void)argv;
    if (!db_in_transaction(d)) {
        fputs("error: no transaction is open\n", out);
        return;
    }
    /* 提交失败时 db_commit 会回滚事务，同样要告诉用户。 */
    report_write(d, true, db_commit(d), out);
}

static void cmd_rollback(db *d, char **argv, FILE *out) {
    (void)argv;
    if (!db_in_transaction(d)) {
        fputs("error: no transaction is open\n", out);
        return;
    }
    report(db_rollback(d), out);
}

static void cmd_tree(db *d, char **argv, FILE *out) {
    (void)argv;
    db_status rc = db_print_tree(d, out);
    if (rc != DB_OK) {
        report(rc, out);
    }
}

static void cmd_page(db *d, char **argv, FILE *out) {
    uint32_t pgno;
    if (!parse_u32(argv[1], &pgno)) {
        fputs("error: page number must be a number\n", out);
        return;
    }
    db_status rc = db_print_page(d, pgno, out);
    if (rc == DB_NOT_FOUND) {
        fprintf(out, "error: page %" PRIu32 " does not exist\n", pgno);
    } else if (rc != DB_OK) {
        report(rc, out);
    }
}

static void cmd_info(db *d, char **argv, FILE *out) {
    (void)argv;
    db_print_info(d, out);
}

static void cmd_check(db *d, char **argv, FILE *out) {
    (void)argv;
    db_check_result r;
    if (db_check(d, &r) != DB_OK) {
        fprintf(out, "error: %s\n", r.message);
        return;
    }
    fprintf(out,
            "ok: rows %" PRIu32 ", leaf pages %" PRIu32
            ", internal pages %" PRIu32 ", height %" PRIu32 "\n",
            r.rows, r.leaf_pages, r.internal_pages, r.height);
}

/* 演示崩溃恢复：再允许 n 次写操作，之后的那一次让进程直接退出。 */
static void cmd_fault(db *d, char **argv, FILE *out) {
    (void)d;
    uint32_t n;
    if (!parse_u32(argv[1], &n) || n > FAULT_MAX_WRITES) {
        fprintf(out, "error: n must be a number from 0 to %u\n",
                FAULT_MAX_WRITES);
        return;
    }
    fault_arm((long)n, FAULT_EXIT);
    fprintf(out, "fault: the process will crash after %" PRIu32 " writes\n", n);
}

static void cmd_help(db *d, char **argv, FILE *out);

static const command commands[] = {
    {"insert", 3, 3, "insert <id> <name> <email>", "add a row", cmd_insert},
    {"select", 0, 2, "select [<id> | <first> <last>]", "show rows", cmd_select},
    {"delete", 1, 1, "delete <id>", "remove a row", cmd_delete},
    {"begin", 0, 0, "begin", "start a transaction", cmd_begin},
    {"commit", 0, 0, "commit", "make the transaction permanent", cmd_commit},
    {"rollback", 0, 0, "rollback", "undo the transaction", cmd_rollback},
    {".tree", 0, 0, ".tree", "print the shape of the B+ tree", cmd_tree},
    {".page", 1, 1, ".page <n>", "print the layout of page n", cmd_page},
    {".info", 0, 0, ".info", "print file and cache statistics", cmd_info},
    {".check", 0, 0, ".check", "verify the B+ tree and records", cmd_check},
    {".fault", 1, 1, ".fault <n>", "crash after n more writes", cmd_fault},
    {".help", 0, 0, ".help", "list commands", cmd_help},
    {".exit", 0, 0, ".exit", "quit", NULL},
};

#define COMMAND_COUNT (sizeof commands / sizeof commands[0])

static void cmd_help(db *d, char **argv, FILE *out) {
    (void)d;
    (void)argv;
    for (size_t i = 0; i < COMMAND_COUNT; i++) {
        fprintf(out, "  %-32s %s\n", commands[i].usage, commands[i].help);
    }
}

static const command *find_command(const char *name) {
    for (size_t i = 0; i < COMMAND_COUNT; i++) {
        if (strcmp(commands[i].name, name) == 0) {
            return &commands[i];
        }
    }
    return NULL;
}

repl_result repl_execute(db *d, char *line, FILE *out) {
    char *argv[MAX_WORDS + 1];
    int argc = 0;
    /* strtok 把分隔符改写成 '\0'，line 就地切成一个个单词。 */
    for (char *w = strtok(line, " \t\r\n"); w != NULL;
         w = strtok(NULL, " \t\r\n")) {
        if (argc == MAX_WORDS) {
            fputs("error: too many words\n", out);
            return REPL_CONTINUE;
        }
        argv[argc++] = w;
    }
    argv[argc] = NULL; /* 与 main 的 argv 一样以 NULL 结尾 */
    if (argc == 0) {
        return REPL_CONTINUE;
    }
    const command *cmd = find_command(argv[0]);
    if (cmd == NULL) {
        fprintf(out, "error: unknown command '%s', try .help\n", argv[0]);
        return REPL_CONTINUE;
    }
    if (argc - 1 < cmd->min_args || argc - 1 > cmd->max_args) {
        fprintf(out, "usage: %s\n", cmd->usage);
        return REPL_CONTINUE;
    }
    if (cmd->run == NULL) {
        return REPL_EXIT;
    }
    cmd->run(d, argv, out);
    return REPL_CONTINUE;
}

void repl_run(db *d, FILE *in, FILE *out, bool echo) {
    char line[REPL_LINE_MAX];
    for (;;) {
        fputs("mdrdb> ", out);
        fflush(out);
        if (fgets(line, sizeof line, in) == NULL) {
            fputc('\n', out);
            break;
        }
        size_t len = strlen(line);
        bool complete = len > 0 && line[len - 1] == '\n';
        if (!complete && len == sizeof line - 1) {
            int c;
            do {
                c = fgetc(in);
            } while (c != EOF && c != '\n');
            fputs("error: line is too long\n", out);
            continue;
        }
        if (echo) {
            fputs(line, out);
            if (!complete) {
                fputc('\n', out);
            }
            /* 命令可能让进程崩溃（.fault），先把已有的输出送出去。 */
            fflush(out);
        }
        if (repl_execute(d, line, out) == REPL_EXIT) {
            break;
        }
    }
    fflush(out);
}
