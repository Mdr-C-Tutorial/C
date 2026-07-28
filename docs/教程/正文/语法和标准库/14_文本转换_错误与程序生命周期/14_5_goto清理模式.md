# 14.5 goto 与统一清理

第 5 章已经介绍 `goto` 的基本语义。取得多个资源后，失败路径可能需要按相反顺序释放已经取得的资源；这时可以把清理出口集中在函数末尾。

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

## 3. 与可变修改类型的约束

`goto` 还有一个容易忽略的语义边界：不能从某个作用域外部跳入“声明了可变修改类型对象”的内部作用域。遇到这类结构时，应改写控制流或调整作用域边界，而不是强行跳入。

## 4. 清理标签应保持幂等

统一失败出口通常会执行“可能已经执行过，也可能还没执行”的释放逻辑，因此标签后的清理代码应尽量写成幂等形式：`free(NULL)` 允许直接调用，文件句柄在关闭前先判空，状态位在释放后及时复位。这样一来，不同失败路径复用同一标签时更不容易出现二次释放或漏释放。

```c
#include <stdio.h>
#include <stdlib.h>

int load_all(void) {
    FILE *fp = NULL;
    char *buf = NULL;
    int rc = -1;

    fp = fopen("a.txt", "rb");
    if (fp == NULL) {
        goto out;
    }

    buf = malloc(4096);
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
