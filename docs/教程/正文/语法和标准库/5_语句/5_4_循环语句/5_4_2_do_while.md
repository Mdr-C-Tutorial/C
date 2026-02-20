# do while

`do while` 是“先执行、后判断”的循环语句。无论条件是否成立，循环体都会先执行一次，因此它适合“至少执行一次”的流程。

## 1. 基本语法

```c
do {
    statement;
} while (condition);
```

注意结尾分号属于语法的一部分，不能省略。

## 2. 示例

```c
#include <stdio.h>

int main(void) {
    int n = 3;

    do {
        printf("n = %d\n", n);
        n--;
    } while (n > 0);

    return 0;
}
```

即使把初值改成 `n = 0`，循环体仍会先执行一次，然后才根据条件决定是否继续。

## 3. 使用建议

当循环体中包含“读取输入再判断是否继续”这类交互逻辑时，`do while` 往往比 `while` 更自然；若业务逻辑允许零次执行，优先考虑 `while`，可读性通常更好。
