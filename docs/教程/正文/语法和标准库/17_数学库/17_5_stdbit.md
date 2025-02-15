# 位操作

## 位操作函数

## 端序

端序是构成多字节对象的字节的顺序。更多相关内容请参考 [Wikipedia](https://zh.wikipedia.org/wiki/%E5%AD%97%E8%8A%82%E5%BA%8F)。

在 `<stdbit.h>` 中定义了三个宏，用来表示标量类型的端序：

- `__STDC_ENDIAN_BIG__`
- `__STDC_ENDIAN_LITTLE__`
- `__STDC_ENDIAN_NATIVE__`

1. 如果标量类型为**小端序**，则 `__STDC_ENDIAN_NATIVE__` 等于 `__STDC_ENDIAN_LITTLE__`；
2. 如果标量类型为**大端序**，则 `__STDC_ENDIAN_NATIVE__` 等于 `__STDC_ENDIAN_BIG__`。
3. 如果既不使用大端序，也不使用小端序，则 `__STDC_ENDIAN_NATIVE__` 既不等于 `__STDC_ENDIAN_BIG__`，也不等于 `__STDC_ENDIAN_LITTLE__`。

示例：

```c
#include <stdio.h>
#include <stdbit.h>

int main() {
    if (__STDC_ENDIAN_NATIVE__ == __STDC_ENDIAN_BIG__) {
        printf("大端序\n");
    } else if (__STDC_ENDIAN_NATIVE__ == __STDC_ENDIAN_LITTLE__) {
        printf("小端序\n");
    } else {
        printf("未知的端序\n");
    }
    return 0;
}
```
