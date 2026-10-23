// 集成测试：启动真实的 mdr_server 子进程，经回环地址发送请求、检查响应。
//
// 用法：integration_test <mdr_server 的路径> <端口>
//
// 测试用的文档根目录建在 $TMPDIR（未设置时为 /tmp）下，结束时删除。
// 服务器子进程设置了 PR_SET_PDEATHSIG：测试进程不管怎样退出（包括被
// timeout 或 Ctrl+C 杀死），内核都会结束服务器，它不会留在后台占着端口。

#define _POSIX_C_SOURCE 200809L

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <poll.h>
#include <signal.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/prctl.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

// 测试服务器的参数。连接数和超时都设得很小，相关的测试才能很快跑完。
#define TEST_MAX_CONNECTIONS 4
#define TEST_MAX_REQUEST 4096
#define TEST_IDLE_TIMEOUT_MS 1000

// 比套接字发送缓冲区和接收缓冲区加起来还大，服务器一定会遇到 EAGAIN。
#define BIG_FILE_SIZE (8u * 1024 * 1024)

static const char INDEX_HTML[] = "<h1>Mdr Server</h1>\n";
static const char HELLO_TXT[] = "hello, world\n";
static const char DOCS_INDEX[] = "<p>docs</p>\n";
static const char SECRET_TXT[] = "secret\n";
static const char OUTSIDE_TXT[] = "outside of the document root\n";
static const char UTF8_TXT[] = "你好\n";

static const char *server_path;
static int server_port;
static pid_t server_pid = -1;
static char work_dir[PATH_MAX];

// ---------------------------------------------------------------------------
// 工具函数

static char failure_reason[512];

// 记下失败原因并返回 false，方便写成 return fail(...)。
static bool fail(const char *format, ...) {
    va_list args;
    va_start(args, format);
    vsnprintf(failure_reason, sizeof failure_reason, format, args);
    va_end(args);
    return false;
}

static int64_t now_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (int64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

static void sleep_ms(int ms) {
    struct timespec ts = {ms / 1000, (long)(ms % 1000) * 1000000L};
    while (nanosleep(&ts, &ts) < 0 && errno == EINTR) {
    }
}

static const char *in_work_dir(const char *name) {
    static char path[PATH_MAX + 64];
    snprintf(path, sizeof path, "%s/%s", work_dir, name);
    return path;
}

static unsigned char big_byte(size_t i) {
    return (unsigned char)((i * 7) ^ (i >> 12));
}

// ---------------------------------------------------------------------------
// 测试用的文档根目录

static const char *const fixture_dirs[] = {"root", "root/docs"};
static const char *const fixture_files[] = {
    "root/index.html",
    "root/hello.txt",
    "root/docs/index.html",
    "root/.secret",
    "root/\xE4\xB8\xAD\xE6\x96\x87.txt",
    "root/big.bin",
    "outside.txt",
    "server.log",
};

static bool write_file(const char *name, const void *data, size_t length) {
    FILE *fp = fopen(in_work_dir(name), "wb");
    if (fp == NULL) {
        return false;
    }
    bool ok = fwrite(data, 1, length, fp) == length;
    return fclose(fp) == 0 && ok;
}

static bool write_big_file(void) {
    FILE *fp = fopen(in_work_dir("root/big.bin"), "wb");
    if (fp == NULL) {
        return false;
    }
    unsigned char chunk[65536];
    bool ok = true;
    for (size_t offset = 0; ok && offset < BIG_FILE_SIZE;
         offset += sizeof chunk) {
        for (size_t i = 0; i < sizeof chunk; i++) {
            chunk[i] = big_byte(offset + i);
        }
        ok = fwrite(chunk, 1, sizeof chunk, fp) == sizeof chunk;
    }
    return fclose(fp) == 0 && ok;
}

static bool make_fixture(void) {
    const char *tmp = getenv("TMPDIR");
    snprintf(work_dir, sizeof work_dir, "%s/mdr_server_test.XXXXXX",
             tmp != NULL && tmp[0] != '\0' ? tmp : "/tmp");
    if (mkdtemp(work_dir) == NULL) {
        perror("mkdtemp");
        return false;
    }
    for (size_t i = 0; i < sizeof fixture_dirs / sizeof fixture_dirs[0]; i++) {
        if (mkdir(in_work_dir(fixture_dirs[i]), 0755) < 0) {
            perror("mkdir");
            return false;
        }
    }
    return write_file("root/index.html", INDEX_HTML, strlen(INDEX_HTML)) &&
           write_file("root/hello.txt", HELLO_TXT, strlen(HELLO_TXT)) &&
           write_file("root/docs/index.html", DOCS_INDEX, strlen(DOCS_INDEX)) &&
           write_file("root/.secret", SECRET_TXT, strlen(SECRET_TXT)) &&
           write_file("root/\xE4\xB8\xAD\xE6\x96\x87.txt", UTF8_TXT,
                      strlen(UTF8_TXT)) &&
           write_file("outside.txt", OUTSIDE_TXT, strlen(OUTSIDE_TXT)) &&
           write_big_file();
}

static void remove_fixture(void) {
    for (size_t i = 0; i < sizeof fixture_files / sizeof fixture_files[0];
         i++) {
        unlink(in_work_dir(fixture_files[i]));
    }
    for (size_t i = sizeof fixture_dirs / sizeof fixture_dirs[0]; i > 0; i--) {
        rmdir(in_work_dir(fixture_dirs[i - 1]));
    }
    rmdir(work_dir);
}

static void print_server_log(void) {
    FILE *fp = fopen(in_work_dir("server.log"), "r");
    if (fp == NULL) {
        return;
    }
    fprintf(stderr, "---- server log ----\n");
    char line[1024];
    while (fgets(line, sizeof line, fp) != NULL) {
        fputs(line, stderr);
    }
    fprintf(stderr, "--------------------\n");
    fclose(fp);
}

// ---------------------------------------------------------------------------
// 客户端

struct client {
    int fd;
    size_t length; // buf 中已收到、尚未取走的字节数
    char buf[16384];
};

// 连接服务器。rcvbuf 大于 0 时先缩小接收缓冲区，用来模拟读得慢的客户端。
static bool client_connect(struct client *c, int rcvbuf) {
    c->length = 0;
    c->fd = socket(AF_INET, SOCK_STREAM, 0);
    if (c->fd < 0) {
        return fail("socket: %s", strerror(errno));
    }
    if (rcvbuf > 0) {
        setsockopt(c->fd, SOL_SOCKET, SO_RCVBUF, &rcvbuf, sizeof rcvbuf);
    }
    int on = 1;
    setsockopt(c->fd, IPPROTO_TCP, TCP_NODELAY, &on, sizeof on);
    // 每次 recv、send 最多等 3 秒，服务器出了问题测试也不会卡住。
    struct timeval timeout = {3, 0};
    setsockopt(c->fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof timeout);
    setsockopt(c->fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof timeout);

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof addr);
    addr.sin_family = AF_INET;
    addr.sin_port = htons((uint16_t)server_port);
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    if (connect(c->fd, (struct sockaddr *)&addr, sizeof addr) < 0) {
        int err = errno;
        close(c->fd);
        c->fd = -1;
        errno = err;
        return fail("connect: %s", strerror(err));
    }
    return true;
}

