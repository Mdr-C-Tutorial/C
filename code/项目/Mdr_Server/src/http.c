#include "http.h"

#include <inttypes.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

// ---------------------------------------------------------------------------
// 字符分类
//
// 只认 ASCII，不用 <ctype.h>：那些函数的结果受 locale 影响，而且传入
// 负值的 char（非 ASCII 字节）时行为未定义。

static bool is_digit(char c) { return c >= '0' && c <= '9'; }

static bool is_alpha(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

// token 字符（RFC 9110 §5.6.2）：方法名和头字段名都由它组成。
static bool is_tchar(char c) {
    return is_digit(c) || is_alpha(c) ||
           (c != '\0' && strchr("!#$%&'*+-.^_`|~", c) != NULL);
}

// 可见的 ASCII 字符，不含空格。
static bool is_visible(char c) { return c > ' ' && c < 0x7f; }

// 字段值里允许的空白（OWS）。
static bool is_ows(char c) { return c == ' ' || c == '\t'; }

static char to_lower(char c) {
    return c >= 'A' && c <= 'Z' ? (char)(c - 'A' + 'a') : c;
}

// 长度为 length 的 s 与全小写的 lower 是否相同，不区分大小写。
static bool equals_ignore_case(const char *s, size_t length,
                               const char *lower) {
    if (strlen(lower) != length) {
        return false;
    }
    for (size_t i = 0; i < length; i++) {
        if (to_lower(s[i]) != lower[i]) {
            return false;
        }
    }
    return true;
}

// ---------------------------------------------------------------------------
// 请求解析

// 请求行之前的空行要忽略（RFC 9112 §2.2）：有的客户端在上一个请求之后
// 多发一个 CRLF，用 nc 手工输入请求时也常多按一次回车。返回这些空行
// 占的字节数。
static size_t skip_empty_lines(const char *data, size_t length) {
    size_t i = 0;
    while (i < length) {
        if (data[i] == '\n') {
            i++;
        } else if (data[i] == '\r' && i + 1 < length && data[i + 1] == '\n') {
            i += 2;
        } else {
            break;
        }
    }
    return i;
}

// 找请求头的结尾，即第一个空行，返回到空行末尾为止的字节数；还没收到
// 空行时返回 0。行尾应为 CRLF，按 RFC 9112 §2.2 也接受单独的 LF。
static size_t find_head_end(const char *data, size_t length) {
    for (size_t i = 0; i + 1 < length; i++) {
        if (data[i] != '\n') {
            continue;
        }
        if (data[i + 1] == '\n') {
            return i + 2;
        }
        if (data[i + 1] == '\r' && i + 2 < length && data[i + 2] == '\n') {
            return i + 3;
        }
    }
    return 0;
}

struct line_reader {
    const char *pos;
    const char *end;
};

// 取出下一行（去掉行尾的 CR LF），返回它的长度。调用者先用 find_head_end
// 确认了请求头以空行结尾，并且读到空行就停，所以这里一定找得到 LF。
static size_t read_line(struct line_reader *reader, const char **line) {
    const char *begin = reader->pos;
    const char *lf = memchr(begin, '\n', (size_t)(reader->end - begin));
    size_t length = (size_t)(lf - begin);
    reader->pos = lf + 1;
    if (length > 0 && begin[length - 1] == '\r') {
        length--;
    }
    *line = begin;
    return length;
}

// request-line = method SP request-target SP HTTP-version（RFC 9112 §3）
static int parse_request_line(const char *line, size_t length,
                              struct http_request *req) {
    const char *end = line + length;
    const char *sp1 = memchr(line, ' ', length);
    if (sp1 == NULL) {
        return 400;
    }
    const char *target = sp1 + 1;
    const char *sp2 = memchr(target, ' ', (size_t)(end - target));
    if (sp2 == NULL) {
        return 400;
    }

    size_t method_length = (size_t)(sp1 - line);
    if (method_length == 0 || method_length >= sizeof req->method) {
        return 400;
    }
    for (size_t i = 0; i < method_length; i++) {
        if (!is_tchar(line[i])) {
            return 400;
        }
    }
    memcpy(req->method, line, method_length);
    req->method[method_length] = '\0';

    size_t target_length = (size_t)(sp2 - target);
    if (target_length == 0) {
        return 400;
    }
    for (size_t i = 0; i < target_length; i++) {
        // 不允许空白和控制字符。这也保证了请求目标原样写进日志或
        // Location 头时，不会混进换行之类的字符。
        if (!is_visible(target[i])) {
            return 400;
        }
    }
    if (target_length > HTTP_MAX_TARGET) {
        return 414;
    }
    memcpy(req->target, target, target_length);
    req->target[target_length] = '\0';

    // HTTP-version = "HTTP/" DIGIT "." DIGIT
    const char *version = sp2 + 1;
    if (end - version != 8 || memcmp(version, "HTTP/", 5) != 0 ||
        !is_digit(version[5]) || version[6] != '.' || !is_digit(version[7])) {
        return 400;
    }
    if (version[5] != '1') {
        return 505;
    }
    // 比 1.1 高的次版本号按 1.1 处理（RFC 9110 §2.5）。
    req->minor_version = version[7] == '0' ? 0 : 1;
    return 0;
}

// 解析头字段时记下的、之后要用的信息。其余的头字段读过就扔。
struct header_summary {
    int host_count;
    bool connection_close;
    bool connection_keep_alive;
    bool has_body;
};

// Connection 头的值是逗号分隔的选项列表，例如 "keep-alive, Upgrade"。
static void scan_connection_options(const char *value, size_t length,
                                    struct header_summary *summary) {
    size_t i = 0;
    while (i < length) {
        while (i < length && (value[i] == ',' || is_ows(value[i]))) {
            i++;
        }
        size_t begin = i;
        while (i < length && value[i] != ',' && !is_ows(value[i])) {
            i++;
        }
        if (equals_ignore_case(value + begin, i - begin, "close")) {
            summary->connection_close = true;
        } else if (equals_ignore_case(value + begin, i - begin, "keep-alive")) {
            summary->connection_keep_alive = true;
        }
    }
}

// field-line = field-name ":" OWS field-value OWS（RFC 9112 §5）
static int parse_header_line(const char *line, size_t length,
                             struct header_summary *summary) {
    // 以空白开头的行是过时的折行写法（obs-fold），RFC 9112 §5.2 允许
    // 服务器直接拒绝。
    if (is_ows(line[0])) {
        return 400;
    }
    const char *colon = memchr(line, ':', length);
    if (colon == NULL || colon == line) {
        return 400;
    }
    size_t name_length = (size_t)(colon - line);
    for (size_t i = 0; i < name_length; i++) {
        // 这里也拒绝了字段名和冒号之间的空白（RFC 9112 §5.1）。各个实现
        // 对 "Host : x" 这类写法的理解不一致，就会给请求走私留下机会。
        if (!is_tchar(line[i])) {
            return 400;
        }
    }

    const char *value = colon + 1;
    const char *end = line + length;
    for (const char *p = value; p < end; p++) {
        unsigned char byte = (unsigned char)*p;
        // 控制字符（包括单独的 CR 和 NUL）不能出现在字段值里。
        if ((byte < 0x20 && byte != '\t') || byte == 0x7f) {
            return 400;
        }
    }
    while (value < end && is_ows(*value)) {
        value++;
    }
    while (end > value && is_ows(end[-1])) {
        end--;
    }
    size_t value_length = (size_t)(end - value);

    if (equals_ignore_case(line, name_length, "host")) {
        summary->host_count++;
    } else if (equals_ignore_case(line, name_length, "connection")) {
        scan_connection_options(value, value_length, summary);
    } else if (equals_ignore_case(line, name_length, "content-length")) {
        if (value_length == 0) {
            return 400;
        }
        for (size_t i = 0; i < value_length; i++) {
            if (!is_digit(value[i])) {
                return 400;
            }
            if (value[i] != '0') {
                summary->has_body = true;
            }
        }
    } else if (equals_ignore_case(line, name_length, "transfer-encoding")) {
        summary->has_body = true;
    }
    return 0;
}

static enum http_parse_result fail(struct http_request *req, int status) {
    req->error_status = status;
    req->keep_alive = false;
    return HTTP_PARSE_ERROR;
}

enum http_parse_result http_parse_request(const char *data, size_t length,
                                          struct http_request *req) {
    req->method[0] = '\0';
    req->target[0] = '\0';
    req->minor_version = 0;
    req->keep_alive = false;
    req->has_body = false;
    req->head_length = 0;
    req->error_status = 0;

    // 先确认整个请求头都到了再解析。请求头的总长度受输入缓冲区限制，
    // 每次从头扫描的代价可以接受。
    size_t skipped = skip_empty_lines(data, length);
    data += skipped;
    length -= skipped;
    size_t head_length = find_head_end(data, length);
    if (head_length == 0) {
        return HTTP_PARSE_INCOMPLETE;
    }
    req->head_length = skipped + head_length;

    struct line_reader reader = {data, data + head_length};
    const char *line;
    size_t line_length = read_line(&reader, &line);
    int status = parse_request_line(line, line_length, req);
    if (status != 0) {
        return fail(req, status);
    }

    struct header_summary summary = {0};
    while ((line_length = read_line(&reader, &line)) > 0) {
        status = parse_header_line(line, line_length, &summary);
        if (status != 0) {
            return fail(req, status);
        }
    }

    // HTTP/1.1 的请求必须有且只有一个 Host 头（RFC 9112 §3.2）。
    if (summary.host_count > 1 ||
        (req->minor_version == 1 && summary.host_count == 0)) {
        return fail(req, 400);
    }
    // 是否保持连接（RFC 9112 §9.3）：HTTP/1.1 默认保持，除非请求里有
    // "Connection: close"；HTTP/1.0 默认不保持，除非有 "keep-alive"。
    if (summary.connection_close) {
        req->keep_alive = false;
    } else {
        req->keep_alive =
            req->minor_version == 1 || summary.connection_keep_alive;
    }
    req->has_body = summary.has_body;
    return HTTP_PARSE_OK;
}

// ---------------------------------------------------------------------------
// 响应头

const char *http_reason_phrase(int status) {
    switch (status) {
    case 200:
        return "OK";
    case 301:
        return "Moved Permanently";
    case 400:
        return "Bad Request";
    case 403:
        return "Forbidden";
    case 404:
        return "Not Found";
    case 414:
        return "URI Too Long";
    case 431:
        return "Request Header Fields Too Large";
    case 500:
        return "Internal Server Error";
    case 501:
        return "Not Implemented";
    case 505:
        return "HTTP Version Not Supported";
    default:
        return "Unknown";
    }
}

// 在 out[*length] 处追加格式化的文本；放不下时返回 false。
static bool append(char *out, size_t size, size_t *length, const char *format,
                   ...) {
    va_list args;
    va_start(args, format);
    int n = vsnprintf(out + *length, size - *length, format, args);
    va_end(args);
    if (n < 0 || (size_t)n >= size - *length) {
        return false;
    }
    *length += (size_t)n;
    return true;
}

size_t http_format_head(char *out, size_t size,
                        const struct http_response *resp) {
    // Date 头使用 IMF-fixdate 格式，例如 "Sun, 06 Nov 1994 08:49:37 GMT"
    // （RFC 9110 §5.6.7）。程序从不调用 setlocale，%a 和 %b 输出的是英文
    // 缩写。gmtime 返回指向静态对象的指针，本程序是单线程的，不会被别人
    // 改写。
    char date[40];
    const struct tm *tm = gmtime(&resp->date);
    if (size == 0 || tm == NULL ||
        strftime(date, sizeof date, "%a, %d %b %Y %H:%M:%S GMT", tm) == 0) {
        return 0;
    }

    size_t length = 0;
    bool ok =
        append(out, size, &length, "HTTP/1.1 %d %s\r\n", resp->status,
               http_reason_phrase(resp->status)) &&
        append(out, size, &length, "Date: %s\r\nServer: Mdr_Server\r\n", date);
    if (ok && resp->content_type != NULL) {
        ok = append(out, size, &length, "Content-Type: %s\r\n",
                    resp->content_type);
    }
    if (ok && resp->location != NULL) {
        ok = append(out, size, &length, "Location: %s\r\n", resp->location);
    }
    ok = ok &&
         append(out, size, &length, "Content-Length: %" PRIu64 "\r\n",
                resp->content_length) &&
         append(out, size, &length, "Connection: %s\r\n\r\n",
                resp->keep_alive ? "keep-alive" : "close");
    return ok ? length : 0;
}

const char *http_content_type(const char *path) {
    static const struct {
        const char *extension;
        const char *type;
    } types[] = {
        {"html", "text/html; charset=utf-8"},
        {"htm", "text/html; charset=utf-8"},
        {"css", "text/css; charset=utf-8"},
        {"js", "text/javascript; charset=utf-8"},
        {"txt", "text/plain; charset=utf-8"},
        {"md", "text/markdown; charset=utf-8"},
        {"json", "application/json"},
        {"svg", "image/svg+xml"},
        {"png", "image/png"},
        {"jpg", "image/jpeg"},
        {"jpeg", "image/jpeg"},
        {"gif", "image/gif"},
        {"webp", "image/webp"},
        {"ico", "image/x-icon"},
        {"pdf", "application/pdf"},
        {"wasm", "application/wasm"},
    };
    const char *dot = strrchr(path, '.');
    const char *slash = strrchr(path, '/');
    if (dot != NULL && (slash == NULL || dot > slash)) {
        const char *extension = dot + 1;
        for (size_t i = 0; i < sizeof types / sizeof types[0]; i++) {
            if (equals_ignore_case(extension, strlen(extension),
                                   types[i].extension)) {
                return types[i].type;
            }
        }
    }
    return "application/octet-stream";
}

// ---------------------------------------------------------------------------
// 请求目标到文件路径

static int hex_digit(char c) {
    if (c >= '0' && c <= '9') {
        return c - '0';
    }
    if (c >= 'a' && c <= 'f') {
        return c - 'a' + 10;
    }
    if (c >= 'A' && c <= 'F') {
        return c - 'A' + 10;
    }
    return -1;
}

// 在 out 末尾追加一个路径段，段与段之间用 '/' 分隔，并给结尾的 '\0'
// 留出位置。
static bool append_segment(char *out, size_t size, size_t *length,
                           const unsigned char *segment,
                           size_t segment_length) {
    size_t separator = *length > 0 ? 1 : 0;
    if (*length + separator + segment_length >= size) {
        return false;
    }
    if (separator != 0) {
        out[(*length)++] = '/';
    }
    memcpy(out + *length, segment, segment_length);
    *length += segment_length;
    return true;
}

int http_target_to_path(const char *target, char *out, size_t size) {
    // 只接受以 '/' 开头的 origin-form。
    if (size == 0 || target[0] != '/') {
        return 400;
    }

    // 第一步：解码 %XX，到查询串（'?' 之后）为止，静态文件用不到查询串。
    // 必须先解码再检查路径段，否则 "%2e%2e" 就能绕过对 ".." 的检查。
    // 解码结果放在 unsigned char 数组里，大于 127 的字节不必转换成 char。
    unsigned char decoded[HTTP_MAX_TARGET];
    size_t n = 0;
    for (const char *p = target; *p != '\0' && *p != '?'; p++) {
        if (n == sizeof decoded) {
            return 414;
        }
        unsigned char byte = (unsigned char)*p;
        if (byte == '%') {
            int high = hex_digit(p[1]);
            int low = high < 0 ? -1 : hex_digit(p[2]);
            if (low < 0) {
                return 400;
            }
            byte = (unsigned char)(high * 16 + low);
            p += 2;
        }
        // 文件名里不会有 NUL，"%00" 只可能是想截断路径。
        if (byte == '\0') {
            return 400;
        }
        decoded[n++] = byte;
    }

    // 第二步：按 '/' 切成路径段，逐段复制到 out。
    //   * 连续的 '/' 只算一个，开头的 '/' 全部去掉。结果总是相对路径，
    //     openat 才会相对文档根目录去找它。如果只去掉一个 '/'，
    //     "//etc/passwd" 就成了绝对路径 "/etc/passwd"。
    //   * 拒绝以 '.' 开头的段：".." 会跳出文档根目录，".git"、".env"
    //     这类隐藏文件也不应对外提供。
    size_t length = 0;
    size_t i = 0;
    while (i < n) {
        if (decoded[i] == '/') {
            i++;
            continue;
        }
        size_t begin = i;
        while (i < n && decoded[i] != '/') {
            i++;
        }
        if (decoded[begin] == '.') {
            return 403;
        }
        if (!append_segment(out, size, &length, decoded + begin, i - begin)) {
            return 414;
        }
    }

    // 第三步：以 '/' 结尾（包括 "/" 本身）表示目录，补上默认的文件名。
    static const unsigned char index_name[] = "index.html";
    if (decoded[n - 1] == '/' && !append_segment(out, size, &length, index_name,
                                                 sizeof index_name - 1)) {
        return 414;
    }
    out[length] = '\0';
    return 0;
}
