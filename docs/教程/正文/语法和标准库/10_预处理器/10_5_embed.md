# `#embed` 包含二进制资源

`#embed` 可以在编译时包含（一般是二进制的）文件的内容，生成一个整数常量的列表。

## 用法

基本语法：

```c
#embed [可选参数] "文件路径"
```

运行结果：该代码块主要用于语法或结构说明，单独运行通常无终端输出。


可选参数：

- `prefix(`和`suffix(`：指定生成数组前后的代码
- `limit(`：限制读取的最大字节数
- `if_empty`：文件为空时的替代内容

示例 1：嵌入文件内容到数组

```c
const unsigned char image_data[] = {
    #embed "image.png"
};
```

运行结果：该代码块主要用于语法或结构说明，单独运行通常无终端输出。


示例 2：带限制和空文件处理

```c
const char config_data[] = {
    #embed limit(1024) if_empty("default") "config.bin"
};
```

运行结果：该代码块主要用于语法或结构说明，单独运行通常无终端输出。


## `__has_embed`

`__has_embed` 是特性测试宏，用于检测编译器是否支持 `#embed`：

```c
#if __has_embed("文件路径")
    // 支持#embed
#else
    // 不支持#embed
#endif
```

运行结果：该代码块主要用于语法或结构说明，单独运行通常无终端输出。


返回：

- 1：支持该文件类型的嵌入
- 0：不支持该文件类型或完全不支持#embed

这里需要提一嘴：`#embed` 与 `__has_embed` 属于 C23 新增能力，旧标准或旧工具链上可能不可用。写可移植代码时，建议始终保留回退路径。

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

可能的输出（示例）：

<TerminalWindow>

<输出与输入或平台相关，请以实际运行为准>

</TerminalWindow>


## 使用建议

`#embed` 适合把小型静态资源编译进程序映像，例如默认配置片段、测试数据、固定模板。对大体积资源，更常见做法仍是运行期加载，以避免二进制体积和重编译成本快速上升。

## 边界与回退路径

`#embed` 本质属于编译期资源注入，因此文件路径解析依赖构建环境。为了减少“本地可编译、CI 失败”的差异，建议把资源路径集中到统一配置，并始终提供 `__has_embed` 失败分支。这样即使工具链暂不支持，也能退回到运行期加载或内置最小默认内容。

## 与构建系统的协同

当资源文件由生成步骤产出时，应确保生成动作发生在编译前，否则 `#embed` 看到的只是缺失文件。把资源生成、路径配置、特性检测放在同一构建阶段管理，通常能显著降低跨平台迁移时的偶发失败。
