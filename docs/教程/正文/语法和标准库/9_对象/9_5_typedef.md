# typedef

`typedef` 是一种存储类说明符 (Storage-class specifier)，用于引入**类型别名**（typedef name）。它不会创建新的“不同类型”，而是为已有类型提供一个新的名字。

## 1. `typedef` 做了什么

### 1.1 类型别名不改变类型兼容性

```c
typedef int i32;

int main(void) {
    i32 a = 1;
    int b = a; /* OK：i32 与 int 兼容 */
    return 0;
}
```

`typedef` 的核心价值在于：提高可读性、降低复杂声明的书写成本、以及隐藏实现细节（尤其是在头文件 API 中）。

## 2. 常见用法

### 2.1 给结构体类型起别名（推荐）

在 C 中，`struct tag` 与普通标识符处于不同命名空间。很多项目会用 `typedef` 给结构体类型提供一个更短的名字：

```c
typedef struct point {
    int x;
    int y;
} point;
```

### 2.2 前向声明 + 不完全类型（用于隐藏实现）

```c
typedef struct list list;

/* 此处可以只暴露 list* 相关 API，而把 struct list 的定义放到 .c 文件中。 */
```

### 2.3 函数指针别名（用于清晰 API）

```c
typedef int (*compare_fn)(const void* a, const void* b);
```

### 2.4 数组别名（适合固定维度）

```c
typedef int vec3[3];

int main(void) {
    vec3 v = {1, 2, 3};
    (void)v;
    return 0;
}
```

## 3. 典型陷阱：隐藏指针

`typedef` 可以把指针“藏起来”，但这会让 `const` 等限定符的阅读变得更容易出错。

```c
typedef int* int_ptr;

int main(void) {
    int x = 0;
    int_ptr p = &x;

    const int_ptr cp = p;
    /* cp 的类型是 “int* const”（常量指针），而不是 “const int*”。 */

    (void)cp;
    return 0;
}
```

::: warning 建议

除非你的团队对这类写法有明确约定，否则不要用 `typedef` 隐藏裸指针。对于读者而言，`int*` 比 `int_ptr` 更直观。

:::

## 4. 习题

1. 把下面的声明改写成 `typedef` 版本，使其更易读：
   1. `int (*fp)(int, int);`
   2. `int (*a[10])(double);`
2. 解释：为什么 `typedef int* int_ptr; const int_ptr p;` 中 `p` 不是“指向 const int 的指针”？
3. 写一个“隐藏实现”的最小示例：在头文件里只暴露 `typedef struct foo foo;` 与 `foo* foo_create(void);`，并说明这样做的好处与代价。

