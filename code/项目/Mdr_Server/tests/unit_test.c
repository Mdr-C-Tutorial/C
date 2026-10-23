// 单元测试：缓冲区、定时器队列、HTTP 解析与格式化。
//
// 这些模块只用标准 C，不碰套接字和时钟，所以本测试在任何平台上都能
// 编译运行，也不需要启动服务器。

#include "buffer.h"
#include "http.h"
#include "timer.h"

#include <stdio.h>
#include <string.h>

static int checks;
static int failures;

#define CHECK(cond)                                                            \
    do {                                                                       \
        checks++;                                                              \
        if (!(cond)) {                                                         \
            failures++;                                                        \
            fprintf(stderr, "%s:%d: CHECK failed: %s\n", __FILE__, __LINE__,   \
                    #cond);                                                    \
        }                                                                      \
    } while (0)

#define CHECK_STR(actual, expected)                                            \
    do {                                                                       \
        checks++;                                                              \
        if (strcmp((actual), (expected)) != 0) {                               \
            failures++;                                                        \
            fprintf(stderr, "%s:%d: expected \"%s\", got \"%s\"\n", __FILE__,  \
                    __LINE__, (expected), (actual));                           \
        }                                                                      \
    } while (0)

// ---------------------------------------------------------------------------
// buffer

static void test_buffer(void) {
    struct buffer buf;
    CHECK(buffer_init(&buf, 8));
    CHECK(buffer_length(&buf) == 0);
    CHECK(buffer_space(&buf) == 8);

    CHECK(buffer_append(&buf, "abcdef", 6));
    buffer_consume(&buf, 4);
    CHECK(buffer_length(&buf) == 2);
    CHECK(memcmp(buffer_data(&buf), "ef", 2) == 0);

    // 尾部只剩 2 字节，放不下 3 字节；整理之后就放得下了。
    CHECK(!buffer_append(&buf, "ghi", 3));
    buffer_compact(&buf);
    CHECK(buffer_space(&buf) == 6);
    CHECK(buffer_append(&buf, "ghi", 3));
    CHECK(memcmp(buffer_data(&buf), "efghi", 5) == 0);

    // 全部处理完后回到开头。
    buffer_consume(&buf, 5);
    CHECK(buffer_length(&buf) == 0);
    CHECK(buffer_space(&buf) == 8);

    // 像 read 那样直接写到尾部，再登记长度。
    memcpy(buffer_tail(&buf), "12345678", 8);
    buffer_commit(&buf, 8);
    CHECK(buffer_is_full(&buf));
    buffer_free(&buf);
}

// ---------------------------------------------------------------------------
// timer

static void test_timer_queue(void) {
    struct timer_queue queue;
    timer_queue_init(&queue, 100);
    struct timer a;
    struct timer b;
    struct timer c;
    timer_init(&a, &a);
    timer_init(&b, &b);
    timer_init(&c, &c);

    CHECK(timer_queue_wait_ms(&queue, 0) == -1);
    timer_restart(&queue, &a, 0);  // 100 ms 时到期
    timer_restart(&queue, &b, 10); // 110
    timer_restart(&queue, &c, 20); // 120
    CHECK(timer_queue_wait_ms(&queue, 50) == 50);

    // a 有了新的活动：重新计时后移到队尾，最早到期的变成 b。
    timer_restart(&queue, &a, 30); // 130
    CHECK(timer_queue_wait_ms(&queue, 50) == 60);
    CHECK(timer_queue_pop_expired(&queue, 109) == NULL);
    CHECK(timer_queue_pop_expired(&queue, 125) == &b);
    CHECK(timer_queue_pop_expired(&queue, 125) == &c);
    CHECK(timer_queue_pop_expired(&queue, 125) == NULL);
    CHECK(timer_queue_wait_ms(&queue, 200) == 0); // 已经过期

    timer_cancel(&a);
    timer_cancel(&a); // 重复取消没有影响
    CHECK(!timer_is_active(&a));
    CHECK(timer_queue_wait_ms(&queue, 200) == -1);
}

// ---------------------------------------------------------------------------
// http_parse_request

static enum http_parse_result parse(const char *text,
                                    struct http_request *req) {
    return http_parse_request(text, strlen(text), req);
}

static void test_parse_basic(void) {
    struct http_request req;
    const char *text = "GET /index.html HTTP/1.1\r\n"
                       "Host: example.com\r\n"
                       "User-Agent: test\r\n"
                       "\r\n";
    CHECK(parse(text, &req) == HTTP_PARSE_OK);
    CHECK_STR(req.method, "GET");
    CHECK_STR(req.target, "/index.html");
    CHECK(req.minor_version == 1);
    CHECK(req.keep_alive);
    CHECK(!req.has_body);
    CHECK(req.head_length == strlen(text));

    // 请求头还没收齐，最后的空行也算请求头的一部分。
    CHECK(http_parse_request(text, strlen(text) - 1, &req) ==
          HTTP_PARSE_INCOMPLETE);
    CHECK(http_parse_request(text, 10, &req) == HTTP_PARSE_INCOMPLETE);
}

static void test_parse_pipelined(void) {
    // 两个请求挨在一起：只解析第一个，head_length 指出第二个从哪里开始。
    const char *first = "HEAD /a HTTP/1.1\r\nHost: x\r\n\r\n";
    const char *text = "HEAD /a HTTP/1.1\r\nHost: x\r\n\r\n"
                       "GET /b HTTP/1.1\r\nHost: x\r\n\r\n";
    struct http_request req;
    CHECK(parse(text, &req) == HTTP_PARSE_OK);
    CHECK_STR(req.method, "HEAD");
    CHECK(req.head_length == strlen(first));
    CHECK(parse(text + req.head_length, &req) == HTTP_PARSE_OK);
    CHECK_STR(req.target, "/b");
}

static void test_parse_keep_alive(void) {
    struct http_request req;
    CHECK(parse("GET / HTTP/1.1\r\nHost: x\r\nConnection: close\r\n\r\n",
                &req) == HTTP_PARSE_OK);
    CHECK(!req.keep_alive);

    // HTTP/1.0 默认不保持连接，也不要求 Host。
    CHECK(parse("GET / HTTP/1.0\r\n\r\n", &req) == HTTP_PARSE_OK);
    CHECK(req.minor_version == 0);
    CHECK(!req.keep_alive);

    // 选项不区分大小写，可以和别的选项写在一起。
    CHECK(parse("GET / HTTP/1.0\r\nConnection: Upgrade, Keep-Alive\r\n\r\n",
                &req) == HTTP_PARSE_OK);
    CHECK(req.keep_alive);

    // 头字段名也不区分大小写。
    CHECK(parse("GET / HTTP/1.1\r\nhost: x\r\nCONNECTION: CLOSE\r\n\r\n",
                &req) == HTTP_PARSE_OK);
    CHECK(!req.keep_alive);
}

static void test_parse_body_and_line_endings(void) {
    struct http_request req;
    CHECK(parse("GET / HTTP/1.1\r\nHost: x\r\nContent-Length: 0\r\n\r\n",
                &req) == HTTP_PARSE_OK);
    CHECK(!req.has_body);
    CHECK(parse("GET / HTTP/1.1\r\nHost: x\r\nContent-Length: 12\r\n\r\n",
                &req) == HTTP_PARSE_OK);
    CHECK(req.has_body);
    CHECK(parse("POST / HTTP/1.1\r\nHost: x\r\n"
                "Transfer-Encoding: chunked\r\n\r\n",
                &req) == HTTP_PARSE_OK);
    CHECK(req.has_body);

    // 只用 LF 换行的请求也接受（RFC 9112 §2.2）。
    CHECK(parse("GET /lf HTTP/1.1\nHost: x\n\n", &req) == HTTP_PARSE_OK);
    CHECK_STR(req.target, "/lf");

    // 请求行之前的空行忽略掉，head_length 把它们也算在内。
    const char *blank_first = "\r\n\nGET /x HTTP/1.1\r\nHost: x\r\n\r\n";
    CHECK(parse(blank_first, &req) == HTTP_PARSE_OK);
    CHECK_STR(req.target, "/x");
    CHECK(req.head_length == strlen(blank_first));
    CHECK(parse("\r\n", &req) == HTTP_PARSE_INCOMPLETE);

    // 比 1.1 高的次版本号按 1.1 处理。
    CHECK(parse("GET / HTTP/1.2\r\nHost: x\r\n\r\n", &req) == HTTP_PARSE_OK);
    CHECK(req.minor_version == 1);
}

static void expect_error(const char *text, int status) {
    struct http_request req;
    checks++;
    if (parse(text, &req) != HTTP_PARSE_ERROR || req.error_status != status) {
        failures++;
        fprintf(stderr, "expected error %d for: %s\n", status, text);
    }
}

static void test_parse_errors(void) {
    expect_error("GET / HTTP/1.1\r\n\r\n", 400); // 缺少 Host
    expect_error("GET / HTTP/1.1\r\nHost: a\r\nHost: b\r\n\r\n", 400);
    expect_error("HELLO\r\n\r\n", 400);                      // 不是请求行
    expect_error("GET  / HTTP/1.1\r\nHost: x\r\n\r\n", 400); // 两个空格
    expect_error("GET / HTTP/1.1 \r\nHost: x\r\n\r\n", 400);
    expect_error("GET / HTTX/1.1\r\nHost: x\r\n\r\n", 400);
    expect_error("GET / HTTP/2.0\r\nHost: x\r\n\r\n", 505);
    expect_error("G(T / HTTP/1.1\r\nHost: x\r\n\r\n", 400); // 方法名不是 token
    expect_error("GET /a\x01 HTTP/1.1\r\nHost: x\r\n\r\n", 400);
    expect_error("GET / HTTP/1.1\r\nHost : x\r\n\r\n", 400); // 冒号前有空白
    expect_error("GET / HTTP/1.1\r\nHost: x\r\n folded\r\n\r\n", 400);
    expect_error("GET / HTTP/1.1\r\nHost: x\r\nNo-Colon\r\n\r\n", 400);
    expect_error("GET / HTTP/1.1\r\nHost: x\r\nX: a\rb\r\n\r\n", 400);
    expect_error("GET / HTTP/1.1\r\nHost: x\r\nContent-Length: 1x\r\n\r\n",
                 400);

    // 请求目标太长时回 414。
    static char text[HTTP_MAX_TARGET + 64];
    char long_name[HTTP_MAX_TARGET + 1];
    memset(long_name, 'a', HTTP_MAX_TARGET);
    long_name[HTTP_MAX_TARGET] = '\0';
    snprintf(text, sizeof text, "GET /%s HTTP/1.1\r\nHost: x\r\n\r\n",
             long_name);
    expect_error(text, 414);
}

// ---------------------------------------------------------------------------
// http_target_to_path

static void expect_path(const char *target, const char *expected) {
    char out[256];
    int status = http_target_to_path(target, out, sizeof out);
    checks++;
    if (status != 0 || strcmp(out, expected) != 0) {
        failures++;
        fprintf(stderr, "%s: expected \"%s\", got status %d \"%s\"\n", target,
                expected, status, status == 0 ? out : "");
    }
}

static void expect_path_status(const char *target, int expected) {
    char out[256];
    int status = http_target_to_path(target, out, sizeof out);
    checks++;
    if (status != expected) {
        failures++;
        fprintf(stderr, "%s: expected status %d, got %d\n", target, expected,
                status);
    }
}

static void test_target_to_path(void) {
    expect_path("/", "index.html");
    expect_path("/hello.txt", "hello.txt");
    expect_path("/docs/", "docs/index.html");
    expect_path("/docs", "docs");
    expect_path("/index.html?v=2", "index.html");
    expect_path("/a%20b.txt", "a b.txt");
    expect_path("/%E4%B8%AD%E6%96%87.txt", "\xE4\xB8\xAD\xE6\x96\x87.txt");
    // 多余的 '/' 全部去掉，结果总是相对路径。
    expect_path("//etc/passwd", "etc/passwd");
    expect_path("/a//b///c.txt", "a/b/c.txt");

    // 以 '.' 开头的路径段一律拒绝，解码之后再检查。
    expect_path_status("/../secret", 403);
    expect_path_status("/%2e%2e/secret", 403);
    expect_path_status("/%2E%2E%2Fsecret", 403);
    expect_path_status("/docs/../../secret", 403);
    expect_path_status("/a/./b", 403);
    expect_path_status("/.git/config", 403);

    expect_path_status("/a%00.txt", 400);
    expect_path_status("/a%zz", 400);
    expect_path_status("/a%4", 400);
    expect_path_status("relative", 400);
    expect_path_status("http://example.com/", 400);

    // 输出缓冲区放不下时回 414。
    char small[8];
    CHECK(http_target_to_path("/abcdefgh", small, sizeof small) == 414);
}

// ---------------------------------------------------------------------------
// 响应头与 Content-Type

static void test_format_head(void) {
    char out[512];
    struct http_response resp = {
        .status = 200,
        .content_type = "text/plain; charset=utf-8",
        .content_length = 5,
        .keep_alive = true,
        .date = 784111777, // RFC 9110 中的示例时间
    };
    size_t n = http_format_head(out, sizeof out, &resp);
    const char *expected = "HTTP/1.1 200 OK\r\n"
                           "Date: Sun, 06 Nov 1994 08:49:37 GMT\r\n"
                           "Server: Mdr_Server\r\n"
                           "Content-Type: text/plain; charset=utf-8\r\n"
                           "Content-Length: 5\r\n"
                           "Connection: keep-alive\r\n"
                           "\r\n";
    CHECK(n == strlen(expected));
    CHECK_STR(out, expected);

    struct http_response redirect = {
        .status = 301,
        .location = "/docs/",
        .keep_alive = false,
        .date = 784111777,
    };
    n = http_format_head(out, sizeof out, &redirect);
    CHECK(n > 0);
    CHECK(strstr(out, "HTTP/1.1 301 Moved Permanently\r\n") == out);
    CHECK(strstr(out, "Location: /docs/\r\n") != NULL);
    CHECK(strstr(out, "Connection: close\r\n\r\n") != NULL);
    CHECK(strstr(out, "Content-Type") == NULL);

    // 放不下时返回 0，而不是写出半个响应头。
    CHECK(http_format_head(out, 40, &resp) == 0);
}

static void test_content_type(void) {
    CHECK_STR(http_content_type("index.html"), "text/html; charset=utf-8");
    CHECK_STR(http_content_type("docs/STYLE.CSS"), "text/css; charset=utf-8");
    CHECK_STR(http_content_type("a/b.png"), "image/png");
    CHECK_STR(http_content_type("archive.tar.gz"), "application/octet-stream");
    CHECK_STR(http_content_type("v1.2/README"), "application/octet-stream");
}

int main(void) {
    test_buffer();
    test_timer_queue();
    test_parse_basic();
    test_parse_pipelined();
    test_parse_keep_alive();
    test_parse_body_and_line_endings();
    test_parse_errors();
    test_target_to_path();
    test_format_head();
    test_content_type();

    printf("unit tests: %d checks, %d failed\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
