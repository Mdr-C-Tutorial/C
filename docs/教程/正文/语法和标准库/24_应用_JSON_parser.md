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
