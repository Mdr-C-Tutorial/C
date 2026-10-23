# 17.7 `#line`

`#line` 指令用于修改后续代码中 `__LINE__` 与 `__FILE__` 的逻辑值。它主要服务于“代码生成器”场景：当源代码由工具生成时，编译诊断可以映射回原始输入文件。

## 1. 基本形式

```c
#line 120 "schema.dsl"
```

从这一行之后，预处理器会把行号视为 `120`，文件名视为 `schema.dsl`。编译器报错、`assert` 信息、调试输出中的位置元数据也会随之变化。

## 2. 示例

```c
#include <stdio.h>

int main(void) {
#line 42 "generated_from_template.c"
    printf("%s:%d\n", __FILE__, __LINE__);
    return 0;
}
```

输出：

::: terminal

generated_from_template.c:42

:::

## 3. 生成代码中的常见用法

很多代码生成器会在生成片段前插入 `#line`，把后续诊断映射回模板或 DSL 文件。这样当编译器报告某行语法错误时，用户看到的是原始输入位置，而不是中间产物位置。

```c
#line 8 "model.idl"
struct person {
    int id;
    const char *name;
};
```

如果这段定义触发诊断，报错位置会落到 `model.idl` 的对应行。

## 4. 指令形式补充

`#line` 既可以只给出行号，也可以同时给出文件名。两种形式都作用于“后续行”的逻辑位置元数据。生成器若只需要校正行号，可使用短形式；若还需要映射来源文件，再补充文件名参数。

## 5. 习题

<Exercise id="11726" :d="2" :w="2">

阅读程序，先不运行，写出五行输出：

```c
#include <stdio.h>

int main(void) {
    printf("%d %s\n", __LINE__, __FILE__);
#line 100 "demo.c"
    printf("%d %s\n", __LINE__, __FILE__);
#line 7
    printf("%d %s\n", __LINE__, __FILE__);
#line 20 "gen.c"
    printf("%d %s\n", __LINE__, __FILE__);
    printf("%d %s\n", __LINE__, __FILE__);
    return 0;
}
```

1. 假设源文件名是 `demo.c`，写出五行输出。
2. `#line` 给的行号作用于它自己这一行还是下一行？用第一行与第二行的输出说明。
3. 第三行输出里的文件名为什么还是 `demo.c`，而不是回到某个默认值？短形式与完整形式差在哪里？
4. `#line 20 "gen.c"` 之后有两条相同的 `printf`，为什么它们的行号不同？
5. 把源文件改名成 `main.c` 重新编译，五行输出里哪几行变化、哪几行不变？

</Exercise>

<Exercise id="11727" :d="3" :w="3">

下面是一段「由模板生成」的代码：`stats.model` 里写着生成的函数，`main` 在真正的源文件里。

```c
/* stats.inc，模拟生成器的产物 */
#line 1 "stats.model"
static int check_positive(int raw) {
    assert(raw >= 0);
    return raw;
}
```

```c
/* main.c */
#include <assert.h>
#include <stdio.h>
#include "stats.inc"

int main(void) {
    fprintf(stderr, "%s:%d\n", __FILE__, __LINE__);
    check_positive(3);
    check_positive(-1);
    return 0;
}
```

1. 运行程序，写出 `assert` 失败时的完整诊断（文件与行号），并说明这个位置对应 `stats.inc` 的第几行、`stats.model` 的第几行。
2. 去掉 `stats.inc` 里的 `#line`，重新编译运行，断言报出的文件名与行号变成什么？两种情况下「文件第几行」与「报出来的行号」是一回事吗？
3. `main.c` 里那行 `fprintf` 打印出的位置受 `stats.inc` 中 `#line` 的影响吗？把这一行搬进 `stats.inc`、放在 `#line 1 "stats.model"` 之后，再运行一次，两处输出各是什么？这说明 `#line` 的作用在 `#include` 结束处回到了哪个文件。
4. 把 `#line 1 "stats.model"` 换成 `#line 1`，`assert` 报出的文件名是什么？两种形式里文件名参数各有什么要求？
5. 生成器如果希望诊断指向模板里的第 40 行而不是第 1 行，应该怎么写？说明为什么生成器要为每个片段单独写一条 `#line`，而不是在文件开头写一条就够。

