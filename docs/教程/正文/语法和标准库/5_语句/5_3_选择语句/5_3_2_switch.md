# switch

`switch` 语句是一种多分支选择语句，用于根据值选择不同的执行路径。

## 语法

一般地：

```c
switch (表达式) {
    case 整数常量表达式1:
        语句序列1
        break; // 有时可以没有 break
    case 整数常量表达式2:
        语句序列2
        break; // 有时可以没有 break

    // ... 其他 case
    // ... 其他语句序列
    default:
        默认语句序列
        break;
}
```

运行结果：该代码块主要用于语法或结构说明，单独运行通常无终端输出。


其中，`表达式` 必须是整数类型（包括 char、short、int、long 及其无符号版本，以及枚举类型）。

执行 `switch` 语句时，首先计算 `表达式` 的值，然后将其与每个 `case` 标签的值进行比较。如果找到匹配的 `case` 标签，则从相应的标签处开始执行。如果没有找到匹配的 `case` 标签并且存在 `default` 子句，则从 `default` 子句开始执行。

以上面的示例代码为例：

- 如果 `表达式` 的值等于 `整数常量表达式1`，则从 `整数常量表达式1` 这个标签处开始执行，即执行 `语句序列1`，`语句序列2`，`其他语句序列` 和 `默认语句序列`；
- 如果 `表达式` 的值等于 `整数常量表达式2`，则从 `整数常量表达式2` 这个标签处开始执行，即执行 `语句序列2`，`其他语句序列` 和 `默认语句序列`；
- 如果 `表达式` 的值等于其他 `case` 标签后的值，则从相应的标签处开始执行；
- 如果 `表达式` 的值不等于任何一个 `case` 标签的值，则从 `default` 标签开始执行，即执行 `默认语句序列`。

如果在 `switch` 语句内执行时遇到跳转语句 `break;`，则会跳出 switch 语句。我们将在 [5.4.1](/教程/正文/语法和标准库/5_语句/5_5_跳转语句/5_5_1_break_continue.md) 详细介绍 `break;` 语句的其他功能。

简单来说：**对应 `case` 进，遇到 `break` 出**。

### case 标签

`case` 标签用于标识 `switch` 语句中的一个分支。每个 `case` 标签后面跟着一个整数常量表达式。这些表达式不能重复（比如同一个 `switch` 语句中不能存在两个 `case 1:`）。

```c
int a = 1;
const int b = 2;
constexpr int c = 3;

int value = 3;

switch (a) {
    case a: // 错误：不是常量表达式
    case b: // 错误：即使是 const 对象，也不是常量表达式
    case c: // 正确：使用 constexpr 说明符声明的对象是常量表达式
    case 4: // 正确
    case 2 + 3: // 正确
    case b + 3: // 错误
    case c + 3: // 正确
    case c + 1: // 错误：前面已经有了 case 4:
    default:
}
```

运行结果：该代码块主要用于语法或结构说明，单独运行通常无终端输出。


::: details `case` 标签可以在嵌套的复合语句中吗？

可以，但是强烈不推荐。看下面这段代码：

```c
switch (a) {
    case 1:
        printf("到了 case 1 这个标签\n");
        if (val > 0) {
            printf("进入 if 块\n");
    case 2:
            printf("到了 case 2 这个标签\n");
        }
        printf("从 if 块中出来了\n");
        break;
}
```

可能的输出（示例）：

<TerminalWindow>

<输出与输入或平台相关，请以实际运行为准>

</TerminalWindow>


这种代码**可读性极差**，并且**容易引发逻辑错误**，所以强烈不推荐这种行为。

历史上有个著名的使用**手动循环展开**的编程技巧，用于“高速”地将数据从一个数组复制到另一个数组：

```c
void duffs_device(char *to, char *from, int count) {
    int n = (count + 7) / 8; // 计算需要执行多少轮，每轮处理 8 个字节
    switch (count % 8) { // 计算第一次迭代需要处理的“零头”
        case 0: do { *to++ = *from++;
        case 7:      *to++ = *from++;
        case 6:      *to++ = *from++;
        case 5:      *to++ = *from++;
        case 4:      *to++ = *from++;
        case 3:      *to++ = *from++;
        case 2:      *to++ = *from++;
        case 1:      *to++ = *from++;
                } while (--n > 0);
    }
}
```

运行结果：该代码块主要用于语法或结构说明，单独运行通常无终端输出。


这种技巧**是历史的产物**，在当时可能起到加快运行速度的作用。但在**硬件**和**编译器优化**相当发达的今天，**绝不要**使用这种“技巧”。

:::

### break 和 fallthrough

`break;` 语句在 `switch` 语句中拥有特殊含义：如果在 `switch` 语句的任何位置执行时遇到跳转语句 `break;`，则会跳出 `switch` 语句。

