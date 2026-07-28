# 17.7 `#line`

`#line` 指令用于修改后续代码中 `__LINE__` 与 `__FILE__` 的逻辑值。它主要服务于“代码生成器”场景：当源代码由工具生成时，编译诊断可以映射回原始输入文件。

## 1. 基本形式

```c
#line 120 "schema.dsl"
```
从这一行之后，预处理器会把行号视为 `120`，文件名视为 `schema.dsl`。编译器报错、`assert` 信息、调试输出中的位置元数据也会随之变化。

## 2. 示例

```c
#include <stdio.h>

int main(void) {
#line 42 "generated_from_template.c"
    printf("%s:%d\n", __FILE__, __LINE__);
    return 0;
}
```

输出：

::: terminal

generated_from_template.c:42

:::
输出会显示为 `generated_from_template.c:42`。这能让用户在阅读诊断时定位到“真正编辑的源”，而不是中间产物。

## 3. 生成代码中的常见用法

很多代码生成器会在生成片段前插入 `#line`，把后续诊断映射回模板或 DSL 文件。这样当编译器报告某行语法错误时，用户看到的是原始输入位置，而不是中间产物位置。

```c
#line 8 "model.idl"
struct person {
    int id;
    const char *name;
};
```
如果这段定义触发诊断，报错位置会落到 `model.idl` 的对应行，排错路径会更短。

## 4. 指令形式补充

`#line` 既可以只给出行号，也可以同时给出文件名。两种形式都作用于“后续行”的逻辑位置元数据。生成器若只需要校正行号，可使用短形式；若还需要映射来源文件，再补充文件名参数。
