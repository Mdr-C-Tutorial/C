# 12.4 switch 与状态机

本节把已经学过的 `switch` 与枚举、结构、字符串和函数接口组合起来。先掌握第 4 章的基本 `switch` 语义，再阅读下面的应用。

## 1. 有限状态自动机

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

如果功能可以分类，推荐使用分层菜单让逻辑更清晰，也提高用户的使用体验。

::::

## 2. 习题

### 2.1 巩固

<Exercise id="11212" :d="3" :w="2" multi>

第 1 节的 `parse_json` 处理输入 `{"a": 1}` 时，状态依次经过 `START`、`OBJECT_START`、`KEY`、`COLON`、`VALUE`、`NUMBER`、`COMMA`、`OBJECT_END`、`COMPLETE`。

(1) _[2.2]_ 写出 `VALUE` 状态被访问了几次，每次面对的字符是什么。为什么空白字符不会让状态改变。

(2) _[2.2]_ `NUMBER` 分支在读到 `'}'` 时先打印键值对，再执行 `pos--`，然后跳出 `switch`。若删掉 `pos--`，`'}'` 会在哪一步被处理，程序最后处于什么状态？用第 1 节测试 2 `{"name": "Bob", "age": 25}` 的结果说明。

(3) _[2.1]_ `OBJECT_START` 状态既在读到 `'{'` 之后使用，也在读到 `','` 之后使用。对输入 `{}`，它看到 `'}'` 时走到哪个状态，程序输出哪一行表示解析完成？

(4) _[2.1]_ 输入 `{"a": 1}x` 时，`OBJECT_END` 看到 `'x'` 后进入 `ERROR`。程序打印的最终状态数字是多少，这个数字由什么决定？

(5) _[3.2]_ `switch` 里 `case STATE_ERROR` 与 `case STATE_COMPLETE` 两段都在打印信息，但运行第 1 节的四个测试时这两段一次也没有执行。结合 `while` 的条件说明原因，并给出两种改法：一种让循环在进入这两个状态后仍执行一次，另一种把打印挪到循环之后。两种改法对第 1 节四个测试的输出各有什么影响？

</Exercise>

### 2.2 编程

<Exercise id="11213" :d="3" :w="3">

第 1 节末尾的「不适用的情况」指出 `switch` 不适合处理范围，也不适合选项很多的菜单。用两段小程序验证这两点。

第一部分：成绩等级。实现两个函数：

```c
const char* grade_switch(int score); /* 用 score / 10 的 switch，与正文的写法相同 */
const char* grade_if(int score);     /* 用 if / else if */
```

(1) 对 `85`、`105`、`110`、`-5` 四个值分别调用两个函数，列表写出返回值。哪几个值上两者不一致？

(2) 解释不一致的原因：`110 / 10` 得到什么，有没有 `case` 命中，落到哪个分支；`105 / 10` 又为什么恰好一致。据此说明为什么正文说 `switch` 不适合处理范围。

(3) 把及格线从 60 改成 65，即 `60`～`64` 判为 `E`、`65`～`69` 仍要判为 `D`。`grade_if` 只改一个常量即可。`grade_switch` 不能只把 `case 6` 改成别的标签：除以 10 之后，60～69 都落在同一个值上，无法区分。可行的改法是缩小除数，例如改成 `score / 5`，让 65 落在 `13` 上，再把各个 `case` 重新对应。写出 `score / 5` 版本的 `grade_switch`，用 55、60、65、70、85、90 六个值验证它与 `grade_if` 一致，并说明为什么 `case` 的个数从 5 个变成了 8 个。这一点正是正文说「`switch` 不适合处理范围」的实际代价。

第二部分：命令菜单。用函数指针表实现一个小型命令分发器，命令有 `add`、`del`、`list`，分发器支持 `help` 与 `quit`：

```c
typedef void (*handler_fn)(const char* arg);
```

要求：

1. 用结构体数组保存命令名、用法串与处理函数。`add`、`del` 的处理函数打印 `add: 42`、`del: 7` 这样的结果；`list` 不需要参数，只打印 `list`。
2. 分发函数接收一整行命令：

   ```c
   int dispatch(char *line); /* 返回 0 表示应结束程序，返回 1 表示继续 */
   ```

   命令名与参数之间用一个或多个空格分隔，行首空白忽略，行尾的换行符由调用方先去掉。`help` 打印三行用法，未知命令打印 `unknown command: <名字>`，空行忽略，`quit` 返回 0。`dispatch` 可以就地修改这一行，把命令名与参数之间改成终止空字符。
