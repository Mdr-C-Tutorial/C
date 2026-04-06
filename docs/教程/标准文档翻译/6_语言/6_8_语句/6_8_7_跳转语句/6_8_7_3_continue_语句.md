# 6.8.7.3 continue 语句

## 约束

1. `continue` 语句只能出现在循环体中，或者本身就是循环体。

## 语义

1. `continue` 语句会跳转到最内层包围它的迭代语句的“继续执行”位置，也就是循环体的末尾。
2. 更精确地说，在下列三种语句中，只要所示 `continue` 不位于某个更内层的迭代语句中，它都等价于 `goto contin;`：

```c
while (/* ... */) {
    /* ... */
    continue;
    /* ... */
contin:
}

do {
    /* ... */
    continue;
    /* ... */
contin:;
} while (/* ... */);

for (/* ... */) {
    /* ... */
    continue;
    /* ... */
contin:
}
```
