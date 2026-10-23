# 17.4 `#error` 和 `#warning`

## 1. `#error`

`#error` 指令用于在编译时显示指定的错误信息，使得程序 ill-formed，然后中止编译。

它常用于“前提检查”，例如编译器版本不满足、目标平台不在支持列表、关键宏缺失等。与运行时检查不同，`#error` 会把问题提前到构建阶段暴露出来。

## 2. `#warning`

`#warning` 指令用于在编译时显示指定的警告信息，不影响程序的有效性，继续编译。

`#warning` 在 C23 才进入标准；更早版本里它通常是编译器扩展。若项目要求严格 C11/C17 兼容，可改用条件编译 + 注释提示，或使用构建系统输出警告。

## 3. 示例

```c
#if __STDC__ != 1
#  error "Not a standard compiler"
#endif

#if __STDC_VERSION__ >= 202311L
#  warning "Using C23"
#endif

#include <stdio.h>

int main(void) {
    printf("Hello, world!\n");
    return 0;
}
```

在 C23 模式下，编译器必须为 `#warning` 发出诊断，但诊断文字由实现决定。如果编译继续并生成可执行文件，运行时输出为：

::: terminal

Hello, world!

:::

## 4. 习题

<Exercise id="11714" :d="2" :w="2">

下面三段小程序用来观察 `#error` 与 `#warning` 的差别。先写出预期（能否生成 `.o`、退出码是 0 还是非 0、诊断出现在哪一步），再用 `gcc -std=c17 -c` 逐个验证。

```c
/* a.c */
#define NEED_LEVEL 3
#if NEED_LEVEL < 4
#error NEED_LEVEL is too small
#endif
int main(void) { return 0; }
```

```c
/* b.c */
#warning "this configuration is not supported"
int main(void) { return 0; }
```

```c
/* c.c */
#if 0
#error hidden branch
#warning hidden warning
#endif
int main(void) { return 0; }
```

1. 三段的退出码与 `.o` 是否存在，做成一张表。
2. `a.c` 里 `#error` 后面的文字写成 `NEED_LEVEL is too small`，诊断里显示的是宏名还是它的值？为什么？`#error` 的记号序列与 `#if` 的表达式处理方式有什么不同？
3. `b.c` 用 `-std=c17 -pedantic` 编译会多出一条什么诊断？`-std=c2x` 下这条还在吗？据此说明正文「`#warning` 在 C23 才进入标准」这句话在 GCC 13.2 上的具体表现。
4. `c.c` 里的 `#error` 为什么没有触发？如果把 `#if 0` 改成 `#if 1`，两段诊断的先后顺序是什么？
5. 把 `b.c` 的 `#warning` 换成 `#error`，重新编译，说明「诊断出现」与「编译中止」这两件事在实现里是分开做的：`#error` 报出后编译器是否立即停下？用同时含一个 `#error` 和一处语法错误的文件验证，看两条错误是否都出现。

</Exercise>

<Exercise id="11715" :d="3" :w="3">

为一个库写构建期检查头 `bc.h`，把「这个库只在什么环境下能编」写成一组 `#error`。

要求：

1. 不是宿主实现（`__STDC_HOSTED__ != 1`）时 `#error`，消息写明需要 hosted 实现。
2. `CHAR_BIT != 8` 时 `#error`，消息写明假定 8 位字节。
3. 没有 `__STDC_VERSION__` 或低于 `201112L` 时 `#error`，消息写明需要 C11。
4. 没有 `__has_include` 时 `#error`；有它但找不到 `<stdint.h>` 时另给一条 `#error`。这两条要写成嵌套。
5. 未定义 `NDEBUG` 时 `#warning` 提示这是调试构建。
6. 用 `_Static_assert` 再加两条运行不出来的检查：`int` 至少 32 位，指针至少 32 位。

再写一个只包含 `bc.h` 的 `use.c`，在下面这些配置下各编一次，把诊断与退出码记下来：

```text
gcc -std=c11 -Wall -Wextra -c use.c
gcc -std=c17 -Wall -Wextra -c use.c
gcc -std=c17 -Wall -Wextra -DNDEBUG -c use.c
gcc -std=c17 -Wall -Wextra -D__STDC_HOSTED__=0 -c use.c
gcc -std=c17 -Wall -Wextra -U__has_include -c use.c
gcc -std=c89 -Wall -Wextra -c use.c
```

回答：

