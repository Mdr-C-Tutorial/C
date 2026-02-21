# 综合案例：JSON parser

这一章把前面章节里的字符串处理、内存管理、递归下降和错误处理放到一个完整案例中：实现一个最小可用的 JSON 解析器。目标不是一次写出完整工业实现，而是建立“语法到对象结构”的工程路径。

## 1. 范围约束

本案例支持对象、数组、字符串、数字、`true`、`false`、`null`。先不处理注释、`NaN`、`Infinity` 等非标准扩展。把范围收紧，才能把正确性做到可验证。

## 2. 数据结构

```c
typedef enum {
    JSON_NULL,
    JSON_BOOL,
    JSON_NUMBER,
    JSON_STRING,
    JSON_ARRAY,
    JSON_OBJECT
} json_kind;

typedef struct json_value json_value;

typedef struct {
    char *key;
    json_value *value;
} json_member;

struct json_value {
    json_kind kind;
    union {
        int b;
        double num;
        char *str;
        struct { json_value **items; size_t len; } arr;
        struct { json_member *items; size_t len; } obj;
    } as;
};
```

运行结果：该代码块主要用于语法或结构说明，单独运行通常无终端输出。


这个结构把“标签 + 联合体”模式落地为统一 JSON 节点表示。不同种类共享外层壳体，访问时由 `kind` 决定解释方式。

## 3. 解析流程

解析器维护一个只读游标，跳过空白后按首字符分派：`{` 进入对象解析，`[` 进入数组解析，`\"` 进入字符串解析，数字前缀进入数字解析，字母前缀匹配关键字。每个子解析函数成功时推进游标，失败时返回错误码并保持资源可回收。

```c
typedef struct {
    const char *cur;
    const char *end;
    const char *msg;
} json_parser;

static void skip_ws(json_parser *p) {
    while (p->cur < p->end &&
           (*p->cur == ' ' || *p->cur == '\\n' ||
            *p->cur == '\\r' || *p->cur == '\\t')) {
        p->cur++;
    }
}
```

运行结果：该代码块主要用于语法或结构说明，单独运行通常无终端输出。


这类游标结构可以把错误定位到具体输入位置，也便于后续扩展增量解析。

## 4. 内存与清理策略

JSON 解析最容易出错的点不是语法分支，而是失败路径清理。建议所有“创建节点”的函数都遵循同一模板：先分配、再填充、任何中途失败都跳转到统一释放标签，保证不会泄漏。成功返回后，由调用方统一调用 `json_free` 深度释放整棵树。

## 5. 结果校验

解析完成后必须做两件事：确认根节点不为空，确认游标已经到达输入末尾（忽略尾随空白）。否则应判为“有剩余垃圾输入”，而不是“解析成功”。这一步能拦截大量看似成功、实则半截输入的问题。

## 6. 下一步扩展

当最小版本稳定后，可以按顺序扩展：字符串转义完整支持（含 `\\uXXXX`）、数字范围检查、错误位置信息格式化、流式输入接口。每加一项能力都应配套回归样例，避免功能增长破坏已有正确性。

## 7. 一个可运行的最小实现（C11）

下面给出一个单文件版本，覆盖本章约定的最小范围：对象、数组、字符串、数字、`true`、`false`、`null`。字符串转义支持常见短转义和 `\uXXXX`（含代理对组合）。为便于验证，示例里同时提供了释放函数和打印函数。