3. 在 `main` 里依次分发下面这些行（不必从标准输入读）：

   ```c
   char line1[] = "add 42";
   char line2[] = "  del 7";
   char line3[] = "list";
   char line4[] = "help";
   char line5[] = "foo 1";
   char line6[] = "quit";
   ```

   期望输出：

   ```text
   add: 42
   del: 7
   list
   add <id>
   del <id>
   list
   unknown command: foo
   ```

   分发 `line6` 之后不再有输出。

4. 增加第四条命令 `show <id>`。分别说明在表驱动版本和 `switch` 版本里各要改几处、哪些地方容易漏改，据此回答正文为什么说选项多时 `switch` 比较冗长。

</Exercise>

<Exercise id="11214" :d="4" :w="3" multi>

第 1 节的解析器能通过四个测试，但对一些不合法的输入也报告成功。用下面的表逐个运行原程序：

```text
{"a": 1}          {"a": -}          {"a": 1,}
{"a": 01}         {"a": 1}x         {"a": "x"}
{"a": 1.5}        {"a": true}       {"": 1}
```

(1) _[2.3]_ 把每个输入的最终结果（成功或最终状态数字）填成一张表。哪些输入本应被拒绝却被接受？

(2) _[3.2]_ `{"a": -}` 被接受，是因为 `NUMBER` 分支不检查已经读到几个数字。指出需要新增哪个对象、在哪些位置更新它，并说明为什么 `-` 后面没有数字时应当在读到 `'}'` 时进入 `ERROR`。

(3) _[3.2]_ `{"a": 1,}` 被接受，是因为 `OBJECT_START` 无法区分「对象刚开头」与「刚读完一个逗号」。给出最小改法：增加一个标记，在哪种转换时置位、在哪种转换时清零。

(4) _[4.3]_ 修好这两处，并让 `01` 也被拒绝（第一位是 `0` 时后面不能再跟数字）。用第 (1) 问的表重新验收，写出新的结果表，其中合法输入只有 `{"a": 1}`、`{"a": "x"}`、`{"": 1}`。

(5) _[3.1]_ 修复后给 `enum ParseState` 增加一个 `STATE_A` 常量，但不加对应的 `case`。用 `-Wall -Wextra` 编译，贴出警告，说明这个警告为什么在把 `switch` 用作状态机时有用。

</Exercise>

### 2.3 综合与挑战

<Exercise id="11215" :d="5" :w="4">

以第 1 节的解析器为基础，扩展成一个能识别小数与字面量的小型 JSON 对象校验器。

要求：

1. 保留「枚举状态 + `switch`」的结构，在原有状态上增加处理小数部分与 `true`、`false`、`null` 的状态，不要改成递归下降。
2. 数字支持可选的负号、整数部分、可选的小数点与至少一位小数。整数部分为 `0` 时后面不能再跟数字；小数点后没有数字视为错误。
3. 字符串支持 `\"` 与 `\\` 两种转义，其余反斜杠开头视为错误。
4. 字面量必须完整匹配 `true`、`false`、`null`，后面紧跟字母也算错误。
5. 接口与输出：

   ```c
   struct parse_result {
       int ok;          /* 成功为 1，失败为 0 */
       size_t error_pos; /* 失败时第一个出错字符的下标，从 0 开始；成功时为 0 */
   };

   struct parse_result parse_object_ext(const char* text);
   ```

   失败时还必须打印出错的字符及其下标；成功时打印每个键值对。

6. 用下面的表验收（`ok` 表示应当接受，`pos` 是期望的出错下标）：

   | 输入 | 结果 |
   | --- | --- |
   | `{"a": 1.5}` | ok |
   | `{"a": -0.25}` | ok |
   | `{"a": 1.}` | pos 8 |
   | `{"a": .5}` | pos 6 |
   | `{"a": true}` | ok |
   | `{"a": false}` | ok |
   | `{"a": null}` | ok |
   | `{"a": tru}` | pos 9 |
   | `{"a": truex}` | pos 10 |
   | `{"a": true, "b": 2}` | ok |
   | `{"a": 1}x` | pos 8 |
   | `{"a": 0}` | ok |

   表里每个输入要么整个被接受，要么在给定下标处停下。程序边解析边打印键值对是允许的，但 `ok` 字段必须与表一致。

7. 统计扩展后的状态数，与原来的 11 个对比。说明新增了哪几类状态、每类各解决什么问题。

8. _[5.3*]_ 把 `switch` 版改写成状态转移表：行是状态，列是字符类别（空白、引号、冒号、逗号、数字、负号、字母、`}`、`{`、其他），表项是下一状态与动作。用第 6 问的表逐条比较两版结果，必须完全一致。比较两版源文件的长度，说明状态继续增多时哪一种更容易维护，以及表驱动版本的代价在哪里。

</Exercise>