1. 最后一行（`-std=c89`）只有 1 条错误，加上 `-pedantic` 仍是 1 条，加上 `-pedantic-errors` 却变成 4 条。把这 4 条抄下来，逐条说明各来自哪一行。第 6 条的两个 `_Static_assert` 在纯 C90 下一条诊断都没有，为什么必须加 `-pedantic` 才看得见？这也说明为什么「用 `_Static_assert` 做版本检查」在有 C90 目标的工程里靠不住。
2. `-D__STDC_HOSTED__=0` 时，`__STDC_HOSTED__` 本来由编译器预定义，命令行覆盖它会产生什么额外提示？被覆盖后用 `-dM` 看到的值是多少？这说明「用 `-D` 覆盖实现预定义宏」这种做法有什么代价？
3. 如果一个头文件把 `#error` 写在条件之外（不加 `#if` 保护），引用它的每个 `.c` 都会各报一次。用两个 `.c` 各包含一次，数一数诊断条数；换成带包含保护的头文件再数一次。为什么加保护不能减少条数（提示：每个翻译单元都从零开始预处理，守护宏在那个翻译单元里也是第一次定义）？
4. 把第 5 条的 `#warning` 换成 `#error`，讨论一个库在头文件里用 `#error` 检查 `NDEBUG` 是不是好主意。再想一层：`bc.h` 的每条 `#error` 都针对「做不到就没法编」的硬条件，而 `#warning` 针对「能编但可能有麻烦」的软条件，这条界线对一个库的头文件意味着什么？

验收：六次编译的诊断条数与退出码与你的分析一致；`-std=c17 -DNDEBUG` 那次零诊断、退出码 0。

</Exercise>

<Exercise id="11716" :d="4" :w="3">

`#warning` 在 C17 下是扩展，在 C23 下才是标准。写一个 `bw.h`，让调用方无论用哪种标准、加上什么警告选项，都能得到一条「构建期提示」。

要求：

1. 在 `bw.h` 里定义 `BUILD_WARNING(msg)`。调用方写 `BUILD_WARNING("old config path")`，编译时看到 `old config path` 这条提示。
2. 实现使用 `_Pragma`，不能依赖 C23 的 `#warning`；`-std=c17 -pedantic` 下不能出现「before C2X is a GCC extension」。
3. 同一个头文件在 `-std=c2x` 下也不得改变行为。
4. 给出两个版本并比较：直接把 `_Pragma("GCC warning \"msg\"")` 写进宏，与用 `DO_PRAGMA(x) _Pragma(#x)` 加 `DO_PRAGMA(GCC warning msg)` 中转。两者对 `"old config path"` 分别输出什么？

验证矩阵（每条命令各跑一次，记下诊断文字与退出码）：

```text
gcc -std=c17 -pedantic -c a.c
gcc -std=c2x -c a.c
gcc -std=c17 -Wall -Wextra -Werror -c a.c
gcc -std=c17 -Wall -Wextra -Werror -Wno-error=cpp -c a.c
gcc -std=c17 -Wall -Wextra -Werror -Wno-cpp -c a.c
```

回答：

1. 直接写 `_Pragma("GCC warning \"msg\"")` 的版本为什么把提示文字变成了字面的 `msg`？`#x` 在这里把什么变成了字符串？
2. 在这个场景里 `#warning` 与 `_Pragma` 有一个关键差别：`#warning` 受 `-Wcpp` 控制，`-Wno-error=cpp` 能把它降级；`_Pragma` 版产生的是普通 `-Werror`，不受 `-Wcpp` 约束。用矩阵里的第三条、第四条、第五条确认：`-Wno-error=cpp` 与 `-Wno-cpp` 能否把 `_Pragma` 版的提示降级？哪些选项才能真正让构建通过？
3. 如果调用方用 `-Werror` 编译整个工程，这个提示会让构建失败。至少给出两种让库作者既能提示、又不破坏调用方 `-Werror` 的做法（提示：`#pragma message`，以及把提示只在显式的开关下打开）。
4. 把 `bw.h` 放进两个 `.c` 各包含一次，`-Werror` 下会失败几次？为什么？
5. 如果目标编译器不是 GCC 而是 MSVC，`#pragma GCC warning` 会被当成什么？这一问只需要说明「非标准 pragma 的可移植边界在哪」，不必实测。

验收：`-std=c17 -pedantic` 与 `-std=c2x` 下提示文字都是 `old config path`，且 `-pedantic` 不产生扩展警告；`-Werror` 下退出码 1；`-Wno-cpp` 与 `-Wno-error=cpp` 都**不能**让第四条、第五条通过，只有 `-Wno-error`（不带 `=cpp`）或改用 `#pragma message` 才能让构建继续，这一点要写进报告。

