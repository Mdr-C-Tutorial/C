# 结构体

结构体是一种由一系列成员组成的类型，这些成员按顺序分布在内存中

## 1. 定义

关键字 `struct` + “名字” + 声明列表
其中声明列表是一个用**逗号**分隔，**花括号**环绕的列表，列表中的每一项是一个成员的声明

```c
struct aircraft_carrier {
    char* name;
    int id;
};
```

上述代码引入了一个新的类型 `struct aircraft_carrier`，并指定了它的含义。
这个类型包含两个成员：`name` 和 `id`。

### `typedef` 起别名

```c
typedef struct {
    char* name;
    int id;
} aircraft_carrier;
```

或者

```c
struct aircraft_carrier {
    char* name;
    int id;
};
typedef struct aircraft_carrier aircraft_carrier;
```

取别名之后，用 `aircraft_carrier` 表示这个结构体类型而不是 `struct aircraft_carrier`。（在第二种情况中，`struct aircraft` 也可用）

## 2. 初始化

```c
struct aircraft_carrier carrier = {"LiaoNing", 16};
struct aircraft_carrier carrier2 = {.id = 17, .name = "ShanDong"};
```

初始化结构体要用**花括号**环绕，**逗号**分隔的列表，列表中的每一项都应该是能初始化对应成员的表达式。
可以为列表中的表达式提供 **指派符**，初始化结构体内特定的成员。
第二行中 `.id` `.name` 即为 **指派符**。

### 嵌套初始化

```c
#include <stdio.h>

typedef struct {
    int a;
    int b[2];
} A;

typedef struct {
    int c;
    A d;
} B;

int main(void) {
    A a1 = {.a = 1, .b[0] = 4, .b[1] = 5};
    B arr1[] = {
                   {10, .d = a1, .d.a = 2},  // 此时 arr1[0]={10, {1, {4, 5} } };
                   [0].d.b[0] = 6            // 此时 arr1[0]={10, {1, {6, 5} } };
                };
    // B arr2[] = {{10, .d = a1, .d.a = 2}, [0] = {.d.b[0] = 6}};
    printf("%d", arr1[0].d.a);
    printf("%d", arr2[0].d.a);

    return 0;
}
```

注释掉的行会报**warning**，因为在列表的第一个表达式里面已经初始化了 `arr2[0]`，再初始化一遍会使之前初始化的被重置。

## 3. 柔性数组成员

## 4. 对象表示