static void client_close(struct client *c) {
    if (c->fd >= 0) {
        close(c->fd);
        c->fd = -1;
    }
}

static bool send_bytes(struct client *c, const void *data, size_t length) {
    const char *p = data;
    while (length > 0) {
        ssize_t n = send(c->fd, p, length, 0);
        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }
            return fail("send: %s", strerror(errno));
        }
        p += n;
        length -= (size_t)n;
    }
    return true;
}

static bool send_text(struct client *c, const char *text) {
    return send_bytes(c, text, strlen(text));
}

static const char *recv_error(void) {
    return errno == EAGAIN || errno == EWOULDBLOCK ? "timed out"
                                                   : strerror(errno);
}

static bool client_recv_more(struct client *c) {
    if (c->length == sizeof c->buf) {
        return fail("response head too large");
    }
    ssize_t n = recv(c->fd, c->buf + c->length, sizeof c->buf - c->length, 0);
    if (n > 0) {
        c->length += (size_t)n;
        return true;
    }
    if (n == 0) {
        return fail("connection closed by the server");
    }
    return fail("recv: %s", recv_error());
}

static void client_consume(struct client *c, size_t n) {
    memmove(c->buf, c->buf + n, c->length - n);
    c->length -= n;
}

struct response {
    int status;
    char head[4096];
    uint64_t content_length;
    char *body;
    size_t body_length;
};

static void response_free(struct response *r) {
    free(r->body);
    r->body = NULL;
}

// 在响应头里找名为 name 的字段（不区分大小写），把值复制到 out。
static bool find_header(const struct response *r, const char *name, char *out,
                        size_t size) {
    size_t name_length = strlen(name);
    const char *line = strstr(r->head, "\r\n");
    while (line != NULL) {
        line += 2;
        if (strncasecmp(line, name, name_length) == 0 &&
            line[name_length] == ':') {
            const char *value = line + name_length + 1;
            while (*value == ' ') {
                value++;
            }
            size_t length = strcspn(value, "\r");
            if (length >= size) {
                length = size - 1;
            }
            memcpy(out, value, length);
            out[length] = '\0';
            return true;
        }
        line = strstr(line, "\r\n");
    }
    return false;
}