</Exercise>

<Exercise id="11717" :d="5" :w="3">

`#error` 最适合放在「用错就一定编不出来」的地方，`#warning` 最适合放在「能编但最好别这样用」的地方。下面把两者放进同一个配置头，观察它们的相互作用，以及 `#error` 之后剩余的检查会怎样。

```c [gate.h]
#ifndef GATE_H
#define GATE_H

#if !defined(__STDC_VERSION__) || __STDC_VERSION__ < 201112L
#error "gate.h needs C11 or later"
#endif

#ifndef BUILD_CHANNEL
#error "BUILD_CHANNEL must be defined"
#elif BUILD_CHANNEL != 1 && BUILD_CHANNEL != 2
#error "BUILD_CHANNEL must be 1 (dev) or 2 (release)"
#endif

#if BUILD_CHANNEL == 1 && !defined(ALLOW_DEBUG_BUILD)
#warning "development build: do not ship this binary"
#endif

#endif
```

1. 写 `app.c` 只包含 `gate.h`。用下面这些命令各编一次，记下退出码、是否生成 `.o`、诊断条数：

   ```text
   gcc -std=c17 -DBUILD_CHANNEL=2 -Wall -Wextra -Werror -c app.c
   gcc -std=c17 -DBUILD_CHANNEL=1 -Wall -Wextra -c app.c
   gcc -std=c17 -DBUILD_CHANNEL=1 -DALLOW_DEBUG_BUILD -Wall -Wextra -c app.c
   gcc -std=c17 -Wall -Wextra -c app.c
   gcc -std=c17 -DBUILD_CHANNEL=3 -Wall -Wextra -c app.c
   gcc -std=c89 -DBUILD_CHANNEL=2 -Wall -Wextra -c app.c
   ```

2. 第四条（不定义 `BUILD_CHANNEL`）只报「BUILD_CHANNEL must be defined」一条。为什么 `#elif BUILD_CHANNEL != 1 && ...` 里的比较没有一起报错？用一个独立的小文件验证 `#elif` 的求值时机：

   ```c
   #if 1
   #error taken
   #elif 1 / 0
   #error never reached
   #endif
   ```

   再把第一行改成 `#if 0`，看第二条错误是否出现。据此说明 `#elif` 在什么情况下才会被求值，以及为什么 `#ifndef ... #error ... #elif ...` 这个顺序能避免连锁诊断。
3. 第五条（`BUILD_CHANNEL=3`）在 `-std=c17` 下只报 1 条错误。第一条版本检查为什么不报？把 `-std=c89 -DBUILD_CHANNEL=3` 合起来编译，一共几条错误、分别来自哪一条指令？为什么两条都出现，而不是被前一条挡住？
4. 把第六条的 `BUILD_CHANNEL` 改成 1（即 `-std=c89 -DBUILD_CHANNEL=1 -Wall -Wextra`），诊断里除了版本错误，还有没有那条「development build」提示？如果有，说明 `#error` 之后编译器是继续扫描还是立即停止？再补一个实验：在 `gate.h` 后面加一条 `#error "second"`，看两条错误是否都出现。
5. 把 `gate.h` 的结构改成「先做版本检查，通过后再检查配置」的两层嵌套：

   ```c
   #if !defined(__STDC_VERSION__) || __STDC_VERSION__ < 201112L
   #error "gate.h needs C11 or later"
   #else
   /* 其余检查放在这里 */
   #endif
   ```

   用 `-std=c89 -DBUILD_CHANNEL=3 -pedantic-errors` 分别编译改前与改后的版本，数一数错误条数。为什么嵌套之后能减少误导性的连带诊断？这正是正文没说、但在真实的配置头里每天都要处理的事。

6. 把 `gate.h` 改成在 `#error` 的消息里带上实际值（提示：`#error` 的记号不会被宏替换，要用 `_Pragma` 或 `_Static_assert` 才能打印值，见 11716 与 11714 第 2 问）。给出你选择的方案和理由。

验收：前六条命令的退出码依次为 0、0、0、1、1、1；第四条与第五条各恰好 1 条 `#error`；独立小文件里 `#if 1` 版本只报 1 条、`#if 0` 版本报 2 条（`division by zero` 与 `never reached`）；`-std=c89 -DBUILD_CHANNEL=1` 下有 1 条错误加 1 条警告；`-std=c89 -DBUILD_CHANNEL=3` 有 2 条错误；改前 `-std=c89 -DBUILD_CHANNEL=3 -pedantic-errors` 有 2 条错误，改后只剩 1 条。

</Exercise>