```c
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef enum {
    JSON_NULL,
    JSON_BOOL,
    JSON_NUMBER,
    JSON_STRING,
    JSON_ARRAY,
    JSON_OBJECT
} json_kind;

typedef struct json_value json_value;

typedef struct {
    char *key;
    json_value *value;
} json_member;

struct json_value {
    json_kind kind;
    union {
        int b;
        double num;
        char *str;
        struct {
            json_value **items;
            size_t len;
            size_t cap;
        } arr;
        struct {
            json_member *items;
            size_t len;
            size_t cap;
        } obj;
    } as;
};

typedef struct {
    const char *cur;
    const char *end;
    const char *msg;
} json_parser;

static void json_free(json_value *v);

static json_value *json_new(json_kind kind) {
    json_value *v = (json_value *)calloc(1, sizeof(*v));
    if (v != NULL) {
        v->kind = kind;
    }
    return v;
}

static void skip_ws(json_parser *p) {
    while (p->cur < p->end &&
           (*p->cur == ' ' || *p->cur == '\n' || *p->cur == '\r' || *p->cur == '\t')) {
        p->cur++;
    }
}

static int parser_fail(json_parser *p, const char *msg) {
    if (p->msg == NULL) {
        p->msg = msg;
    }
    return 0;
}

static int append_byte(char **buf, size_t *len, size_t *cap, unsigned char c) {
    if (*len + 1 >= *cap) {
        size_t new_cap = (*cap == 0) ? 32 : (*cap * 2);
        char *new_buf = (char *)realloc(*buf, new_cap);
        if (new_buf == NULL) {
            return 0;
        }
        *buf = new_buf;
        *cap = new_cap;
    }
    (*buf)[(*len)++] = (char)c;
    return 1;
}

static int parse_hex4(json_parser *p, unsigned *out) {
    unsigned v = 0;
    for (int i = 0; i < 4; ++i) {
        if (p->cur >= p->end) {
            return 0;
        }
        char c = *p->cur++;
        unsigned d;
        if (c >= '0' && c <= '9') {
            d = (unsigned)(c - '0');
        } else if (c >= 'a' && c <= 'f') {
            d = (unsigned)(c - 'a' + 10);
        } else if (c >= 'A' && c <= 'F') {
            d = (unsigned)(c - 'A' + 10);
        } else {
            return 0;
        }
        v = (v << 4) | d;
    }
    *out = v;
    return 1;
}

static int append_utf8(char **buf, size_t *len, size_t *cap, unsigned cp) {
    if (cp <= 0x7Fu) {
        return append_byte(buf, len, cap, (unsigned char)cp);
    }
    if (cp <= 0x7FFu) {
        return append_byte(buf, len, cap, (unsigned char)(0xC0u | (cp >> 6))) &&
               append_byte(buf, len, cap, (unsigned char)(0x80u | (cp & 0x3Fu)));
    }
    if (cp <= 0xFFFFu) {
        return append_byte(buf, len, cap, (unsigned char)(0xE0u | (cp >> 12))) &&
               append_byte(buf, len, cap, (unsigned char)(0x80u | ((cp >> 6) & 0x3Fu))) &&
               append_byte(buf, len, cap, (unsigned char)(0x80u | (cp & 0x3Fu)));
    }
    if (cp <= 0x10FFFFu) {
        return append_byte(buf, len, cap, (unsigned char)(0xF0u | (cp >> 18))) &&
               append_byte(buf, len, cap, (unsigned char)(0x80u | ((cp >> 12) & 0x3Fu))) &&
               append_byte(buf, len, cap, (unsigned char)(0x80u | ((cp >> 6) & 0x3Fu))) &&
               append_byte(buf, len, cap, (unsigned char)(0x80u | (cp & 0x3Fu)));
    }
    return 0;
}

static json_value *parse_value(json_parser *p);

static json_value *parse_string(json_parser *p) {
    if (p->cur >= p->end || *p->cur != '\"') {
        parser_fail(p, "expected '\"' for string");
        return NULL;
    }
    p->cur++;

    char *buf = NULL;
    size_t len = 0;
    size_t cap = 0;

    while (p->cur < p->end) {
        unsigned char c = (unsigned char)*p->cur++;
        if (c == '\"') {
            break;
        }
        if (c == '\\') {
            if (p->cur >= p->end) {
                parser_fail(p, "unfinished escape");
                free(buf);
                return NULL;
            }
            unsigned char e = (unsigned char)*p->cur++;
            switch (e) {
                case '\"': c = '\"'; break;
                case '\\': c = '\\'; break;
                case '/':  c = '/';  break;
                case 'b':  c = '\b'; break;
                case 'f':  c = '\f'; break;
                case 'n':  c = '\n'; break;
                case 'r':  c = '\r'; break;
                case 't':  c = '\t'; break;
                case 'u': {
                    unsigned hi = 0;
                    if (!parse_hex4(p, &hi)) {
                        parser_fail(p, "bad \\uXXXX escape");
                        free(buf);
                        return NULL;
                    }
                    unsigned cp = hi;
                    if (hi >= 0xD800u && hi <= 0xDBFFu) {
                        if (p->end - p->cur < 6 || p->cur[0] != '\\' || p->cur[1] != 'u') {
                            parser_fail(p, "missing low surrogate");
                            free(buf);
                            return NULL;
                        }
                        p->cur += 2;
                        unsigned lo = 0;
                        if (!parse_hex4(p, &lo) || lo < 0xDC00u || lo > 0xDFFFu) {
                            parser_fail(p, "bad low surrogate");
                            free(buf);
                            return NULL;
                        }
                        cp = 0x10000u + (((hi - 0xD800u) << 10) | (lo - 0xDC00u));
                    } else if (hi >= 0xDC00u && hi <= 0xDFFFu) {
                        parser_fail(p, "unexpected low surrogate");
                        free(buf);
                        return NULL;
                    }
                    if (!append_utf8(&buf, &len, &cap, cp)) {
                        parser_fail(p, "out of memory");
                        free(buf);
                        return NULL;
                    }
                    continue;
                }
                default:
                    parser_fail(p, "unknown escape");
                    free(buf);
                    return NULL;
            }
        } else if (c < 0x20u) {
            parser_fail(p, "control char in string");
            free(buf);
            return NULL;
        }
        if (!append_byte(&buf, &len, &cap, c)) {
            parser_fail(p, "out of memory");
            free(buf);
            return NULL;
        }
    }

    if (p->cur > p->end || (p->cur <= p->end && (p->cur == p->end) && (len == 0 || buf[len - 1] != '\"'))) {
        /* no-op: real close check already by break */
    }
    if (p->cur > p->end || (p->cur == p->end && (len == 0 || (len > 0 && buf[len - 1] != '\"')))) {
        /* no-op guard to keep parser simple */
    }

    if (p->cur <= p->end && (p->cur == p->end || p->cur[-1] != '\"')) {
        parser_fail(p, "unterminated string");
        free(buf);
        return NULL;
    }

    if (!append_byte(&buf, &len, &cap, '\0')) {
        parser_fail(p, "out of memory");
        free(buf);
        return NULL;
    }

    json_value *v = json_new(JSON_STRING);
    if (v == NULL) {
        parser_fail(p, "out of memory");
        free(buf);
        return NULL;
    }
    v->as.str = buf;
    return v;
}

static json_value *parse_number(json_parser *p) {
    const char *start = p->cur;
    if (p->cur < p->end && (*p->cur == '-')) {
        p->cur++;
    }
    if (p->cur >= p->end) {
        parser_fail(p, "bad number");
        return NULL;
    }
    if (*p->cur == '0') {
        p->cur++;
    } else if (*p->cur >= '1' && *p->cur <= '9') {
        while (p->cur < p->end && isdigit((unsigned char)*p->cur)) {
            p->cur++;
        }
    } else {
        parser_fail(p, "bad number");
        return NULL;
    }
    if (p->cur < p->end && *p->cur == '.') {
        p->cur++;
        if (p->cur >= p->end || !isdigit((unsigned char)*p->cur)) {
            parser_fail(p, "bad fraction");
            return NULL;
        }
        while (p->cur < p->end && isdigit((unsigned char)*p->cur)) {
            p->cur++;
        }
    }
    if (p->cur < p->end && (*p->cur == 'e' || *p->cur == 'E')) {
        p->cur++;
        if (p->cur < p->end && (*p->cur == '+' || *p->cur == '-')) {
            p->cur++;
        }
        if (p->cur >= p->end || !isdigit((unsigned char)*p->cur)) {
            parser_fail(p, "bad exponent");
            return NULL;
        }
        while (p->cur < p->end && isdigit((unsigned char)*p->cur)) {
            p->cur++;
        }
    }

    size_t n = (size_t)(p->cur - start);
    char *tmp = (char *)malloc(n + 1);
    if (tmp == NULL) {
        parser_fail(p, "out of memory");
        return NULL;
    }
    memcpy(tmp, start, n);
    tmp[n] = '\0';

    errno = 0;
    char *endptr = NULL;
    double d = strtod(tmp, &endptr);
    if (errno == ERANGE || endptr == tmp || *endptr != '\0') {
        free(tmp);
        parser_fail(p, "number out of range");
        return NULL;
    }
    free(tmp);

    json_value *v = json_new(JSON_NUMBER);
    if (v == NULL) {
        parser_fail(p, "out of memory");
        return NULL;
    }
    v->as.num = d;
    return v;
}

static int expect_literal(json_parser *p, const char *lit) {
    size_t n = strlen(lit);
    if ((size_t)(p->end - p->cur) < n) {
        return 0;
    }
    if (memcmp(p->cur, lit, n) != 0) {
        return 0;
    }
    p->cur += n;
    return 1;
}

static int array_push(json_value *arr, json_value *item) {
    if (arr->as.arr.len == arr->as.arr.cap) {
        size_t new_cap = (arr->as.arr.cap == 0) ? 4 : arr->as.arr.cap * 2;
        json_value **new_items = (json_value **)realloc(arr->as.arr.items, new_cap * sizeof(*new_items));
        if (new_items == NULL) {
            return 0;
        }
        arr->as.arr.items = new_items;
        arr->as.arr.cap = new_cap;
    }
    arr->as.arr.items[arr->as.arr.len++] = item;
    return 1;
}

static int object_push(json_value *obj, char *key, json_value *value) {
    if (obj->as.obj.len == obj->as.obj.cap) {
        size_t new_cap = (obj->as.obj.cap == 0) ? 4 : obj->as.obj.cap * 2;
        json_member *new_items = (json_member *)realloc(obj->as.obj.items, new_cap * sizeof(*new_items));
        if (new_items == NULL) {
            return 0;
        }
        obj->as.obj.items = new_items;
        obj->as.obj.cap = new_cap;
    }
    obj->as.obj.items[obj->as.obj.len].key = key;
    obj->as.obj.items[obj->as.obj.len].value = value;
    obj->as.obj.len++;
    return 1;
}

static json_value *parse_array(json_parser *p) {
    if (p->cur >= p->end || *p->cur != '[') {
        parser_fail(p, "expected '['");
        return NULL;
    }
    p->cur++;
    skip_ws(p);

    json_value *arr = json_new(JSON_ARRAY);
    if (arr == NULL) {
        parser_fail(p, "out of memory");
        return NULL;
    }
    if (p->cur < p->end && *p->cur == ']') {
        p->cur++;
        return arr;
    }

    for (;;) {
        skip_ws(p);
        json_value *item = parse_value(p);
        if (item == NULL) {
            json_free(arr);
            return NULL;
        }
        if (!array_push(arr, item)) {
            json_free(item);
            json_free(arr);
            parser_fail(p, "out of memory");
            return NULL;
        }
        skip_ws(p);
        if (p->cur >= p->end) {
            json_free(arr);
            parser_fail(p, "unterminated array");
            return NULL;
        }
        if (*p->cur == ',') {
            p->cur++;
            continue;
        }
        if (*p->cur == ']') {
            p->cur++;
            return arr;
        }
        json_free(arr);
        parser_fail(p, "expected ',' or ']'");
        return NULL;
    }
}

static json_value *parse_object(json_parser *p) {
    if (p->cur >= p->end || *p->cur != '{') {
        parser_fail(p, "expected '{'");
        return NULL;
    }
    p->cur++;
    skip_ws(p);

    json_value *obj = json_new(JSON_OBJECT);
    if (obj == NULL) {
        parser_fail(p, "out of memory");
        return NULL;
    }
    if (p->cur < p->end && *p->cur == '}') {
        p->cur++;
        return obj;
    }

    for (;;) {
        skip_ws(p);
        json_value *k = parse_string(p);
        if (k == NULL) {
            json_free(obj);
            return NULL;
        }
        char *key = k->as.str;
        free(k);

        skip_ws(p);
        if (p->cur >= p->end || *p->cur != ':') {
            free(key);
            json_free(obj);
            parser_fail(p, "expected ':'");
            return NULL;
        }
        p->cur++;
        skip_ws(p);

        json_value *val = parse_value(p);
        if (val == NULL) {
            free(key);
            json_free(obj);
            return NULL;
        }
        if (!object_push(obj, key, val)) {
            free(key);
            json_free(val);
            json_free(obj);
            parser_fail(p, "out of memory");
            return NULL;
        }

        skip_ws(p);
        if (p->cur >= p->end) {
            json_free(obj);
            parser_fail(p, "unterminated object");
            return NULL;
        }
        if (*p->cur == ',') {
            p->cur++;
            continue;
        }
        if (*p->cur == '}') {
            p->cur++;
            return obj;
        }
        json_free(obj);
        parser_fail(p, "expected ',' or '}'");
        return NULL;
    }
}

static json_value *parse_value(json_parser *p) {
    skip_ws(p);
    if (p->cur >= p->end) {
        parser_fail(p, "unexpected end");
        return NULL;
    }

    switch (*p->cur) {
        case '{': return parse_object(p);
        case '[': return parse_array(p);
        case '"': return parse_string(p);
        case 't': {
            if (!expect_literal(p, "true")) {
                parser_fail(p, "bad literal");
                return NULL;
            }
            json_value *v = json_new(JSON_BOOL);
            if (v == NULL) {
                parser_fail(p, "out of memory");
                return NULL;
            }
            v->as.b = 1;
            return v;
        }
        case 'f': {
            if (!expect_literal(p, "false")) {
                parser_fail(p, "bad literal");
                return NULL;
            }
            json_value *v = json_new(JSON_BOOL);
            if (v == NULL) {
                parser_fail(p, "out of memory");
                return NULL;
            }
            v->as.b = 0;
            return v;
        }
        case 'n': {
            if (!expect_literal(p, "null")) {
                parser_fail(p, "bad literal");
                return NULL;
            }
            json_value *v = json_new(JSON_NULL);
            if (v == NULL) {
                parser_fail(p, "out of memory");
                return NULL;
            }
            return v;
        }
        default:
            if (*p->cur == '-' || isdigit((unsigned char)*p->cur)) {
                return parse_number(p);
            }
            parser_fail(p, "unexpected token");
            return NULL;
    }
}

static json_value *json_parse(const char *text, const char **err_msg, const char **err_pos) {
    json_parser p;
    p.cur = text;
    p.end = text + strlen(text);
    p.msg = NULL;

    json_value *root = parse_value(&p);
    if (root == NULL) {
        if (err_msg != NULL) *err_msg = p.msg;
        if (err_pos != NULL) *err_pos = p.cur;
        return NULL;
    }
    skip_ws(&p);
    if (p.cur != p.end) {
        json_free(root);
        if (err_msg != NULL) *err_msg = "trailing characters";
        if (err_pos != NULL) *err_pos = p.cur;
        return NULL;
    }
    if (err_msg != NULL) *err_msg = NULL;
    if (err_pos != NULL) *err_pos = NULL;
    return root;
}

static void print_indent(int n) {
    for (int i = 0; i < n; ++i) {
        putchar(' ');
    }
}

static void json_print(const json_value *v, int indent) {
    switch (v->kind) {
        case JSON_NULL:
            printf("null");
            break;
        case JSON_BOOL:
            printf(v->as.b ? "true" : "false");
            break;
        case JSON_NUMBER:
            printf("%.17g", v->as.num);
            break;
        case JSON_STRING:
            printf("\"%s\"", v->as.str);
            break;
        case JSON_ARRAY:
            printf("[\n");
            for (size_t i = 0; i < v->as.arr.len; ++i) {
                print_indent(indent + 2);
                json_print(v->as.arr.items[i], indent + 2);
                if (i + 1 < v->as.arr.len) {
                    printf(",");
                }
                printf("\n");
            }
            print_indent(indent);
            printf("]");
            break;
        case JSON_OBJECT:
            printf("{\n");
            for (size_t i = 0; i < v->as.obj.len; ++i) {
                print_indent(indent + 2);
                printf("\"%s\": ", v->as.obj.items[i].key);
                json_print(v->as.obj.items[i].value, indent + 2);
                if (i + 1 < v->as.obj.len) {
                    printf(",");
                }
                printf("\n");
            }
            print_indent(indent);
            printf("}");
            break;
    }
}

static void json_free(json_value *v) {
    if (v == NULL) {
        return;
    }
    switch (v->kind) {
        case JSON_STRING:
            free(v->as.str);
            break;
        case JSON_ARRAY:
            for (size_t i = 0; i < v->as.arr.len; ++i) {
                json_free(v->as.arr.items[i]);
            }
            free(v->as.arr.items);
            break;
        case JSON_OBJECT:
            for (size_t i = 0; i < v->as.obj.len; ++i) {
                free(v->as.obj.items[i].key);
                json_free(v->as.obj.items[i].value);
            }
            free(v->as.obj.items);
            break;
        default:
            break;
    }
    free(v);
}

int main(void) {
    const char *src =
        "{"
        "\"name\":\"mdr\","
        "\"ok\":true,"
        "\"score\":99.5,"
        "\"tags\":[\"c\",\"json\"],"
        "\"meta\":{\"x\":1,\"y\":null}"
        "}";

    const char *err_msg = NULL;
    const char *err_pos = NULL;
    json_value *root = json_parse(src, &err_msg, &err_pos);
    if (root == NULL) {
        long pos = (long)(err_pos - src);
        printf("parse error at %ld: %s\n", pos, err_msg != NULL ? err_msg : "unknown");
        return 1;
    }

    json_print(root, 0);
    printf("\n");
    json_free(root);
    return 0;
}
```