static size_t find_head_end(const char *data, size_t length) {
    for (size_t i = 0; i + 3 < length; i++) {
        if (memcmp(data + i, "\r\n\r\n", 4) == 0) {
            return i + 4;
        }
    }
    return 0;
}

// 读一个完整的响应。HEAD 请求的响应没有正文，要由调用者告诉这里。
static bool read_response(struct client *c, bool head_request,
                          struct response *r) {
    response_free(r);
    memset(r, 0, sizeof *r);
    size_t head_length;
    while ((head_length = find_head_end(c->buf, c->length)) == 0) {
        if (!client_recv_more(c)) {
            return false;
        }
    }
    if (head_length >= sizeof r->head) {
        return fail("response head too large");
    }
    memcpy(r->head, c->buf, head_length);
    r->head[head_length] = '\0';
    client_consume(c, head_length);

    if (sscanf(r->head, "HTTP/1.1 %d ", &r->status) != 1) {
        return fail("bad status line: %.40s", r->head);
    }
    char value[64];
    if (!find_header(r, "Content-Length", value, sizeof value)) {
        return fail("response without Content-Length");
    }
    r->content_length = strtoull(value, NULL, 10);
    if (head_request) {
        return true;
    }

    r->body = malloc(r->content_length + 1);
    if (r->body == NULL) {
        return fail("out of memory");
    }
    // 先取走缓冲区里已经收到的正文，其余的直接收进 body。
    size_t have =
        c->length < r->content_length ? c->length : (size_t)r->content_length;
    memcpy(r->body, c->buf, have);
    client_consume(c, have);
    r->body_length = have;
    while (r->body_length < r->content_length) {
        ssize_t n = recv(c->fd, r->body + r->body_length,
                         r->content_length - r->body_length, 0);
        if (n == 0) {
            return fail("connection closed after %zu of %llu body bytes",
                        r->body_length, (unsigned long long)r->content_length);
        }
        if (n < 0) {
            return fail("recv body: %s", recv_error());
        }
        r->body_length += (size_t)n;
    }
    r->body[r->body_length] = '\0';
    return true;
}

static bool request(struct client *c, const char *text, bool head_request,
                    struct response *r) {
    return send_text(c, text) && read_response(c, head_request, r);
}

static bool expect_status(const struct response *r, int status) {
    return r->status == status ||
           fail("expected status %d, got %d", status, r->status);
}

static bool expect_header(const struct response *r, const char *name,
                          const char *expected) {
    char value[256];
    if (!find_header(r, name, value, sizeof value)) {
        return fail("missing header %s", name);
    }
    return strcmp(value, expected) == 0 ||
           fail("%s: expected \"%s\", got \"%s\"", name, expected, value);
}

static bool expect_body(const struct response *r, const char *expected) {
    size_t length = strlen(expected);
    return (r->body_length == length &&
            memcmp(r->body, expected, length) == 0) ||
           fail("unexpected body (%zu bytes): \"%.60s\"", r->body_length,
                r->body != NULL ? r->body : "");
}

static bool expect_big_body(const struct response *r) {
    if (r->body_length != BIG_FILE_SIZE) {
        return fail("big.bin: got %zu bytes", r->body_length);
    }
    for (size_t i = 0; i < r->body_length; i++) {
        if ((unsigned char)r->body[i] != big_byte(i)) {
            return fail("big.bin: wrong byte at offset %zu", i);
        }
    }
    return true;
}

// 服务器应该已经关闭了连接：recv 返回 0，之前也没有多余的数据。
static bool expect_eof(struct client *c) {
    if (c->length > 0) {
        return fail("unexpected data after the response");
    }
    char byte;
    ssize_t n = recv(c->fd, &byte, 1, 0);
    if (n == 0) {
        return true;
    }
    if (n > 0) {
        return fail("unexpected data after the response");
    }
    return fail("expected EOF: %s", recv_error());
}

// 在 ms 毫秒内应该收不到任何数据。
static bool expect_silence(struct client *c, int ms) {
    struct pollfd pfd = {.fd = c->fd, .events = POLLIN};
    int n = poll(&pfd, 1, ms);
    return n == 0 || fail("expected no response yet");
}

// ---------------------------------------------------------------------------
// 启动和停止服务器

