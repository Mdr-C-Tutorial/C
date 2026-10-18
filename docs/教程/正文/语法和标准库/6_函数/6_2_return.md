# 6.2 return

`return` 用于结束当前函数执行，并把控制权交还调用者。

## 1. 返回值规则

非 `void` 函数应返回与声明兼容的表达式；`void` 函数可写 `return;` 提前结束。把 `return` 放在分支入口处（卫语句）通常能减少嵌套深度，让主流程更平直。

```c
#include <stdbool.h>

bool is_valid_score(int score) {
    if (score < 0 || score > 100) {
        return false;
    }
    return true;
}
```

## 2. 与对象生存期的关系

`return` 执行时，当前函数的自动存储期对象会结束生命周期。因此不要返回指向局部对象的指针。

```c
int *bad(void) {
    int x = 42;
    return &x; /* 错误：返回悬垂指针 */
}
```

可以改为直接返回值本身、由调用者提供存储，或返回动态分配的存储（见第 11 章）并约定由调用者释放。

## 3. `main` 的返回语义

在宿主环境中，`main` 返回值会作为进程退出状态交给执行环境。通常约定 `0`（或 `EXIT_SUCCESS`）表示成功，非零（或 `EXIT_FAILURE`）表示失败。把失败原因映射到稳定返回码，能让脚本、CI 和调用链更容易判断执行结果。

```c
#include <stdlib.h>

int main(void) {
    int ok = 1;
    if (!ok) {
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
```

## 4. 非 `void` 函数的完整返回

::: concept function-end-without-return title="执行到非 void 函数末尾"

如果非 `void` 函数执行到末尾的 `}` 而没有执行 `return`，并且调用者使用了这次调用的值，行为未定义。`main` 是例外：执行到 `main` 末尾的 `}` 相当于返回 `0`。

:::

写分支时应确保每条路径都有明确返回，避免把“理论不会走到这里”当作隐式前提。

## 5. 返回结构体

函数可以返回结构体对象（结构体在第 10 章介绍），一次返回多个相关的值，从而减少用于输出的指针形参。需要同时报告成功与否时，可以在返回的结构体中加入表示状态的成员，或用返回值表示状态、用指针形参输出结果。
