# `#embed` 包含二进制资源

`#embed` 可以在编译时包含（一般是二进制的）文件的内容，生成一个整数常量的列表。

## 用法

基本语法：

```c
#embed [可选参数] "文件路径"
```

可选参数：

- `prefix(`和`suffix(`：指定生成数组前后的代码
- `limit(`：限制读取的最大字节数
- `if_empty`：文件为空时的替代内容

示例1：嵌入文件内容到数组

```c
const unsigned char image_data[] = {
    #embed "image.png"
};
```

示例2：带限制和空文件处理

```c
const char config_data[] = {
    #embed limit(1024) if_empty("default") "config.bin"
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

返回：

- 1：支持该文件类型的嵌入
- 0：不支持该文件类型或完全不支持#embed

## 完整示例

```c
#include <stdio.h>

int main() {
    #if __has_embed("data.bin")
        const unsigned char data[] = {
            #embed prefix(0x) suffix(,) "data.bin"
        };
        size_t data_size = sizeof(data);
        printf("Loaded %zu bytes\n", data_size);
    #else
        printf("Embed not supported\n");
    #endif
    return 0;
}
```