static bool start_server(void) {
    char root[PATH_MAX + 64];
    snprintf(root, sizeof root, "%s", in_work_dir("root"));
    int log_fd = open(in_work_dir("server.log"),
                      O_WRONLY | O_CREAT | O_APPEND | O_CLOEXEC, 0644);
    if (log_fd < 0) {
        perror("open server.log");
        return false;
    }

    char port[16];
    char max_connections[16];
    char max_request[16];
    char idle_timeout[16];
    snprintf(port, sizeof port, "%d", server_port);
    snprintf(max_connections, sizeof max_connections, "%d",
             TEST_MAX_CONNECTIONS);
    snprintf(max_request, sizeof max_request, "%d", TEST_MAX_REQUEST);
    snprintf(idle_timeout, sizeof idle_timeout, "%d", TEST_IDLE_TIMEOUT_MS);

    pid_t parent = getpid();
    pid_t pid = fork();
    if (pid < 0) {
        perror("fork");
        close(log_fd);
        return false;
    }
    if (pid == 0) {
        // 测试进程一旦退出，内核就向服务器发 SIGKILL。如果父进程在
        // prctl 之前就已经退出，就不会再有这个信号，所以要再检查一次。
        prctl(PR_SET_PDEATHSIG, SIGKILL);
        if (getppid() != parent) {
            _exit(127);
        }
        dup2(log_fd, STDOUT_FILENO);
        dup2(log_fd, STDERR_FILENO);
        execl(server_path, server_path, "-p", port, "-r", root, "-c",
              max_connections, "-m", max_request, "-t", idle_timeout, "-v",
              (char *)NULL);
        _exit(127);
    }
    close(log_fd);
    server_pid = pid;

    // 等服务器开始监听：反复尝试连接，最多 3 秒。
    for (int i = 0; i < 300; i++) {
        int status;
        if (waitpid(pid, &status, WNOHANG) == pid) {
            server_pid = -1;
            fprintf(stderr, "server exited during startup\n");
            return false;
        }
        struct client probe;
        if (client_connect(&probe, 0)) {
            client_close(&probe);
            return true;
        }
        sleep_ms(10);
    }
    fprintf(stderr, "server did not start listening\n");
    return false;
}

// 等服务器进程退出，最多 timeout_ms 毫秒；返回它的退出状态，超时返回 -1。
static int wait_server_exit(int timeout_ms) {
    int64_t deadline = now_ms() + timeout_ms;
    while (now_ms() < deadline) {
        int status;
        if (waitpid(server_pid, &status, WNOHANG) == server_pid) {
            server_pid = -1;
            return status;
        }
        sleep_ms(10);
    }
    return -1;
}

static void stop_server(void) {
    if (server_pid > 0) {
        kill(server_pid, SIGKILL);
        waitpid(server_pid, NULL, 0);
        server_pid = -1;
    }
}

// ---------------------------------------------------------------------------
// 测试用例

// 新建一个连接，发送 text，读回一个响应。
static bool fetch(struct client *c, const char *text, bool head_request,
                  struct response *r) {
    return client_connect(c, 0) && request(c, text, head_request, r);
}

static bool test_get_index(void) {
    struct client c = {.fd = -1};
    struct response r = {0};
    bool ok = fetch(&c, "GET / HTTP/1.1\r\nHost: t\r\n\r\n", false, &r) &&
              expect_status(&r, 200) &&
              expect_header(&r, "Content-Type", "text/html; charset=utf-8") &&
              expect_header(&r, "Connection", "keep-alive") &&
              expect_body(&r, INDEX_HTML);
    response_free(&r);
    client_close(&c);
    return ok;
}

// HEAD 只回响应头，Content-Length 是文件长度。之后同一连接上的请求照常
// 工作，说明服务器没有多发正文。
static bool test_head(void) {
    struct client c = {.fd = -1};
    struct response r = {0};
    char length[16];
    snprintf(length, sizeof length, "%zu", strlen(HELLO_TXT));
    bool ok =
        fetch(&c, "HEAD /hello.txt HTTP/1.1\r\nHost: t\r\n\r\n", true, &r) &&
        expect_status(&r, 200) && expect_header(&r, "Content-Length", length) &&
        request(&c, "GET /hello.txt HTTP/1.1\r\nHost: t\r\n\r\n", false, &r) &&
        expect_body(&r, HELLO_TXT);
    response_free(&r);
    client_close(&c);
    return ok;
}

static bool test_not_found_keeps_connection(void) {
    struct client c = {.fd = -1};
    struct response r = {0};
    bool ok =
        fetch(&c, "GET /nope.txt HTTP/1.1\r\nHost: t\r\n\r\n", false, &r) &&
        expect_status(&r, 404) &&
        expect_header(&r, "Connection", "keep-alive") &&
        request(&c, "GET /hello.txt HTTP/1.1\r\nHost: t\r\n\r\n", false, &r) &&
        expect_status(&r, 200);
    response_free(&r);
    client_close(&c);
    return ok;
}

