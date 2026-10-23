/* kilo_drive —— 在伪终端里运行 kilo，按脚本送入按键并收集屏幕输出。
 *
 * 用法：
 *   kilo_drive [-o raw.out] [-p all.txt] [-l last.txt] [-t 毫秒] [-s 毫秒]
 *              脚本 -- 程序 [参数...]
 *
 * 脚本每行一条动作：
 *   # 注释
 *   sleep 300          等待 300 毫秒，同时读走程序输出
 *   keys hello<C-s>    送入按键；记号写在尖括号里，如 <C-s>、<bs>、<up>
 *
 * 退出码：0 正常；1 用法或输入错误；2 子进程失败；3 超时；4 输出中有 sanitizer 报告。
 */
#define _XOPEN_SOURCE 700

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <pty.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define EXIT_USAGE 1
#define EXIT_CHILD 2
#define EXIT_TIMEOUT 3
#define EXIT_SANITIZER 4

#define SCREEN_ROWS 24
#define SCREEN_COLS 80

struct buffer {
    char *data;
    size_t len;
    size_t cap;
};

static void buffer_append(struct buffer *b, const char *s, size_t n) {
    if (b->len + n > b->cap) {
        size_t cap = b->cap ? b->cap : 4096;

        while (cap < b->len + n) {
            cap *= 2;
        }
        char *data = realloc(b->data, cap);

        if (data == NULL) {
            perror("realloc");
            exit(EXIT_USAGE);
        }
        b->data = data;
        b->cap = cap;
    }
    memcpy(b->data + b->len, s, n);
    b->len += n;
}

static void die(const char *what) {
    perror(what);
    exit(EXIT_USAGE);
}

static long now_ms(void) {
    struct timespec ts;

    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000L + ts.tv_nsec / 1000000L;
}

static int contains(const struct buffer *b, const char *needle) {
    size_t n = strlen(needle);

    if (b->len < n) {
        return 0;
    }
    for (size_t i = 0; i + n <= b->len; i++) {
        if (memcmp(b->data + i, needle, n) == 0) {
            return 1;
        }
    }
    return 0;
}

/* 从主设备读一次。返回 0 表示对端已经全部关闭。 */
static int drain_once(int fd, struct buffer *out) {
    char buf[4096];
    ssize_t n = read(fd, buf, sizeof buf);

    if (n > 0) {
        buffer_append(out, buf, (size_t)n);
        return 1;
    }
    if (n == -1 && errno == EINTR) {
        return 1;
    }
    if (n == -1 && errno == EIO) {
        return 0; /* 从设备全部关闭之后，主设备返回 EIO，而不是 0 */
    }
    if (n == -1) {
        die("read");
    }
    return 0;
}

/* 读输出，直到连续 quiet_ms 毫秒没有新数据。 */
static void pump(int fd, struct buffer *out, int quiet_ms, int *closed) {
    long deadline = now_ms() + quiet_ms;

    while (!*closed) {
        long left = deadline - now_ms();

        if (left <= 0) {
            break;
        }
        struct pollfd p = {.fd = fd, .events = POLLIN, .revents = 0};
        int r = poll(&p, 1, (int)left);

        if (r == -1) {
            if (errno == EINTR) {
                continue;
            }
            die("poll");
        }
        if (r == 0) {
            break;
        }
        if (!drain_once(fd, out)) {
            *closed = 1;
        }
        deadline = now_ms() + quiet_ms;
    }
}

static int write_all(int fd, const char *s, size_t n) {
    size_t off = 0;

    while (off < n) {
        ssize_t w = write(fd, s + off, n - off);

        if (w == -1) {
            if (errno == EINTR) {
                continue;
            }
            return -1;
        }
        off += (size_t)w;
    }
    return 0;
}

static const struct {
    const char *name;
    const char *bytes;
} key_names[] = {
    {"<esc>", "\x1b"},      {"<enter>", "\r"},     {"<tab>", "\t"},
    {"<bs>", "\x7f"},       {"<del>", "\x1b[3~"},  {"<up>", "\x1b[A"},
    {"<down>", "\x1b[B"},   {"<right>", "\x1b[C"}, {"<left>", "\x1b[D"},
    {"<home>", "\x1b[H"},   {"<end>", "\x1b[F"},   {"<pgup>", "\x1b[5~"},
    {"<pgdn>", "\x1b[6~"},
};

