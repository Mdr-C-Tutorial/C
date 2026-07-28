# 17.5 `#embed` 包含二进制资源

`#embed` 可以在编译时包含（一般是二进制的）文件的内容，生成一个整数常量的列表。

## 用法

基本语法：

```c
#embed "文件路径" [可选参数]
```
可选参数：

- `prefix(...)` 和 `suffix(...)`：在资源展开结果前后插入记号；
- `limit(...)`：限制读取的最大字节数；
- `if_empty(...)`：资源为空时使用替代记号。

示例 1：嵌入文件内容到数组

```c
const unsigned char image_data[] = {
    #embed "image.png"
};
```
示例 2：带限制和空文件处理

```c
const char config_data[] = {
    #embed "config.bin" limit(1024) if_empty("default")
};
```
## `__has_embed`

`__has_embed` 是特性测试宏，用于检测编译器是否支持 `#embed`：

```c
#if __has_embed("文件路径")
    // 支持#embed
#else
    // 不支持#embed
#endif
```
结果是下列三个预定义宏之一：

- `__STDC_EMBED_FOUND__`：找到非空资源；
- `__STDC_EMBED_EMPTY__`：找到空资源；
- `__STDC_EMBED_NOT_FOUND__`：未找到资源。

这里需要提一嘴：`#embed` 与 `__has_embed` 属于 C23 新增能力，旧标准或旧工具链上可能不可用。写可移植代码时，建议始终保留回退路径。

## 完整示例

```c
#include <stdio.h>

int main(void) {
#if __has_embed("data.bin") == __STDC_EMBED_FOUND__
    static const unsigned char data[] = {
#embed "data.bin"
    };
    printf("Loaded %zu bytes\n", sizeof data);
#elif __has_embed("data.bin") == __STDC_EMBED_EMPTY__
    puts("Loaded 0 bytes");
#else
    puts("data.bin was not found");
#endif
    return 0;
}
```

结果由构建时的 `data.bin` 决定：

::: code-group

```text [3 字节资源]
Loaded 3 bytes
```

```text [空资源]
Loaded 0 bytes
```

```text [资源不存在]
data.bin was not found
```

:::