static bool test_utf8_file_name(void) {
    struct client c = {.fd = -1};
    struct response r = {0};
    bool ok =
        fetch(&c, "GET /%E4%B8%AD%E6%96%87.txt HTTP/1.1\r\nHost: t\r\n\r\n",
              false, &r) &&
        expect_status(&r, 200) && expect_body(&r, UTF8_TXT);
    response_free(&r);
    client_close(&c);
    return ok;
}

// 文档根目录外的文件（outside.txt 在根目录的上一级）无论怎样拼写路径
// 都拿不到。
static bool test_path_traversal(void) {
    char absolute[PATH_MAX + 64];
    snprintf(absolute, sizeof absolute, "/%s/outside.txt", work_dir);
    struct {
        const char *target;
        int status;
    } cases[] = {
        {"/../outside.txt", 403},
        {"/%2e%2e/outside.txt", 403},
        {"/docs/../../outside.txt", 403},
        {"/.secret", 403},
        // 多余的 '/' 被去掉，路径仍相对文档根目录解析。
        {"//outside.txt", 404},
        {absolute, 404},
    };
    for (size_t i = 0; i < sizeof cases / sizeof cases[0]; i++) {
        char text[PATH_MAX + 128];
        snprintf(text, sizeof text, "GET %s HTTP/1.1\r\nHost: t\r\n\r\n",
                 cases[i].target);
        struct client c = {.fd = -1};
        struct response r = {0};
        bool ok =
            fetch(&c, text, false, &r) && expect_status(&r, cases[i].status) &&
            (strstr(r.body, "outside") == NULL || fail("leaked outside.txt"));
        response_free(&r);
        client_close(&c);
        if (!ok) {
            char reason[sizeof failure_reason];
            memcpy(reason, failure_reason, sizeof reason);
            return fail("%s: %s", cases[i].target, reason);
        }
    }
    return true;
}

// 目录的地址不以 '/' 结尾时回 301。Location 去掉开头多余的 '/'（浏览器会把
// "//docs/" 当成另一个网站的地址），查询串照原样带上。
static bool test_directory_redirect(void) {
    struct client c = {.fd = -1};
    struct response r = {0};
    bool ok =
        fetch(&c, "GET /docs HTTP/1.1\r\nHost: t\r\n\r\n", false, &r) &&
        expect_status(&r, 301) && expect_header(&r, "Location", "/docs/") &&
        request(&c, "GET //docs?x=1 HTTP/1.1\r\nHost: t\r\n\r\n", false, &r) &&
        expect_status(&r, 301) && expect_header(&r, "Location", "/docs/?x=1") &&
        request(&c, "GET /docs/ HTTP/1.1\r\nHost: t\r\n\r\n", false, &r) &&
        expect_status(&r, 200) && expect_body(&r, DOCS_INDEX);
    response_free(&r);
    client_close(&c);
    return ok;
}

static bool test_keep_alive(void) {
    struct client c = {.fd = -1};
    struct response r = {0};
    bool ok = client_connect(&c, 0);
    for (int i = 0; ok && i < 3; i++) {
        ok = request(&c, "GET /hello.txt HTTP/1.1\r\nHost: t\r\n\r\n", false,
                     &r) &&
             expect_status(&r, 200) && expect_body(&r, HELLO_TXT);
    }
    response_free(&r);
    client_close(&c);
    return ok;
}

// 三个请求一次发出，三个响应按顺序回来。
static bool test_pipelining(void) {
    struct client c = {.fd = -1};
    struct response r = {0};
    char length[16];
    snprintf(length, sizeof length, "%zu", strlen(INDEX_HTML));
    bool ok = client_connect(&c, 0) &&
              send_text(&c, "GET /hello.txt HTTP/1.1\r\nHost: t\r\n\r\n"
                            "GET /docs/ HTTP/1.1\r\nHost: t\r\n\r\n"
                            "HEAD / HTTP/1.1\r\nHost: t\r\n\r\n") &&
              read_response(&c, false, &r) && expect_body(&r, HELLO_TXT) &&
              read_response(&c, false, &r) && expect_body(&r, DOCS_INDEX) &&
              read_response(&c, true, &r) &&
              expect_header(&r, "Content-Length", length);
    response_free(&r);
    client_close(&c);
    return ok;
}

static bool test_connection_close(void) {
    struct client c = {.fd = -1};
    struct response r = {0};
    bool ok = fetch(&c,
                    "GET /hello.txt HTTP/1.1\r\nHost: t\r\n"
                    "Connection: close\r\n\r\n",
                    false, &r) &&
              expect_status(&r, 200) &&
              expect_header(&r, "Connection", "close") &&
              expect_body(&r, HELLO_TXT) && expect_eof(&c);
    response_free(&r);
    client_close(&c);
    return ok;
}

