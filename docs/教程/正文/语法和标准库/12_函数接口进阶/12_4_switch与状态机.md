# 12.4 switch 与状态机

本节把已经学过的 `switch` 与枚举、结构、字符串和函数接口组合起来。先掌握第 4 章的基本 `switch` 语义，再阅读下面的应用。

## 有限状态自动机

`switch` 语句根据不同的条件执行不同的操作，经常用于有限状态自动机。

::: tip 关于 `enum`

下面的状态机示例使用了 `enum`，它用于定义一组命名的整数常量；枚举语法已经在第 10 章说明。

:::

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

    printf("\n==================================================\n");

    // 测试用例 2：多个键值对
    printf("测试 2: 多个键值对\n");
    parse_json("{\"name\": \"Bob\", \"age\": 25}");

    printf("\n==================================================\n");

    // 测试用例 3：空对象
    printf("测试 3: 空对象\n");
    parse_json("{}");

    printf("\n==================================================\n");

    // 测试用例 4：错误的 JSON
    printf("测试 4: 错误的 JSON\n");
    parse_json("{\"name\" \"Alice\"}");  // 缺少冒号

    return 0;
}
```

程序会打印每次状态转换。关键结果是：前三个测试分别完成解析，第四个测试因缺少冒号而失败。

::: terminal

测试 1: 简单对象
JSON 解析成功！

测试 2: 多个键值对
JSON 解析成功！

测试 3: 空对象
JSON 解析成功！

测试 4: 错误的 JSON
JSON 解析失败，最终状态：9

:::
:::: details 不适用的情况

当今市面上的一些教程在讲 `switch` 语句的时候，所用的某些示例情境实际上不适合用 `switch` 语句处理。具体地，`switch` 语句只适用于：

- 有限种状态（尤其是[枚举](/教程/正文/语法和标准库/10_枚举_结构与联合/10_1_枚举)）
- 每种状态的代码逻辑不同

的情况。如果不满足以上两点，很可能有更适合的处理方法。下面举两个例子：

**1. 成绩等级：**

`switch` 不适合用来处理范围。某些教程使用如下的代码，借助除以 10 之后的数进行处理：

::: code-group

```c [不推荐：switch]
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

```c [推荐：if / else if]
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

:::

例如输入 `85` 时，两段程序都会得到：

::: terminal

请输入成绩：85
成绩等级：B

:::
**2. 简单的菜单选择：**

如果菜单选择的选项较多，使用 `switch` 语句会比较冗长：

::: code-group

```c [switch]
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

```c [表驱动]
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

:::

示例省略了四个操作函数的具体实现，因此这里不虚构操作结果；代码组对比的是两种分派结构。

如果功能可以分类，推荐使用分层菜单让逻辑更清晰，也提高用户的使用体验。

::::
