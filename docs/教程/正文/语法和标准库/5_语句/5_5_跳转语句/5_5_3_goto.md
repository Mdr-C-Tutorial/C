# goto

`goto` 会把控制流跳转到同一函数内的某个标号。它功能强，但应谨慎使用。现代 C 工程里，`goto` 最常见、也最合理的用途是统一错误清理路径。

## 1. 基本语法

```c
goto label;
/* ... */
label:
    statement;
```

标号具有函数作用域，但跳转目标必须在同一函数内。

## 2. 典型清理模式

```c
#include <stdio.h>
#include <stdlib.h>

int work(void) {
    FILE *fp = NULL;
    int *buf = NULL;
    int rc = -1;

    fp = fopen("data.txt", "r");
    if (fp == NULL) {
        goto out;
    }

    buf = malloc(1024 * sizeof *buf);
    if (buf == NULL) {
        goto out;
    }

    rc = 0;

out:
    free(buf);
    if (fp != NULL) {
        fclose(fp);
    }
    return rc;
}
```

这类写法比多层 `if` 嵌套更容易保证“每条失败路径都能完整释放资源”。

## 3. 使用边界

不要把 `goto` 当作任意流程控制工具，尤其不要跨越对象初始化语义造成难以推断的状态。若结构化语句已经足够表达意图，优先使用结构化语句。