/* 把带记号的文本展开成要写给主设备的字节。\< 表示字面上的 <。 */
static void expand_keys(const char *s, struct buffer *out, int lineno) {
    while (*s) {
        if (s[0] == '\\' && s[1] == '<') {
            buffer_append(out, "<", 1);
            s += 2;
            continue;
        }
        if (s[0] == '<') {
            if (s[1] == 'C' && s[2] == '-' && s[3] >= 'a' && s[3] <= 'z' &&
                s[4] == '>') {
                char c = (char)(s[3] - 'a' + 1);

                buffer_append(out, &c, 1);
                s += 5;
                continue;
            }
            size_t i;
            int found = 0;

            for (i = 0; i < sizeof key_names / sizeof key_names[0]; i++) {
                size_t n = strlen(key_names[i].name);

                if (strncmp(s, key_names[i].name, n) == 0) {
                    buffer_append(out, key_names[i].bytes,
                                  strlen(key_names[i].bytes));
                    s += n;
                    found = 1;
                    break;
                }
            }
            if (found) {
                continue;
            }
            fprintf(stderr, "kilo_drive: 脚本第 %d 行：不认识的记号 %.5s\n",
                    lineno, s);
            exit(EXIT_USAGE);
        }
        buffer_append(out, s, 1);
        s++;
    }
}

/* 去掉 VT100 转义序列与 \r，留下屏幕上能看到的文字。 */
static void strip_escapes(const struct buffer *in, struct buffer *out) {
    size_t i = 0;

    while (i < in->len) {
        unsigned char c = (unsigned char)in->data[i];

        if (c == 0x1b && i + 1 < in->len) {
            unsigned char next = (unsigned char)in->data[i + 1];

            if (next == '[') {
                i += 2;
                while (i < in->len &&
                       !(in->data[i] >= '@' && in->data[i] <= '~')) {
                    i++;
                }
                i++;
                continue;
            }
            if (next == ']') {
                i += 2;
                while (i < in->len && in->data[i] != '\a') {
                    if (in->data[i] == 0x1b && i + 1 < in->len &&
                        in->data[i + 1] == '\\') {
                        i++;
                        break;
                    }
                    i++;
                }
                i++;
                continue;
            }
            i += 2;
            continue;
        }
        if (c == '\r') {
            i++;
            continue;
        }
        buffer_append(out, in->data + i, 1);
        i++;
    }
}

/* 取最后一帧（最后一次 ESC[?25l 之后的部分）。 */
static void last_frame(const struct buffer *in, struct buffer *out) {
    size_t start = 0;

    for (size_t i = 0; i + 6 <= in->len; i++) {
        if (memcmp(in->data + i, "\x1b[?25l", 6) == 0) {
            start = i;
        }
    }
    buffer_append(out, in->data + start, in->len - start);
}

/* 把最后一帧整理成可比较的文本：只保留正文行，去掉行尾空格、
 * 空行和只有 ~ 的行。 */
static void content_lines(const struct buffer *frame, struct buffer *out) {
    struct buffer plain = {0};
    size_t i = 0;
    int line = 0;

    strip_escapes(frame, &plain);
    while (i < plain.len && line < SCREEN_ROWS - 2) {
        size_t end = i;

        while (end < plain.len && plain.data[end] != '\n') {
            end++;
        }
        size_t len = end - i;

        while (len > 0 && (plain.data[i + len - 1] == ' ' ||
                           plain.data[i + len - 1] == '\t')) {
            len--;
        }
        /* 行首的空白是行号栏留下的，判断是不是 ~ 行时先跳过。 */
        size_t start = i;
        size_t skipped = 0;

        while (skipped < len && (plain.data[start] == ' ' ||
                                 plain.data[start] == '\t')) {
            start++;
            skipped++;
        }
        int tilde = len > skipped;

        for (size_t k = start; k < i + len; k++) {
            if (plain.data[k] != '~') {
                tilde = 0;
            }
        }
        if (len > 0 && !tilde) {
            buffer_append(out, plain.data + i, len);
            buffer_append(out, "\n", 1);
        }
        i = end + 1;
        line++;
    }
    free(plain.data);
}

static void save(const char *path, const struct buffer *b) {
    if (path == NULL) {
        return;
    }
    FILE *fp = fopen(path, "wb");

    if (fp == NULL) {
        die(path);
    }
    if (fwrite(b->data, 1, b->len, fp) != b->len) {
        die(path);
    }
    fclose(fp);
}

static void usage(void) {
    fprintf(stderr,
            "用法：kilo_drive [-o raw.out] [-p all.txt] [-l last.txt]"
            " [-t 毫秒] [-s 毫秒] 脚本 -- 程序 [参数...]\n");
    exit(EXIT_USAGE);
}

