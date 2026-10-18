# 3.8 `sizeof` 运算符

`sizeof` 运算符给出类型或表达式结果类型的大小，单位是字节，结果的类型是 `size_t`。它常用于计算对象大小、数组长度和动态分配所需的字节数。

## 1. 两种写法与一个习惯

`sizeof(type-name)` 对类型取大小，`sizeof expr` 对表达式结果类型取大小。第二种写法语法上可省括号，但为了避免优先级歧义，通常写成 `sizeof(expr)` 更清楚。

```c
size_t a = sizeof(int);
size_t b = sizeof(1 + 2.0);   /* 等价于 sizeof(double) */
```

## 2. 习题

<Exercise id="10313" :d="4" :w="3">

写一个程序，分别输出：

1. `sizeof(int)`
2. `sizeof(double)`
3. `sizeof(char)`

然后解释这三个结果为何可能不同。

</Exercise>