在大多数场景下，我们希望在执行完某个 `case` 后的语句序列后跳出 `switch` 语句，所以在每个 `case` 后的语句序列末尾都需要添加 `break;` 语句。但有例外：有时候，我们希望在执行完某个 `case` 后的语句序列后继续执行下一个 `case` 后的语句序列，这时不加 `break;` 语句。这种情况被称为 **fallthrough**。

在 C23 标准中，有了[属性](/教程/正文/语法和标准库/9_对象/9_6_属性.md)的语法，我们可以在 `case` 标签后使用 `[[fallthrough]];` 语句来提醒编译器：这个 fallthrough 是有意为之，而不是漏写了 `break;`，从而避免可能的警告。

::: details 可能的警告

在 GCC 中，如果编译选项 `-Wimplicit-fallthrough`（包含于 `-Wextra`）被启用，则会在 fallthrough 处发出警告。

:::

示例：

```c
#include <stdio.h>

void func(int x) {
    if (x != 1) {
        putchar('\n');
    }
    printf("func(%d): ", x);
    switch (x) {
        case 1:
            printf("case 1.");
            [[fallthrough]];
        case 2:
            printf("case 2.");
            [[fallthrough]];
        case 3:
            printf("case 3.");
            break;
        case 4:
            printf("case 4.");
            [[fallthrough]];
        case 5:
            [[fallthrough]];
        case 6:
            printf("case 6.");
            break;
    }
}

int main(void) {
    for (int i = 1; i < 7; ++i) func(i);
}
```

可能的输出：

```ansi
func(1): case 1.case 2.case 3.
func(2): case 2.case 3.
func(3): case 3.
func(4): case 4.case 6.
func(5): case 6.
func(6): case 6.
```

运行结果：该代码块主要用于语法或结构说明，单独运行通常无终端输出。


### default 标签

`default` 标签是可选的，用于处理所有未被 `case` 标签匹配的情况。`default` 标签在每个 `switch` 语句中最多只能出现一次，并且可以放在 `switch` 语句内的任何位置（不一定要在最后）。

```c
#include <stdio.h>

void func(int x) {
    if (x != 1) {
        putchar('\n');
    }
    printf("func(%d): ", x);
    switch (x) {
        case 1:
            printf("case 1.");
            break;
        default:
            printf("default.");
            [[fallthrough]];
        case 2:
            printf("case 2.");
    }
}

int main(void) {
    for (int i = 1; i < 4; ++i) func(i);
}
```

可能的输出：

```ansi
func(1): case 1.
func(2): case 2.
func(3): default.case 2.
```

运行结果：该代码块主要用于语法或结构说明，单独运行通常无终端输出。


## 注意事项与应用

### 作用域问题

`case` 只是标签，无法创造它自己的作用域，所以其内的标识符直到 `switch` 语句结束都是可见的。然而，由于 `switch-case` 语句可能跳过某些语句的执行，可能跳过对象的初始化，后续访问未初始化的对象可能导致未定义行为。

```c
#include <stdio.h>
int main() {
    int i = 2;
    switch (i) {
        case 1:
            int x = 100;
            printf("%d", x);
            break;
        case 2:
            printf("%d", x); // 错误：'x' is used uninitialized 而非 'x' undeclared
            break;
    }
    return 0;
}
```

可能的输出（示例）：

<TerminalWindow>

<输出与输入或平台相关，请以实际运行为准>

</TerminalWindow>


很多时候，我们在 `case` 标签末尾加上一个大括号包裹要执行的语句，成为一个复合语句，把标识符的作用范围限制在更小的范围内。

### 空标签

空的 `case` 标签是合法的，常用于多个值共享同一处理逻辑。例如：

```c
switch (ch) {
    case ' ':
    case '\t':
    case '\n':
        // 处理空白字符
        break;
}
```

运行结果：该代码块主要用于语法或结构说明，单独运行通常无终端输出。


### 有限状态自动机

`switch` 语句根据不同的条件执行不同的操作，经常用于有限状态自动机。

状态机示例：

