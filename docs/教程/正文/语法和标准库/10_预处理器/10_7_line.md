# `#line`

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

输出会显示为 `generated_from_template.c:42`。这能让用户在阅读诊断时定位到“真正编辑的源”，而不是中间产物。

## 3. 使用边界

手写业务代码通常不需要 `#line`。只有在“源到源转换”链路中，需要保持诊断可追踪时，它才值得使用。滥用会让调试信息失真，反而增加排错难度。
