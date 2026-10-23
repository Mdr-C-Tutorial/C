#ifndef MDR_SERVER_HTTP_H
#define MDR_SERVER_HTTP_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <time.h>

// HTTP/1.1 的一个子集：解析请求头、生成响应头、把请求目标映射成文件路径。
// 本模块只处理字符串，不碰套接字和文件，所以在任何平台上都能做单元测试。

// 请求目标（request-target）的最大长度，超过时回 414。
#define HTTP_MAX_TARGET 2048

enum http_parse_result {
    HTTP_PARSE_INCOMPLETE, // 还没收到完整的请求头，需要继续读
    HTTP_PARSE_OK,
    HTTP_PARSE_ERROR, // 请求有误，应回 error_status 并关闭连接
};

struct http_request {
    char method[16];                  // 如 "GET"，区分大小写
    char target[HTTP_MAX_TARGET + 1]; // 如 "/docs/a.html?x=1"，原样保存
    int minor_version;                // HTTP/1.0 为 0，HTTP/1.1 为 1
    bool keep_alive;    // 按 HTTP 版本和 Connection 头判断是否保持连接
    bool has_body;      // 带有非 0 的 Content-Length 或 Transfer-Encoding
    size_t head_length; // 请求头的字节数，含结尾的空行和请求行之前的空行
    int error_status;   // 结果为 HTTP_PARSE_ERROR 时应回的状态码
};

// 从 data 开头解析一个请求头。data 中可以只有请求头的一部分（返回
// HTTP_PARSE_INCOMPLETE），也可以在请求头之后还有后续请求的数据（只解析
// 第一个，它的长度见 head_length）。
enum http_parse_result http_parse_request(const char *data, size_t length,
                                          struct http_request *req);

struct http_response {
    int status;
    const char *content_type; // NULL 表示不发送 Content-Type
    uint64_t content_length;
    const char *location; // 重定向的目标，NULL 表示不发送 Location
    bool keep_alive;
    time_t date;
};

// 把状态行和响应头（含结尾的空行）写入 out，返回写入的字节数；
// size 不够时返回 0。
size_t http_format_head(char *out, size_t size,
                        const struct http_response *resp);

const char *http_reason_phrase(int status);

// 按扩展名确定 Content-Type，认不出的一律当作二进制数据。
const char *http_content_type(const char *path);

// 把请求目标翻译成相对于文档根目录的路径，写入 out。
// 成功返回 0；目标不可接受时返回应回的状态码（400、403 或 414）。
int http_target_to_path(const char *target, char *out, size_t size);

#endif