static bool test_http10(void) {
    struct client c = {.fd = -1};
    struct response r = {0};
    // HTTP/1.0 默认不保持连接。
    bool ok = fetch(&c, "GET /hello.txt HTTP/1.0\r\n\r\n", false, &r) &&
              expect_status(&r, 200) &&
              expect_header(&r, "Connection", "close") && expect_eof(&c);
    client_close(&c);
    // 明确要求 keep-alive 时保持连接。
    ok = ok &&
         fetch(&c, "GET /hello.txt HTTP/1.0\r\nConnection: keep-alive\r\n\r\n",
               false, &r) &&
         expect_header(&r, "Connection", "keep-alive") &&
         request(&c, "GET / HTTP/1.0\r\nConnection: keep-alive\r\n\r\n", false,
                 &r) &&
         expect_body(&r, INDEX_HTML);
    response_free(&r);
    client_close(&c);
    return ok;
}

static bool test_bad_requests(void) {
    struct {
        const char *text;
        int status;
    } cases[] = {
        {"GET / HTTP/1.1\r\n\r\n", 400}, // HTTP/1.1 必须有 Host
        {"HELLO\r\n\r\n", 400},
        {"GET / HTTP/2.0\r\nHost: t\r\n\r\n", 505},
        {"DELETE /hello.txt HTTP/1.1\r\nHost: t\r\n\r\n", 501},
    };
    for (size_t i = 0; i < sizeof cases / sizeof cases[0]; i++) {
        struct client c = {.fd = -1};
        struct response r = {0};
        bool ok = fetch(&c, cases[i].text, false, &r) &&
                  expect_status(&r, cases[i].status);
        // 无法解析的请求之后找不到下一个请求的开头，服务器关闭连接。
        if (ok && cases[i].status != 501) {
            ok = expect_header(&r, "Connection", "close") && expect_eof(&c);
        }
        response_free(&r);
        client_close(&c);
        if (!ok) {
            return false;
        }
    }
    return true;
}

// POST 带着 1 MiB 的请求体。服务器不读请求体，回 501 后关闭连接。分阶段
// 关闭保证了客户端能把请求体发完、读到 501；如果服务器直接 close，接收
// 缓冲区里没读的数据会让内核回 RST，客户端的 send 会以 ECONNRESET 失败。
static bool test_unread_body(void) {
    size_t body_length = 1024 * 1024;
    char *body = malloc(body_length);
    if (body == NULL) {
        return fail("out of memory");
    }
    memset(body, 'x', body_length);
    char head[128];
    snprintf(head, sizeof head,
             "POST /hello.txt HTTP/1.1\r\nHost: t\r\n"
             "Content-Length: %zu\r\n\r\n",
             body_length);

    struct client c = {.fd = -1};
    struct response r = {0};
    bool ok = client_connect(&c, 0) && send_text(&c, head) &&
              send_bytes(&c, body, body_length) &&
              read_response(&c, false, &r) && expect_status(&r, 501) &&
              expect_header(&r, "Connection", "close") && expect_eof(&c);
    free(body);
    response_free(&r);
    client_close(&c);
    return ok;
}

static bool test_get_with_body(void) {
    struct client c = {.fd = -1};
    struct response r = {0};
    bool ok = fetch(&c,
                    "GET /hello.txt HTTP/1.1\r\nHost: t\r\n"
                    "Content-Length: 5\r\n\r\nhello",
                    false, &r) &&
              expect_status(&r, 200) && expect_body(&r, HELLO_TXT) &&
              expect_header(&r, "Connection", "close") && expect_eof(&c);
    response_free(&r);
    client_close(&c);
    return ok;
}

static bool test_uri_too_long(void) {
    char text[3000];
    int n = snprintf(text, sizeof text, "GET /");
    memset(text + n, 'a', 2100);
    snprintf(text + n + 2100, sizeof text - (size_t)n - 2100,
             " HTTP/1.1\r\nHost: t\r\n\r\n");
    struct client c = {.fd = -1};
    struct response r = {0};
    bool ok =
        fetch(&c, text, false, &r) && expect_status(&r, 414) && expect_eof(&c);
    response_free(&r);
    client_close(&c);
    return ok;
}

// 请求头超过输入缓冲区（-m 4096）时回 431。
static bool test_header_too_large(void) {
    char text[8192];
    int n =
        snprintf(text, sizeof text, "GET / HTTP/1.1\r\nHost: t\r\nX-Fill: ");
    memset(text + n, 'x', 6000);
    snprintf(text + n + 6000, sizeof text - (size_t)n - 6000, "\r\n\r\n");
    struct client c = {.fd = -1};
    struct response r = {0};
    bool ok =
        fetch(&c, text, false, &r) && expect_status(&r, 431) && expect_eof(&c);
    response_free(&r);
    client_close(&c);
    return ok;
}