</Exercise>

<Exercise id="11728" :d="3" :w="3">

`__FILE__`、`__LINE__` 是预定义宏，`__func__` 是函数内的标识符。`#line` 只改前两个：

```c
#include <stdio.h>

static const char *where(void) {
    return __FILE__;
}

int main(void) {
    printf("%s %d\n", where(), __LINE__);
#line 1 "virtual.c"
    printf("%s %d\n", where(), __LINE__);
    printf("%s %d\n", __func__, __LINE__);
    return 0;
}
```

1. 写出三行输出。第二行的 `where()` 为什么仍返回原来的文件名？
2. `__func__` 是否受 `#line` 影响？说明理由。
3. 定义 `#define HERE __LINE__`，在 `#line 100` 前后各用一次 `HERE`，输出分别是什么？宏体里的 `__LINE__` 在什么时候展开？
4. 把 `__LINE__` 拼进标识符（`#define CAT2(a, b) a##b`、`#define CAT(a, b) CAT2(a, b)`、`#define UNIQUE(p) CAT(p, __LINE__)`）做「按行号生成不重名的标识符」。在一份被 `#include` 两次的片段里各放一句 `static int UNIQUE(tmp_) = 1;`，先让片段开头写 `#line 40 "part.inc"`，两个 `tmp_` 会重名吗？记录报错，再想一个不受 `#line` 干扰的修法（提示：`__COUNTER__` 或加上不同的前缀）。

</Exercise>

<Exercise id="11729" :d="5" :w="4">

写一个最小的代码生成器，把一张表的数据变成 C 初始化器，并让所有诊断指回数据文件。

数据文件 `sensors.dsl` 每行是 `名称 数值`：

```text
temp 25
humidity 60
pressure 1013
```

1. 写 `gen`，命令行接输入文件与输出文件两个参数，把每行转成 `    {"名称", 数值, __FILE__, __LINE__},` 并写进输出文件，每条记录前先写一行 `#line`，让该记录的 `__FILE__`、`__LINE__` 指向 `sensors.dsl` 的原行。格式不对的行向标准错误报告文件名与行号后跳过。
2. 主程序 `#include` 这个片段，放进 `struct row { const char *name; int value; const char *file; int line; }` 的数组，遍历打印 `名称 = 数值 (文件:行)`。期望输出：

```text
temp = 25 (sensors.dsl:1)
humidity = 60 (sensors.dsl:2)
pressure = 1013 (sensors.dsl:3)
```

3. 故意把片段里 `humidity` 那条记录的数值改成字符串字面量 `"oops"`，重新编译，抄下编译器报出的文件名与行号，确认它指向 `sensors.dsl` 的第 2 行而不是输出文件。
4. 生成器如果只在片段开头写一条 `#line 1 "sensors.dsl"`、后面不再写，会怎样？先按每行一条记录生成一次，编译看报错行号；再让每条记录占两行（名称一行、数值一行），重新生成，看报错落在第几行。据此说明为什么生成器要为每条记录都写一条 `#line`。
5. 把 `#line` 里的文件名去掉（只留数字），第 3 步的报错显示成什么？这一现象对「只想改行号、不想暴露内部文件名」的生成器有什么用？
6. 记录里的位置是生成时定死的：生成器把 `__LINE__` 对应的数字写进 `#line`，编译器照着这个数字展开，与数据文件后来的内容无关。往 `sensors.dsl` 中间插入一行而忘记重新生成，运行时打印的行号就全错。说明为什么会这样，并给出一种让这种偏差尽早暴露的办法（提示：让生成器把数据文件的总行数或每行的校验值也写进片段，用 `_Static_assert` 或运行时比对）。

</Exercise>