```c
#include <stdio.h>
#include <ctype.h>
#include <string.h>

// 解析器状态
enum ParseState {
    STATE_START,        // 开始状态
    STATE_OBJECT_START, // 对象开始 {
    STATE_KEY,          // 解析键名
    STATE_COLON,        // 等待冒号
    STATE_VALUE,        // 解析值
    STATE_STRING,       // 字符串值
    STATE_NUMBER,       // 数字值
    STATE_COMMA,        // 等待逗号或结束
    STATE_OBJECT_END,   // 对象结束 }
    STATE_ERROR,        // 错误状态
    STATE_COMPLETE      // 解析完成
};

void parse_json(const char* json_str) {
    enum ParseState state = STATE_START;
    int pos = 0;
    int len = strlen(json_str);
    char current_key[100] = {0};
    char current_value[100] = {0};
    int key_pos = 0, value_pos = 0;

    printf("解析 JSON: %s\n", json_str);
    printf("状态转换过程：\n");

    while (pos <= len && state != STATE_COMPLETE && state != STATE_ERROR) {
        char ch = (pos < len) ? json_str[pos] : '\0';

        switch (state) {
            case STATE_START:
                printf("  [START] 字符：'%c'\n", ch);
                if (ch == '{') {
                    state = STATE_OBJECT_START;
                    printf("    -> 转换到 OBJECT_START\n");
                } else if (!isspace(ch)) {
                    state = STATE_ERROR;
                }
                break;

            case STATE_OBJECT_START:
                printf("  [OBJECT_START] 字符：'%c'\n", ch);
                if (ch == '"') {
                    state = STATE_KEY;
                    key_pos = 0;
                    printf("    -> 转换到 KEY\n");
                } else if (ch == '}') {
                    state = STATE_COMPLETE;  // 空对象
                    printf("    -> 空对象，解析完成\n");
                } else if (!isspace(ch)) {
                    state = STATE_ERROR;
                }
                break;

            case STATE_KEY:
                if (ch == '"') {
                    current_key[key_pos] = '\0';
                    state = STATE_COLON;
                    printf("  [KEY] 解析到键：\"%s\" -> 转换到 COLON\n", current_key);
                } else if (ch != '\0') {
                    if (key_pos < 99) current_key[key_pos++] = ch;
                } else {
                    state = STATE_ERROR;
                }
                break;

            case STATE_COLON:
                printf("  [COLON] 字符：'%c'\n", ch);
                if (ch == ':') {
                    state = STATE_VALUE;
                    printf("    -> 转换到 VALUE\n");
                } else if (!isspace(ch)) {
                    state = STATE_ERROR;
                }
                break;

            case STATE_VALUE:
                printf("  [VALUE] 字符：'%c'\n", ch);
                if (ch == '"') {
                    state = STATE_STRING;
                    value_pos = 0;
                    printf("    -> 转换到 STRING\n");
                } else if (isdigit(ch) || ch == '-') {
                    state = STATE_NUMBER;
                    value_pos = 0;
                    current_value[value_pos++] = ch;
                    printf("    -> 转换到 NUMBER\n");
                } else if (!isspace(ch)) {
                    state = STATE_ERROR;
                }
                break;

            case STATE_STRING:
                if (ch == '"') {
                    current_value[value_pos] = '\0';
                    printf("  [STRING] 解析到字符串值：\"%s\"\n", current_value);
                    printf("    -> 键值对：\"%s\": \"%s\"\n", current_key, current_value);
                    state = STATE_COMMA;
                } else if (ch != '\0') {
                    if (value_pos < 99) current_value[value_pos++] = ch;
                } else {
                    state = STATE_ERROR;
                }
                break;

            case STATE_NUMBER:
                if (isdigit(ch)) {
                    if (value_pos < 99) current_value[value_pos++] = ch;
                } else {
                    current_value[value_pos] = '\0';
                    printf("  [NUMBER] 解析到数字值：%s\n", current_value);
                    printf("    -> 键值对：\"%s\": %s\n", current_key, current_value);
                    state = STATE_COMMA;
                    pos--;  // 回退一个字符，重新处理
                }
                break;

            case STATE_COMMA:
                printf("  [COMMA] 字符：'%c'\n", ch);
                if (ch == ',') {
                    state = STATE_OBJECT_START;  // 继续解析下一个键值对
                    printf("    -> 继续解析，转换到 OBJECT_START\n");
                } else if (ch == '}') {
                    state = STATE_OBJECT_END;
                    printf("    -> 转换到 OBJECT_END\n");
                } else if (!isspace(ch)) {
                    state = STATE_ERROR;
                }
                break;

            case STATE_OBJECT_END:
                printf("  [OBJECT_END] 解析完成\n");
                if (ch == '\0' || isspace(ch)) {
                    state = STATE_COMPLETE;
                } else {
                    state = STATE_ERROR;
                }
                break;

            case STATE_ERROR:
                printf("  [ERROR] 解析错误，位置：%d, 字符：'%c'\n", pos, ch);
                return;

            case STATE_COMPLETE:
                printf("  [COMPLETE] JSON 解析成功完成！\n");
                return;
        }

        pos++;
    }

    if (state == STATE_COMPLETE) {
        printf("JSON 解析成功！\n");
    } else {
        printf("JSON 解析失败，最终状态：%d\n", state);
    }
}

int main() {
    printf("=== JSON 解析器状态机演示 ===\n\n");

    // 测试用例 1：简单的键值对
    printf("测试 1: 简单对象\n");
    parse_json("{\"name\": \"Alice\"}");

    printf("\n" "=" * 50 "\n");

    // 测试用例 2：多个键值对
    printf("测试 2: 多个键值对\n");
    parse_json("{\"name\": \"Bob\", \"age\": 25}");

    printf("\n" "=" * 50 "\n");

    // 测试用例 3：空对象
    printf("测试 3: 空对象\n");
    parse_json("{}");

    printf("\n" "=" * 50 "\n");

    // 测试用例 4：错误的 JSON
    printf("测试 4: 错误的 JSON\n");
    parse_json("{\"name\" \"Alice\"}");  // 缺少冒号

    return 0;
}
```