static bool test_big_file(void) {
    struct client c = {.fd = -1};
    struct response r = {0};
    bool ok =
        fetch(&c, "GET /big.bin HTTP/1.1\r\nHost: t\r\n\r\n", false, &r) &&
        expect_status(&r, 200) &&
        expect_header(&r, "Content-Type", "application/octet-stream") &&
        expect_big_body(&r);
    response_free(&r);
    client_close(&c);
    return ok;
}

// 请求头一个字节一个字节地到达，服务器要能把它们攒成完整的请求。
static bool test_slow_request(void) {
    const char *text = "GET /hello.txt HTTP/1.1\r\nHost: t\r\n\r\n";
    struct client c = {.fd = -1};
    struct response r = {0};
    bool ok = client_connect(&c, 0);
    for (const char *p = text; ok && *p != '\0'; p++) {
        ok = send_bytes(&c, p, 1);
        sleep_ms(2);
    }
    ok = ok && read_response(&c, false, &r) && expect_status(&r, 200) &&
         expect_body(&r, HELLO_TXT);
    response_free(&r);
    client_close(&c);
    return ok;
}

// 一个客户端请求大文件后暂时不读，服务器写满套接字缓冲区，遇到 EAGAIN。
// 这时事件循环不能被它卡住：另一个客户端的请求应该立即得到回应。
static bool test_stalled_reader(void) {
    struct client big = {.fd = -1};
    struct client small = {.fd = -1};
    struct response r = {0};
    bool ok = client_connect(&big, 16384) &&
              send_text(&big, "GET /big.bin HTTP/1.1\r\nHost: t\r\n\r\n");
    sleep_ms(200);
    int64_t start = now_ms();
    ok = ok &&
         fetch(&small, "GET /hello.txt HTTP/1.1\r\nHost: t\r\n\r\n", false,
               &r) &&
         expect_body(&r, HELLO_TXT);
    int64_t elapsed = now_ms() - start;
    if (ok && elapsed > 500) {
        ok = fail("small request took %lld ms", (long long)elapsed);
    }
    // 现在把大文件读完，逐字节核对。
    ok = ok && read_response(&big, false, &r) && expect_status(&r, 200) &&
         expect_big_body(&r);
    response_free(&r);
    client_close(&small);
    client_close(&big);
    return ok;
}

// 连上之后什么也不发，服务器应在空闲超时之后关闭连接。
static bool test_idle_timeout(void) {
    struct client c = {.fd = -1};
    if (!client_connect(&c, 0)) {
        return false;
    }
    int64_t start = now_ms();
    bool ok = expect_eof(&c);
    int64_t elapsed = now_ms() - start;
    if (ok && (elapsed < TEST_IDLE_TIMEOUT_MS - 100 ||
               elapsed > TEST_IDLE_TIMEOUT_MS + 1000)) {
        ok = fail("closed after %lld ms", (long long)elapsed);
    }
    client_close(&c);
    return ok;
}

// 连接池满了以后，新连接在内核的接受队列里等待；有连接关闭、腾出槽位
// 之后，它的请求才得到处理。
static bool test_connection_limit(void) {
    struct client held[TEST_MAX_CONNECTIONS];
    struct client extra = {.fd = -1};
    struct response r = {0};
    for (int i = 0; i < TEST_MAX_CONNECTIONS; i++) {
        held[i].fd = -1;
    }
    sleep_ms(100); // 等前面测试的连接都被服务器回收
    bool ok = true;
    for (int i = 0; ok && i < TEST_MAX_CONNECTIONS; i++) {
        // 每个连接先完成一个请求，确保服务器已经接受它、占用了一个槽位。
        ok = fetch(&held[i], "GET /hello.txt HTTP/1.1\r\nHost: t\r\n\r\n",
                   false, &r) &&
             expect_status(&r, 200);
    }
    ok = ok && client_connect(&extra, 0) &&
         send_text(&extra, "GET /hello.txt HTTP/1.1\r\nHost: t\r\n\r\n") &&
         expect_silence(&extra, 300);
    client_close(&held[0]);
    ok = ok && read_response(&extra, false, &r) && expect_status(&r, 200);
    response_free(&r);
    client_close(&extra);
    for (int i = 0; i < TEST_MAX_CONNECTIONS; i++) {
        client_close(&held[i]);
    }
    return ok;
}