int main(int argc, char **argv) {
    const char *raw_path = NULL;
    const char *plain_path = NULL;
    const char *last_path = NULL;
    const char *script_path;
    int timeout_ms = 10000;
    int settle_ms = 120;
    int opt;

    while ((opt = getopt(argc, argv, "o:p:l:t:s:")) != -1) {
        switch (opt) {
        case 'o':
            raw_path = optarg;
            break;
        case 'p':
            plain_path = optarg;
            break;
        case 'l':
            last_path = optarg;
            break;
        case 't':
            timeout_ms = atoi(optarg);
            break;
        case 's':
            settle_ms = atoi(optarg);
            break;
        default:
            usage();
        }
    }
    if (optind >= argc) {
        usage();
    }
    script_path = argv[optind++];
    if (optind < argc && strcmp(argv[optind], "--") == 0) {
        optind++;
    }
    if (optind >= argc) {
        usage();
    }

    FILE *script = fopen(script_path, "r");

    if (script == NULL) {
        die(script_path);
    }

    struct winsize ws = {.ws_row = SCREEN_ROWS, .ws_col = SCREEN_COLS,
                         .ws_xpixel = 0, .ws_ypixel = 0};
    int master;
    pid_t pid = forkpty(&master, NULL, NULL, &ws);

    if (pid == -1) {
        die("forkpty");
    }
    if (pid == 0) {
        execvp(argv[optind], &argv[optind]);
        perror(argv[optind]);
        _exit(127);
    }

    struct buffer raw = {0};
    int closed = 0;
    long deadline = now_ms() + timeout_ms;
    char line[1024];
    int lineno = 0;

    pump(master, &raw, settle_ms, &closed); /* 等第一帧画完 */

    while (fgets(line, sizeof line, script) != NULL) {
        lineno++;
        size_t len = strlen(line);

        while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r')) {
            line[--len] = '\0';
        }
        char *p = line;

        while (*p == ' ' || *p == '\t') {
            p++;
        }
        if (*p == '\0' || *p == '#') {
            continue;
        }
        if (strncmp(p, "sleep", 5) == 0 && (p[5] == ' ' || p[5] == '\t')) {
            pump(master, &raw, atoi(p + 5), &closed);
            continue;
        }
        if (strncmp(p, "keys", 4) == 0 && (p[4] == ' ' || p[4] == '\t')) {
            struct buffer bytes = {0};

            expand_keys(p + 5, &bytes, lineno);
            if (closed || write_all(master, bytes.data ? bytes.data : "",
                                    bytes.len) == -1) {
                free(bytes.data);
                break;
            }
            free(bytes.data);
            pump(master, &raw, settle_ms, &closed);
            continue;
        }
        fprintf(stderr, "kilo_drive: 脚本第 %d 行：只支持 sleep 和 keys\n",
                lineno);
        return EXIT_USAGE;
    }
    fclose(script);

    int status = 0;
    int exited = 0;

    while (now_ms() < deadline) {
        pid_t r = waitpid(pid, &status, WNOHANG);

        if (r == pid) {
            exited = 1;
            break;
        }
        if (r == -1) {
            die("waitpid");
        }
        pump(master, &raw, 20, &closed);
        struct timespec ts = {0, 20 * 1000 * 1000};

        nanosleep(&ts, NULL);
    }

    int code;

    if (!exited) {
        fprintf(stderr, "kilo_drive: 超时，结束子进程 %ld\n", (long)pid);
        kill(pid, SIGKILL);
        waitpid(pid, &status, 0);
        code = EXIT_TIMEOUT;
    } else if (WIFSIGNALED(status)) {
        fprintf(stderr, "kilo_drive: 子进程被信号 %d 终止\n", WTERMSIG(status));
        code = EXIT_CHILD;
    } else if (WEXITSTATUS(status) != 0) {
        fprintf(stderr, "kilo_drive: 子进程退出码 %d\n", WEXITSTATUS(status));
        code = EXIT_CHILD;
    } else {
        code = 0;
    }

    /* 子进程退出之后把主设备里剩下的输出读完。 */
    pump(master, &raw, 50, &closed);
    close(master);
    save(raw_path, &raw);

    if (plain_path != NULL) {
        struct buffer plain = {0};

        strip_escapes(&raw, &plain);
        save(plain_path, &plain);
        free(plain.data);
    }
    if (last_path != NULL) {
        struct buffer frame = {0};
        struct buffer content = {0};

        last_frame(&raw, &frame);
        content_lines(&frame, &content);
        save(last_path, &content);
        free(frame.data);
        free(content.data);
    }

    if (contains(&raw, "ERROR: AddressSanitizer")) {
        fprintf(stderr, "kilo_drive: 输出中有 AddressSanitizer 报告\n");
        code = EXIT_SANITIZER;
    } else if (contains(&raw, "runtime error:")) {
        fprintf(stderr, "kilo_drive: 输出中有 UndefinedBehaviorSanitizer 报告\n");
        code = EXIT_SANITIZER;
    }

    return code;
}