可能的输出（示例）：

<TerminalWindow>

<输出与输入或平台相关，请以实际运行为准>

</TerminalWindow>


::: details 不适用的情况

当今市面上的一些教程在讲 `switch` 语句的时候，使用的一些示例代码中的情境实际上是不适用 `switch` 语句去处理的。具体地，`switch` 语句只适用于：

- 有限种状态（尤其是[枚举](/教程/正文/语法和标准库/8_枚举.md)）
- 每种状态的代码逻辑不同

的情况。如果不满足以上两点，很可能有更适合的处理方法。下面举两个例子：

**1. 成绩等级：**

`switch` 不适合用来处理范围。某些教程使用如下的代码，借助除以 10 之后的数进行处理：

```c
#include <stdio.h>
int main() {
    int score = 0;
    printf("请输入成绩：");
    scanf("%d", &score);
    switch (score / 10) {
        case 10:
        case 9:
            printf("成绩等级：A\n");
            break;
        case 8:
            printf("成绩等级：B\n");
            break;
        case 7:
            printf("成绩等级：C\n");
            break;
        case 6:
            printf("成绩等级：D\n");
            break;
        default:
            printf("成绩等级：E\n");
            break;
    }
    return 0;
}
```

可能的输出（示例）：

<TerminalWindow>

<输出与输入或平台相关，请以实际运行为准>

</TerminalWindow>


更合适的写法（使用 `if-else` 语句）：

```c
#include <stdio.h>

int main() {
    int score = 0;
    printf("请输入成绩：");
    scanf("%d", &score);
    if (score >= 90) {
        printf("成绩等级：A\n");
    } else if (score >= 80) {
        printf("成绩等级：B\n");
    } else if (score >= 70) {
        printf("成绩等级：C\n");
    } else if (score >= 60) {
        printf("成绩等级：D\n");
    } else {
        printf("成绩等级：E\n");
    }
    return 0;
}
```

可能的输出（示例）：

<TerminalWindow>

<输出与输入或平台相关，请以实际运行为准>

</TerminalWindow>


**2. 简单的菜单选择：**

如果菜单选择的选项较多，使用 `switch` 语句会比较冗长：

```c
#include <stdio.h>

void show_menu(void){ /* 一些操作 */ }
void add_student(void){ /* 一些操作 */ }
void delete_student(void){ /* 一些操作 */ }
void query_student(void){ /* 一些操作 */ }
// other functions...

int main() {
    int choice = 0;
    printf("请选择操作：\n");
    printf("1. 显示菜单\n");
    printf("2. 添加学生\n");
    printf("3. 删除学生\n");
    printf("4. 查询学生\n");
    // others...

    scanf("%d", &choice);
    switch (choice) {
        case 1:
            show_menu();
            break;
        case 2:
            add_student();
            break;
        case 3:
            delete_student();
            break;
        case 4:
            query_student();
            break;
        // other cases...

        default:
            printf("无效的选择\n");
            break;
    }
    return 0;
}
```

可能的输出（示例）：

<TerminalWindow>

<输出与输入或平台相关，请以实际运行为准>

</TerminalWindow>


更合适的写法（使用**表驱动**，涉及到函数指针）：

```c
#include <stdio.h>

void show_menu(void){ /* 一些操作 */ }
void add_student(void){ /* 一些操作 */ }
void delete_student(void){ /* 一些操作 */ }
void query_student(void){ /* 一些操作 */ }
// other functions...

static void (*menu[])(void) = {
    show_menu,
    add_student,
    delete_student,
    query_student,
    // other pointers...
};

int main() {
    int choice = 0;
    printf("请选择操作：\n");
    printf("1. 显示菜单\n");
    printf("2. 添加学生\n");
    printf("3. 删除学生\n");
    printf("4. 查询学生\n");
    // others...

    scanf("%d", &choice);
    if(choice >= 1 && choice <= 4){
        menu[choice - 1]();
    } else {
        printf("无效的选择\n");
    }
    return 0;
}
```

可能的输出（示例）：

<TerminalWindow>

<输出与输入或平台相关，请以实际运行为准>

</TerminalWindow>


如果功能可以分类，推荐使用分层菜单让逻辑更清晰，也提高用户的使用体验。

:::