可能的输出（示例）：

<TerminalWindow>

{
  "name": "mdr",
  "ok": true,
  "score": 99.5,
  "tags": [
    "c",
    "json"
  ],
  "meta": {
    "x": 1,
    "y": null
  }
}

</TerminalWindow>

## 8. 先实现哪些测试用例

建议先准备三组回归输入：一组纯成功路径（对象、数组、数字、转义字符串都覆盖），一组语法失败路径（缺失逗号、缺失闭合符），一组边界路径（空数组、空对象、指数数字、代理对转义）。先把这三组跑稳定，再扩展更多能力。

## 9. 完整实现（可直接复用）

这一节的完整实现已经放在示例代码目录：[`/教程/示例代码/项目/json_parser.c.md`](/教程/示例代码/项目/json_parser.c.md)。同一份实现也已同步到代码目录：`/code/语法和标准库/24_应用_JSON_parser/json_parser.c`。实现包含以下能力：对象、数组、字符串、数字、`true`、`false`、`null`，并提供统一 `json_free` 深度释放。

核心入口保持为三个接口：

```c
json_value *json_parse(const char *text, const char **err_pos, const char **err_msg);
void json_free(json_value *v);
const json_value *json_object_get(const json_value *obj, const char *key);
```

运行结果：该代码块主要用于语法或结构说明，单独运行通常无终端输出。

## 10. 运行示例

下面给出一个最小调用片段，展示“解析 -> 读取 -> 释放”的主路径：

```c
const char *text =
    "{"
    "\"user\":\"Ada\","
    "\"age\":18,"
    "\"active\":true,"
    "\"scores\":[99.5,88,77]"
    "}";

const char *err_pos = NULL;
const char *err_msg = NULL;
json_value *root = json_parse(text, &err_pos, &err_msg);
if (root == NULL) {
    printf("parse error: %s\n", err_msg);
    return 1;
}

const json_value *user = json_object_get(root, "user");
printf("user=%s\n", user->as.str);
json_free(root);
```

可能的输出（示例）：

<TerminalWindow>

user=Ada

</TerminalWindow>