// SIGTERM：空闲连接立即关闭，监听套接字关闭，正在进行的下载照常完成，
// 最后一个连接关闭后进程以状态 0 退出。
static bool test_graceful_shutdown(void) {
    struct client idle = {.fd = -1};
    struct client big = {.fd = -1};
    struct client late = {.fd = -1};
    struct response r = {0};
    sleep_ms(100);
    bool ok =
        fetch(&idle, "GET /hello.txt HTTP/1.1\r\nHost: t\r\n\r\n", false, &r) &&
        client_connect(&big, 16384) &&
        send_text(&big, "GET /big.bin HTTP/1.1\r\nHost: t\r\n\r\n");
    sleep_ms(100); // 让服务器开始发送大文件
    ok = ok && (kill(server_pid, SIGTERM) == 0 || fail("kill failed")) &&
         expect_eof(&idle);
    if (ok && client_connect(&late, 0)) {
        ok = fail("new connection accepted during shutdown");
    }
    ok = ok && read_response(&big, false, &r) && expect_big_body(&r) &&
         expect_eof(&big);
    client_close(&big);
    if (ok) {
        int status = wait_server_exit(3000);
        if (status < 0) {
            ok = fail("server still running after the last connection");
        } else if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
            ok = fail("server exit status %d", status);
        }
    }
    response_free(&r);
    client_close(&late);
    client_close(&idle);
    return ok;
}

// 第二个 SIGTERM 让服务器立即退出，不再等进行中的下载。客户端一直不读，
// 服务器若还在等这个下载完成，就不可能在 1 秒内退出。
static bool test_forced_shutdown(void) {
    stop_server();
    if (!start_server()) {
        return fail("cannot restart the server");
    }
    struct client big = {.fd = -1};
    bool ok = client_connect(&big, 16384) &&
              send_text(&big, "GET /big.bin HTTP/1.1\r\nHost: t\r\n\r\n");
    sleep_ms(100);
    ok = ok && kill(server_pid, SIGTERM) == 0;
    sleep_ms(100); // 两个信号挨得太近会被合并成一个
    ok = ok && kill(server_pid, SIGTERM) == 0;
    if (ok) {
        int status = wait_server_exit(1000);
        if (status < 0) {
            ok = fail("server ignored the second SIGTERM");
        } else if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
            ok = fail("server exit status %d", status);
        }
    }
    client_close(&big);
    return ok;
}

// ---------------------------------------------------------------------------

struct test_case {
    const char *name;
    bool (*run)(void);
};

static const struct test_case tests[] = {
    {"GET / serves index.html", test_get_index},
    {"HEAD sends headers only", test_head},
    {"404 keeps the connection open", test_not_found_keeps_connection},
    {"percent-encoded UTF-8 file name", test_utf8_file_name},
    {"no access outside the document root", test_path_traversal},
    {"directory without '/' is redirected", test_directory_redirect},
    {"keep-alive: three requests on one connection", test_keep_alive},
    {"pipelined requests answered in order", test_pipelining},
    {"Connection: close", test_connection_close},
    {"HTTP/1.0 with and without keep-alive", test_http10},
    {"malformed and unsupported requests", test_bad_requests},
    {"unread request body: lingering close", test_unread_body},
    {"GET with a body closes afterwards", test_get_with_body},
    {"414 for a long request target", test_uri_too_long},
    {"431 for an oversized request head", test_header_too_large},
    {"8 MiB file arrives intact", test_big_file},
    {"request arriving byte by byte", test_slow_request},
    {"stalled reader does not block others", test_stalled_reader},
    {"idle connection is closed after the timeout", test_idle_timeout},
    {"connection limit pauses accept", test_connection_limit},
    {"graceful shutdown on SIGTERM", test_graceful_shutdown},
    {"second SIGTERM exits immediately", test_forced_shutdown},
};

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <mdr_server> <port>\n", argv[0]);
        return 2;
    }
    server_path = argv[1];
    server_port = atoi(argv[2]);
    // 服务器关闭连接后再 send，不应让测试进程被 SIGPIPE 杀死。
    signal(SIGPIPE, SIG_IGN);

    if (!make_fixture()) {
        fprintf(stderr, "cannot create test files under %s\n", work_dir);
        remove_fixture();
        return 1;
    }
    int passed = 0;
    int failed = 0;
    if (start_server()) {
        for (size_t i = 0; i < sizeof tests / sizeof tests[0]; i++) {
            failure_reason[0] = '\0';
            if (tests[i].run()) {
                printf("[ ok ] %s\n", tests[i].name);
                passed++;
            } else {
                printf("[FAIL] %s: %s\n", tests[i].name, failure_reason);
                failed++;
            }
            fflush(stdout);
        }
    } else {
        failed++;
    }
    stop_server();
    if (failed > 0) {
        print_server_log();
    }
    remove_fixture();
    printf("integration tests: %d passed, %d failed\n", passed, failed);
    return failed == 0 ? 0 : 1;
}
