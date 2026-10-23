# A.2 字符与文本：ANSI、Unicode 与 UTF-16

当我们把源文件以现代通行的 UTF-8 编码保存，并在代码中写下一行看似寻常的 `CreateFileA("数据.txt", ...)` 时，编译器不会给出任何警告，程序执行顺利，进程退出码也规整地返回 0。然而一旦打开资源管理器或在终端列出当前目录，磁盘上新建的文件名却匪夷所思地显示为「鏁版嵁.txt」。源码中的字符串字面量明明是清清楚楚的「数据」，调用过程中也没有触发任何 I/O 错误，为什么最终落在文件系统元数据里的却是截然不同的另外三个汉字？

底层的文件系统本身只是忠实地记录它所接收到的字符序列，症结实际上发生在 `CreateFileA` 接收到这组 UTF-8 字节、并将其传递给内核之前的转换环节。理解这一诡异现象的本质，需要探寻 Windows 操作系统的文本处理机制：从内核原生的 UTF-16 编码模型、A 与 W 双轨 API 的历史成因与职责分野，到代码页映射在跨编码流转中的作用与副作用。我们将进一步掌握在 UTF-8 与 UTF-16 之间进行无损往返转换的工程实践，探查命令行参数在进程派发过程中的字节丢失陷阱，理清控制台环境下的多重编码通道，并确立在现代系统级 C 语言项目中稳妥可行的文本架构方案。

## 1. 两套函数：A 与 W

Windows 的文本 API 大多成对出现。接收字符串的函数通常有两个版本，名字分别以 `A` 和 `W` 结尾：

```c
HANDLE CreateFileA(LPCSTR lpFileName, ...);   /* 窄字符，按代码页解释 */
HANDLE CreateFileW(LPCWSTR lpFileName, ...);  /* 宽字符，UTF-16 */
```

其中，后缀 `W` 源自 wide（宽字符），其形参为 `const wchar_t *`，要求传入以 UTF-16 编码的宽字符串；后缀 `A` 则源自 ANSI，形参为 `const char *`，要求字符串按当前宿主进程所绑定的 ANSI 代码页进行编码。值得注意的是，Windows 内核本身是完全基于 UTF-16 构建的，因此所有 `A` 版本的 API 均非内核原生入口，它们在内部的第一件事就是依据当前的 ANSI 代码页将窄字节序列转换为 UTF-16，随后再去调用对应的 `W` 版本系统函数。这就意味着，调用 `CreateFileA` 不仅比直接调用 `CreateFileW` 多付出了至少一次堆栈分配与字符转码的运行时开销，而且受限于本地代码页的字符集边界，根本无法表达当前代码页涵盖范围之外的多语言字符。

不写后缀的名字是一个宏，由 `UNICODE` 决定指向哪个版本：

```c
#ifdef UNICODE
#define CreateFile  CreateFileW
#else
#define CreateFile  CreateFileA
#endif
```

`UNICODE` 由使用者自己定义，编译器不会自动加。用 MinGW 的 gcc 编译一个只包含 `<windows.h>` 的程序，不额外加 `-D`，得到的是 A 版本：

::: terminal title="PowerShell - macrochk"

<span class="prompt">PS&gt;</span> gcc -std=c17 -Wall -Wextra macrochk.c -o macrochk_a.exe

<span class="prompt">PS&gt;</span> ./macrochk_a.exe

```text
UNICODE undefined: CreateFile is CreateFileA
sizeof(TCHAR) = 1
```

<span class="prompt">PS&gt;</span> gcc -std=c17 -Wall -Wextra -DUNICODE -D_UNICODE macrochk.c -o macrochk_w.exe

<span class="prompt">PS&gt;</span> ./macrochk_w.exe

```text
UNICODE defined: CreateFile is CreateFileW
_UNICODE defined
sizeof(TCHAR) = 2
```

:::

这一套由 `TCHAR`、`_T("...")`、`_tmain` 以及 `LPCTSTR` 构成的宏和类型系统，是微软为了让同一份 C 代码能够在 16 位/32 位过渡期既能编译为 ANSI 版本、又能编译为 Unicode 版本而设计的一套折衷抽象：当预处理宏 `UNICODE` 处于定义状态时，`TCHAR` 展开为 `wchar_t`，否则退化为普通 `char`。在 Win32 开发的早期阶段，这类条件编译曾是跨 Windows 9x 与 Windows NT 两大产品线的业界标准范式，在翻阅现存的大量传统工程或驱动源码时仍屡见不鲜，开发者必须具备辨析其语义的能力。但在现代系统编程的实践中，这种二元宏机制早已不再推荐使用：在全系列 NT 架构内核均以 Unicode 运行的今天，新代码应当彻底摒弃模糊的 `TCHAR`，直接且明确地使用 `W` 后缀 API、原生 `wchar_t` 类型以及 `L"..."` 宽字符字面量，消除编译期不确定性。

::: concept win-text-wide-is-native title="Windows 内部以 UTF-16 表示文本"

Windows 内核、注册表、NTFS 文件名、窗口消息里的字符串都是 UTF-16。`wchar_t` 在 Windows 上占 2 字节，一个 `wchar_t` 是一个 UTF-16 代码单元。A 版本函数把窄字符串转成 UTF-16 时使用的是进程的 ANSI 代码页（`GetACP()` 的返回值），与 UTF-8 和用户界面的语言都无关。

:::

::: generate win-text-a-w-walkthrough kind=explanation concepts=win-text-wide-is-native

面向第一次接触 Win32 字符串的读者，用一个具体调用把 A 版本的内部过程走一遍：`CreateFileA("数据.txt")` 的六个字节进入函数后，先按 ANSI 代码页 936 每两字节查一次 GBK 表，得到三个 UTF-16 代码单元，再把这串 UTF-16 交给 `CreateFileW`。说明「转换发生在进入内核之前」这一点，以及为什么同一个可执行文件换一台代码页不同的机器就会得到不同的文件名。

:::

## 2. 代码页

代码页（code page）是一张「字节 ↔ 字符」的对照表。Windows 上同时存在好几张，用途不同：

| 代码页 | 取法 | 本机取值 | 用在哪 |
| --- | --- | --- | --- |
| ANSI 代码页 | `GetACP()` | 936（GBK） | A 版本函数、CRT 的窄字符与宽字符转换、`CP_ACP` |
| OEM 代码页 | `GetOEMCP()` | 936 | 命令行窗口的传统编码，控制台默认使用 |
| 控制台输入代码页 | `GetConsoleCP()` | 936 | 控制台读入的字节 |
| 控制台输出代码页 | `GetConsoleOutputCP()` | 936 | 控制台解释输出的字节 |
| UTF-8 | `CP_UTF8`（65001） | 常量 | 显式指定时使用 |

ANSI 代码页由系统的「非 Unicode 程序的语言」决定，简体中文系统上是 936，也就是 GBK。它随机器而变：同一份程序拿到英文系统上是 1252，拿到日文系统上是 932。代码页描述的是字节到字符的映射，同一串字节在不同代码页下解出完全不同的字符——「乱码」就是拿错了表。

`ansi_vs_wide.c` 把这件事演示出来。同一段文本「数据.txt」，一份以 UTF-8 字节的形式交给 `CreateFileA`，一份以 `L"数据.txt"` 交给 `CreateFileW`，然后用 `FindFirstFileW` 列出目录里实际出现的文件名和码位：

```c
/* ansi_vs_wide.c 的关键几行；完整程序在 code/系统编程/拓展A_Win32系统编程/A_2_字符与文本_ANSI_Unicode与UTF16/ 下 */
const char *narrow_name = "数据.txt";      /* 源文件是 UTF-8，内存里就是 UTF-8 字节 */
const wchar_t *wide_name = L"数据.txt";

CreateFileA(narrow_name, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
CreateFileW(wide_name, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
```

::: terminal title="PowerShell - ansi_vs_wide"

<span class="prompt">PS&gt;</span> gcc -std=c17 -Wall -Wextra ansi_vs_wide.c -o ansi_vs_wide.exe

<span class="prompt">PS&gt;</span> ./ansi_vs_wide.exe

```text
GetACP() = 936, GetOEMCP() = 936
窄字符串的字节： E6 95 B0 E6 8D AE 2E 74 78 74
目录里的 .txt 文件：
  数据.txt U+6570 U+636E U+002E U+0074 U+0078 U+0074
  鏁版嵁.txt U+93C1 U+7248 U+5D41 U+002E U+0074 U+0078 U+0074
```

:::

「数据」两个汉字在 UTF-8 下是六个字节：`E6 95 B0` 是「数」，`E6 8D AE` 是「据」。`CreateFileA` 拿到这六个字节后，按代码页 936 每两个字节查一次表：`E6 95` 在 GBK 里对应 U+93C1「鏁」，`B0 E6` 对应 U+7248「版」，`8D AE` 对应 U+5D41「嵁」。六个字节凑成三个 GBK 字符，文件就以这三个字符的名字建了出来。`.txt` 四个字节是 ASCII，两种编码下相同，所以后缀没有变。

把同一条规则用一遍就能验证。`mojibake.c` 把 UTF-8 字节按 CP936 解码，再用 `WriteConsoleW` 把得到的 UTF-16 原样显示，同时打印码位：

::: terminal title="PowerShell - mojibake"

<span class="prompt">PS&gt;</span> gcc -std=c17 -Wall -Wextra mojibake.c -o mojibake.exe

<span class="prompt">PS&gt;</span> ./mojibake.exe

```text
原文 [数据.txt] -> CP936: [鏁版嵁.txt]  码位： U+93C1 U+7248 U+5D41 U+002E U+0074 U+0078 U+0074
原文 [数据] -> CP936: [鏁版嵁]  码位： U+93C1 U+7248 U+5D41
原文 [输出] -> CP936: [杈撳嚭]  码位： U+6748 U+64B3 U+56AD
```

:::

这里用 `WriteConsoleW` 是为了少一层依赖：它把 UTF-16 直接交给控制台，`printf("%ls")` 的输出还要经过区域设置的转换。控制台的规则留到第 6 节。

同一个汉字在不同代码页下的字节数并不固定：「数」在 GBK 里占 2 字节，在 UTF-8 里占 3 字节，在 UTF-16 里占 2 字节。写程序时凡是出现「按字符数算缓冲区」的地方，都要先分清说的是哪种单位。

<Quiz
  :options="[
    { label: 'Windows 不支持中文文件名', value: 'A' },
    { label: 'A 函数按 ANSI 代码页 936 解释了这六个 UTF-8 字节', value: 'B' },
    { label: '源文件被编辑器保存成了 GBK', value: 'C' },
    { label: 'NTFS 用 UTF-8 保存文件名', value: 'D' }
  ]"
  answer="B"
>

<template #question>

UTF-8 源文件里的 `CreateFileA("数据.txt", ...)` 在简体中文系统上生成了「鏁版嵁.txt」，原因是哪一个？

</template>

正确答案是 **B**。六个 UTF-8 字节被当成三个 GBK 双字节字符来解释，每个字符的码位都由那两字节查表得到，与文件名规则无关。C 错在源文件确实是 UTF-8：程序打印出的字节是 `E6 95 B0 E6 8D AE`。D 错在 NTFS 文件名以 UTF-16 保存，与这里无关。

</Quiz>

## 3. UTF-16 与 wchar_t

Windows 的 `wchar_t` 是 16 位。C 标准把 `wchar_t` 的宽度留给实现决定，只要求它能表示实现支持的扩展字符集里的每个字符，所以同一个 `wchar_t` 在 Linux 上通常是 32 位（把一段小程序在 Linux 上编译运行，`sizeof(wchar_t)` 得到 4），在 Windows 上是 2 字节。跨平台代码把 `wchar_t` 当作「一定能装下一个 Unicode 码位」来用，在 Windows 上就会出错。

宽字符字面量写成 `L"数据"`，类型是 `const wchar_t *`。GCC 按执行字符集把它编译成一串 UTF-16 代码单元，源文件是 UTF-8 也不影响：编译器先按 UTF-8 读入，再转成 UTF-16。可以用 `\u` 和 `\U` 直接写码位，`L"\U0001F600"` 就是一个表情符号。C11 起还有 `u"..."` 和 `char16_t`，它们在 Windows 上与 `wchar_t` 同宽，但 Win32 的声明一律用 `wchar_t`，`char16_t` 传不进 `CreateFileW`。

UTF-16 用 16 位表示基本多文种平面（BMP，U+0000 到 U+FFFF）内的字符，一个码位一个代码单元。BMP 以外的字符——表情符号、部分生僻汉字、数学符号——码位超过 U+FFFF，要用两个代码单元表示，称为代理对（surrogate pair）：第一个单元落在 U+D800～U+DBFF（高代理），第二个落在 U+DC00～U+DFFF（低代理）。

`wide_bytes.c` 把同一段文字在两种编码下的长度和码位并排打印：

```c
/* wide_bytes.c：逐条打印 UTF-8 字节与 UTF-16 代码单元 */
const char *samples[] = {"abc", "数据", "中文 UTF-8", "\U0001F600"};
```

::: terminal title="PowerShell - wide_bytes"

<span class="prompt">PS&gt;</span> gcc -std=c17 -Wall -Wextra wide_bytes.c -o wide_bytes.exe

<span class="prompt">PS&gt;</span> ./wide_bytes.exe

```text
[abc]
  UTF-8   3 字节： 61 62 63
  UTF-16  3 单元： U+0061 U+0062 U+0063
[数据]
  UTF-8   6 字节： E6 95 B0 E6 8D AE
  UTF-16  2 单元： U+6570 U+636E
[中文 UTF-8]
  UTF-8  12 字节： E4 B8 AD E6 96 87 20 55 54 46 2D 38
  UTF-16  8 单元： U+4E2D U+6587 U+0020 U+0055 U+0054 U+0046 U+002D U+0038
[😀]
  UTF-8   4 字节： F0 9F 98 80
  UTF-16  2 单元： U+D83D U+DE00
sizeof(wchar_t) = 2
```

:::

`\U0001F600` 的码位是 U+1F600，超出了 U+FFFF，UTF-16 用 `D83D DE00` 两个单元表示，所以 `wcslen` 对它返回 2。`wcslen` 数代码单元，`strlen` 数字节；要得到字符数，得自己按代理对成对扫描，或者改用 UTF-32。日常代码里更常见的做法是干脆不关心「多少个字符」，只把字符串当不透明的一串单元传下去。

<Quiz
  :options="[
    { label: '1', value: 'A' },
    { label: '2', value: 'B' },
    { label: '4', value: 'C' },
    { label: '编译错误', value: 'D' }
  ]"
  answer="B"
>

<template #question>

Windows 上 `wcslen(L"\U0001F600")` 的值是多少？

</template>

正确答案是 **B**。U+1F600 在 UTF-16 里是一个代理对，占两个代码单元；`wcslen` 数的是代码单元，返回 2。选 1 是按码位数算的，选 4 是按 UTF-8 字节数算的。

</Quiz>

## 4. UTF-8 与 UTF-16 互相转换

程序内部用 UTF-8、调用 Win32 时转成 UTF-16，是这一页后面所有策略的共同基础。两个转换函数都在 `<windows.h>` 里，不需要额外链接：

```c
int MultiByteToWideChar(UINT CodePage, DWORD dwFlags, LPCCH lpMultiByteStr, int cbMultiByte,
                        LPWSTR lpWideCharStr, int cchWideChar);
int WideCharToMultiByte(UINT CodePage, DWORD dwFlags, LPCWCH lpWideCharStr, int cchWideChar,
                        LPSTR lpMultiByteStr, int cbMultiByte, LPCCH lpDefaultChar,
                        LPBOOL lpUsedDefaultChar);
```

两个函数都用「调用两次」的用法。第一次把输出缓冲区传 `NULL`、输出容量传 0，函数返回所需的容量；按这个容量分配缓冲区，第二次真正转换。`cbMultiByte` 或 `cchWideChar` 传 -1 表示输入以空字符结尾，此时返回的长度**包含**结尾的空字符，转换结果也是以空字符结尾的。传正数表示只处理这么多字节或单元，结果不带结尾空字符，长度也不含它。

第一个参数 `CodePage` 要显式写 `CP_UTF8`，不要写 `CP_ACP`：后者的含义随机器而变，同一份程序在别的机器上会得到不同结果。用 `CP_UTF8` 时，`dwFlags` 只能是 0 或 `MB_ERR_INVALID_CHARS`；`WideCharToMultiByte` 方向的 `WC_ERR_INVALID_CHARS` 也只对 UTF-8 与 GB18030 有效，用在 936 上会直接失败，错误码 1004（`ERROR_INVALID_FLAGS`）。

`textconv.c` 把这段逻辑封装成两个函数，`textconv.h` 里只有两个声明：

```c
/* textconv.c */
wchar_t *utf8_to_wide(const char *utf8) {
    int need = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, utf8, -1, NULL, 0);
    if (need == 0) {
        return NULL;
    }
    wchar_t *wide = malloc((size_t)need * sizeof *wide);
    if (wide == NULL) {
        return NULL;
    }
    if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, utf8, -1, wide, need) == 0) {
        free(wide);
        return NULL;
    }
    return wide;
}
```

`wide_to_utf8` 的形状完全对称，只是把方向反过来、把 `MB_ERR_INVALID_CHARS` 换成 `WC_ERR_INVALID_CHARS`。两个函数都返回 `malloc` 出来的缓冲区，调用者负责 `free`；失败时返回 `NULL`，原因留在 `GetLastError()` 里。

`conv.c` 拿四种文本走一遍往返：

::: terminal title="PowerShell - conv"

<span class="prompt">PS&gt;</span> gcc -std=c17 -Wall -Wextra conv.c textconv.c -o conv.exe

<span class="prompt">PS&gt;</span> ./conv.exe

```text
[abc]
  UTF-16 3 单元： U+0061 U+0062 U+0063
  往返字节： 61 62 63
  结果：与原文相同
[数据.txt]
  UTF-16 6 单元： U+6570 U+636E U+002E U+0074 U+0078 U+0074
  往返字节： E6 95 B0 E6 8D AE 2E 74 78 74
  结果：与原文相同
[中文 UTF-8 输出]
  UTF-16 11 单元： U+4E2D U+6587 U+0020 U+0055 U+0054 U+0046 U+002D U+0038 U+0020 U+8F93 U+51FA
  往返字节： E4 B8 AD E6 96 87 20 55 54 46 2D 38 20 E8 BE 93 E5 87 BA
  结果：与原文相同
[😀 表情]
  UTF-16 5 单元： U+D83D U+DE00 U+0020 U+8868 U+60C5
  往返字节： F0 9F 98 80 20 E8 A1 A8 E6 83 85
  结果：与原文相同
```

:::

往返之后字节完全一致，包括代理对。转换是可逆的，前提是两端都按规定的编码解释。

`MB_ERR_INVALID_CHARS` 的意义是「遇到非法序列就失败」。不设它时，非法字节会被换成 U+FFFD（替换字符），函数照样返回成功，坏数据静悄悄变成一串问号。`badconv.c` 把三件事放在一起：非法 UTF-8、孤立代理项、以及两种 flags 取值的对比：

```c
const char *bad[] = {"\x80", "\xE6\x95"};   /* 孤立续字节；截断的三字节序列 */
const wchar_t lone[] = {0xD83D, 0x0021, 0};  /* 孤立的代理项 */
```

::: terminal title="PowerShell - badconv"

<span class="prompt">PS&gt;</span> gcc -std=c17 -Wall -Wextra badconv.c textconv.c -o badconv.exe

<span class="prompt">PS&gt;</span> ./badconv.exe

```text
非法 UTF-8 第 1 例       返回 NULL，GetLastError() = 1113
非法 UTF-8 第 2 例       返回 NULL，GetLastError() = 1113
孤立代理项 D83D 0021    返回 NULL，GetLastError() = 1113
CP936 + MB_ERR_INVALID_CHARS   返回 2，GetLastError() = 0
CP_ACP + WC_ERR_INVALID_CHARS  返回 0，GetLastError() = 1004
CP_UTF8 + flags=0              返回 2，第一个单元 U+FFFD
```

:::

1113 是 `ERROR_NO_UNICODE_TRANSLATION`，系统给出的说明是「在多字节的目标代码页中，没有此 Unicode 字符可以映射到的字符。」同样的字节 `E6 95` 交给 CP936 却不报错，因为它在 GBK 里是一个合法的双字节字符——「非法」始终是相对于某个编码而言的。

最后一行说明不设 flag 时会发生什么：截断的两字节序列被替换成 U+FFFD，长度照样返回 2（替换字符加结尾空字符），调用者从返回值上看不出输入有问题。凡是接收外部输入的地方都该带上 `MB_ERR_INVALID_CHARS`，这样坏数据会变成一次明确的失败。

::: tip 转换函数放头文件，不要每次重写

`utf8_to_wide` 与 `wide_to_utf8` 在本页出现之后，还会在需要同时处理 UTF-8 与 Win32 字符串的地方用到。把 `textconv.c` 编成一个目标文件或静态库，或者在项目里直接一起编译，比每个示例各抄一份可靠：两次调用的长度语义、`-1` 的含义、错误码的检查都容易抄漏。

:::

## 5. 命令行参数

C 程序从 `main` 拿到的 `argv` 是 `char *` 数组，Windows 上这串字节是按 ANSI 代码页从 UTF-16 命令行转换来的。C 运行时先取到进程的 UTF-16 命令行，按代码页 936 转成窄字符串，再按空白和引号切分。代码页表里没有的字符只能变成 `?`，信息在进入 `main` 之前就丢了。

要拿到原始参数，把入口改成 `wmain`：

```c
int wmain(int argc, wchar_t **argv);
```

MinGW 链接时要加 `-municode`，它把启动代码从 `mainCRTStartup` 换成 `wmainCRTStartup`；不加时链接器按普通控制台程序的入口去找 `main`，找不到就报 `undefined reference to 'WinMain'`，这个报错与 `wmain` 本身毫无关系，第一次遇到时很容易看错方向：

```text
ld.exe: ...libmingw32.a(lib64_libmingw32_a-crtexewin.o):crtexewin.c:(.text+0x130): undefined reference to `WinMain'
```

`launch_args.c` 用 `CreateProcessW` 以同一条命令行启动两个程序，一个是 `main` 版、一个是 `wmain` 版，排除掉 shell 自己改写命令行的可能：

```c
wchar_t cmd_main[] = L"args_main.exe 中文 \"a b\" \U0001F600";
wchar_t cmd_wide[] = L"wargs.exe 中文 \"a b\" \U0001F600";
```

::: terminal title="PowerShell - launch_args"

<span class="prompt">PS&gt;</span> gcc -std=c17 -Wall -Wextra args_main.c -o args_main.exe

<span class="prompt">PS&gt;</span> gcc -std=c17 -Wall -Wextra -municode wargs.c -o wargs.exe

<span class="prompt">PS&gt;</span> gcc -std=c17 -Wall -Wextra launch_args.c -o launch_args.exe

<span class="prompt">PS&gt;</span> ./launch_args.exe

```text
=== main 版
命令行：args_main.exe 中文 "a b" 😀
argc = 3
argv[0]: 61 72 67 73 5F 6D 61 69 6E 2E 65 78 65 (13 字节)
argv[1]: D6 D0 CE C4 (4 字节)
argv[2]: 61 20 62 (3 字节)
----
=== wmain 版
命令行：wargs.exe 中文 "a b" 😀
argc = 4
argv[0] = [wargs.exe] 9 个代码单元： U+0077 U+0061 U+0072 U+0067 U+0073 U+002E U+0065 U+0078 U+0065
argv[1] = [中文] 2 个代码单元： U+4E2D U+6587
argv[2] = [a b] 3 个代码单元： U+0061 U+0020 U+0062
argv[3] = [😀] 2 个代码单元： U+D83D U+DE00
----
```

:::

同一条命令行，两个程序看到的世界不一样。`main` 版把「中文」变成了 `D6 D0 CE C4`（GBK 的四字节），把「😀」整个丢掉了：`argc` 只有 3，最后那个参数根本没有出现。`wmain` 版拿到 `U+4E2D U+6587`，表情符号是 `U+D83D U+DE00` 两个单元，一个不少。

表情符号在代码页 936 里没有对应字符，转换时变成两个问号 `??`；接下来 C 运行时对 `argv` 里的每个参数做通配符展开（这是 Windows 上 CRT 的传统行为，为了在命令行里模拟 Unix shell 的 `*.c` 展开），`??` 正好是一个「匹配任意两个字符」的模式。目录里没有两字符的文件名时，这个参数就被当作「没有匹配」而整个丢弃。在一个只放了 `args_main.exe` 的目录里运行，能稳定复现：

::: terminal title="PowerShell - args_main（空目录）"

<span class="prompt">PS&gt;</span> .\args_main.exe "😀"

```text
argc = 1
argv[0]: 43 3A 5C 77 6F 72 6B 5C 75 74 5C 61 72 67 73 5F 6D 61 69 6E 2E 65 78 65 (24 字节)
```

<span class="prompt">PS&gt;</span> .\args_main.exe "??"

```text
argc = 1
argv[0]: 43 3A 5C 77 6F 72 6B 5C 75 74 5C 61 72 67 73 5F 6D 61 69 6E 2E 65 78 65 (24 字节)
```

<span class="prompt">PS&gt;</span> New-Item ab, cd

<span class="prompt">PS&gt;</span> .\args_main.exe "😀"

```text
argc = 3
argv[0]: 43 3A 5C 77 6F 72 6B 5C 75 74 5C 61 72 67 73 5F 6D 61 69 6E 2E 65 78 65 (24 字节)
argv[1]: 61 62 (2 字节)
argv[2]: 63 64 (2 字节)
```

<span class="prompt">PS&gt;</span> .\args_main.exe "x😀"

```text
argc = 2
argv[0]: 43 3A 5C 77 6F 72 6B 5C 75 74 5C 61 72 67 73 5F 6D 61 69 6E 2E 65 78 65 (24 字节)
argv[1]: 78 3F 3F (3 字节)
```

:::

手动输入的 `"??"` 与表情符号转出来的 `??` 得到同样的结果，可见丢参数发生在转换之后、通配符展开这一步。放两个两字符的文件名（`ab`、`cd`）再运行同一句，`argc` 变成 3，表情符号的位置换成了那两个文件名，`argv[1]` 是 `61 62`。更隐蔽的是 `x😀`：它变成 `x??`，模式里有字面字符 `x`，不匹配任何文件时参数保留，只是内容成了 `78 3F 3F`。同一个字符在不同上下文里，有时变成问号、有时整个消失。这是 Windows 上 `main` 特有的一个坑，与编码无关，但只有在编码转换失败时才会遇到，所以放在这里讲。

不使用 CRT 的 `argv` 也能拿到完整参数：`GetCommandLineW()` 返回整条 UTF-16 命令行，`CommandLineToArgvW()` 按微软文档里的规则把它拆开，用 `LocalFree` 释放。这条路对 `main` 和 `wmain` 都可用，`shell32` 由 MinGW 默认链接。`wmain` 更省事，本拓展后面的页面都用它。

::: compare left-title="用 main 接收参数" right-title="用 wmain 接收参数"

```c
/* 代码页外的字符变成 ?，甚至整个参数丢失 */
int main(int argc, char **argv) {
    for (int i = 0; i < argc; i++) {
        printf("%s\n", argv[i]);   /* 中文是 GBK 字节，😀 不见了 */
    }
    return 0;
}
```

---

```c
/* 完整的 UTF-16，交给转换函数即可 */
int wmain(int argc, wchar_t **argv) {
    for (int i = 0; i < argc; i++) {
        char *utf8 = wide_to_utf8(argv[i]);
        printf("%s\n", utf8);
        free(utf8);
    }
    return 0;
}
```

:::

编译 `wmain` 版要记得 `-municode`。这个开关是 MinGW 的，MSVC 直接支持 `wmain`，不需要额外选项。

## 6. 控制台输出

控制台是一个按「当前输出代码页」解释字节的窗口。程序交给它的是字节，它按代码页查表把它们变成屏幕上的字符。代码页是 936 时，把 UTF-8 编码的「数据」六个字节直接写进去，屏幕上出现的是「鏁版嵁」。

不过 CRT 还有一条经过转换的路。`printf`、`fwrite` 这类函数写到控制台时，UCRT 先按区域设置解释这段窄字符串，再把它转成「控制台输出代码页」的字节写出去。区域设置是 `setlocale(LC_ALL, ".UTF8")` 时，内存里的 UTF-8 字节被正确理解，转到代码页 936 之后中文仍然正确，但代码页 936 表示不了的表情符号会变成 `?`；区域设置仍是默认的 `C` 时，UCRT 没有这张转换表，字节原样传给控制台，由控制台按代码页解释，于是「数据」显示成「鏁版嵁」。直接对控制台句柄调用 `WriteFile`，永远走后面这条原样传递的路。

`console_cp.c` 把三条路径放在一次运行里（程序开头调用了 `setlocale(LC_ALL, ".UTF8")`）：

```c
WriteFile(h, line, ...);            /* UTF-8 字节直接进控制台句柄，按输出代码页解释 */
printf("printf: %s", line);         /* 走 UCRT：按区域设置解释后转成输出代码页的字节 */
WriteConsoleW(h, wide, ...);        /* 直接写 UTF-16，与代码页无关 */
```

::: terminal title="PowerShell - console_cp"

<span class="prompt">PS&gt;</span> gcc -std=c17 -Wall -Wextra console_cp.c -o console_cp.exe

<span class="prompt">PS&gt;</span> ./console_cp.exe

```text
GetConsoleOutputCP() = 936
鏁版嵁 UTF-8 杈撳嚭
printf: 数据 UTF-8 输出
GetConsoleOutputCP() = 65001
数据 UTF-8 输出
WriteConsoleW: 数据 UTF-8 输出
```

:::

第一行是 `WriteFile` 直接把 UTF-8 字节送进控制台句柄的结果：代码页还是 936，六个字节被当成三个 GBK 字符。第三行是 `printf` 同一段文字的结果，却是对的——UCRT 按区域设置读懂这段 UTF-8，再转成代码页 936 的字节交给控制台，中文原样保留。第五行是 `SetConsoleOutputCP(CP_UTF8)` 之后再 `WriteFile`，代码页改了，字节的解释跟着改，结果正确。最后一行是 `WriteConsoleW` 直接写 UTF-16，代码页被改回 936 也不受影响。

`printf` 这条路径的优势在重定向时也成立：标准输出接的是文件或管道时，UCRT 不做代码页转换，写出的就是内存里的 UTF-8 字节。程序不必为此写两套代码，只要保证内部编码统一，两种去向都得到同一串字节。代价是这条路要依赖区域设置，且它只能表达「目标代码页里有的字符」——代码页 936 下表情符号会变成 `?`，要原样输出非 BMP 字符，只有 `WriteConsoleW`（或先把代码页设为 65001）能做到。

`WriteConsoleW` 只对控制台句柄有效。标准输出被重定向到文件或管道时，拿到的是文件句柄或管道句柄，调用失败、错误码 6（`ERROR_INVALID_HANDLE`）。所以这个函数不能当成通用的输出手段，重定向时必须用别的路径。

`printf` 的宽字符格式 `%ls` 另有要求。它需要区域设置告诉 CRT「窄字符用哪种编码」，默认的 C 区域设置里没有这个信息，`printf("%ls")` 直接返回 -1，对应的内容一个字节也不输出：

::: terminal title="PowerShell - wprint"

<span class="prompt">PS&gt;</span> gcc -std=c17 -Wall -Wextra wprint.c -o wprint.exe

<span class="prompt">PS&gt;</span> ./wprint.exe

```text
当前区域设置：C
printf 返回 -1
setlocale 返回：Chinese (Simplified)_China.utf8
数据 abc
printf 返回 11
```

:::

`setlocale(LC_ALL, ".UTF8")` 是本机 UCRT 的写法，返回的区域设置名是 `Chinese (Simplified)_China.utf8`，表示「简体中文区域，窄字符按 UTF-8」。之后再调用，`%ls` 正常输出，返回值 11 是实际写出的字节数（`数据` 6 字节、空格 1 字节、`abc` 3 字节、换行 1 字节），它既不等于 UTF-16 的代码单元数，也不等于字符数。区域设置本身的含义见第一部分 [21. 本地化与扩展文本](/教程/正文/语法和标准库/21_本地化与扩展文本)，那里讲的 `setlocale` 管的是数字格式、排序和宽窄字符转换，与这里用的是同一个函数。

把两件事合起来，本拓展此后每页的程序都在入口函数开头写这两行：

```c
setlocale(LC_ALL, ".UTF8");   /* printf 的 %ls 与窄字符串按 UTF-8 解释 */
SetConsoleOutputCP(CP_UTF8);  /* 直接写进控制台的 UTF-8 字节按 UTF-8 解释 */
```

第一行管 CRT，第二行管控制台。缺哪一行都会坏一半：只设代码页时，`%ls` 仍然是 -1；只设区域设置时，`WriteFile` 写出的 UTF-8 字节仍被当作 GBK。两行都写上，`printf`、`fwrite`、`WriteFile`、`WriteConsoleW` 四条路径才一致。

::: compare left-title="只写 UTF-8 字节，不改代码页" right-title="设置区域设置与输出代码页"

```c
#include <stdio.h>
#include <windows.h>

int main(void) {
    printf("数据 UTF-8 输出\n");
    return 0;
}
```

---

```c
#include <locale.h>
#include <stdio.h>
#include <windows.h>

int main(void) {
    setlocale(LC_ALL, ".UTF8");
    SetConsoleOutputCP(CP_UTF8);
    printf("数据 UTF-8 输出\n");
    return 0;
}
```

:::

左边在代码页 936 的控制台上显示成「鏁版嵁 UTF-8 杈撳嚭」，右边显示正常。右边那两行的顺序可以互换，它们设置的是两个独立的开关。

::: warning 重定向后输出的是 UTF-8 字节

标准输出被重定向到文件时，`printf` 写出的是 UTF-8 字节，与控制台代码页无关；用记事本或 `type` 打开显示成乱码，是因为这些工具按 GBK 读文件，程序写出的字节本身是对的。用支持 UTF-8 的编辑器打开，或者 `Get-Content -Encoding utf8`。

:::

<Quiz
  :options="[
    { label: '只调用 SetConsoleOutputCP', value: 'A' },
    { label: '先调用 setlocale(LC_ALL, &quot;.UTF8&quot;)', value: 'B' },
    { label: '改用 puts', value: 'C' },
    { label: '加 -municode 编译', value: 'D' }
  ]"
  answer="B"
>

<template #question>

在 UCRT 下，哪一步能让 `printf("%ls\n", L"中文")` 输出 UTF-8？

</template>

正确答案是 **B**。`%ls` 要把宽字符转成窄字符写出，转换规则来自区域设置；C 区域设置下没有 UTF-8 的转换表，函数直接返回 -1。`SetConsoleOutputCP` 管的是控制台怎么解释已经写出的字节，管不到 CRT 的转换这一步；`-municode` 只影响程序入口和命令行参数。

</Quiz>

## 7. 两种工程策略

梳理完底层的编码转换、参数展开与控制台输出机制后，在实际工程开发中，我们需要确立清晰且一贯的文本架构策略。系统级程序如何组织内部字符串的生命周期与编码形式，直接决定了整个软件体系与第三方生态的交互成本。在 Windows 平台下，业界通常在以下两种主流工程策略中进行权衡：

**策略一：内部一律 UTF-8，只在调用 Win32 的地方转换。** 程序里所有字符串都是 `char *` 的 UTF-8，读写文件、解析配置、网络收发都用它；需要调用 Win32 时，用 `utf8_to_wide` 转成 `wchar_t *`，调用 W 版本函数，需要返回文本时再用 `wide_to_utf8` 转回来。C 标准库、第三方库、跨平台代码都按 UTF-8 工作，转换集中在一层薄薄的边界上。

代价是每个调用点都要转换，写起来啰嗦；好处是程序内部只有一种编码，不会出现这段是 GBK、那段是 UTF-8 的混乱，换一台代码页不同的机器行为不变。

**策略二：让进程的 ANSI 代码页变成 UTF-8。** 从 Windows 10 1903 起，可执行文件的清单里可以声明 `activeCodePage` 为 `UTF-8`。声明之后，进程的 ANSI 代码页就是 65001：A 版本函数按 UTF-8 解释窄字符串，`GetACP()` 返回 65001，`CP_ACP` 也等于 UTF-8。旧代码一行不改，中文文件名就对了。

MinGW 没有 MSVC 那样把清单文件直接传给链接器的用法，要用 `windres` 把清单编成资源再链接进去。`utf8_manifest/` 下是三个文件：

```text
utf8.manifest   清单本身
utf8.rc         一行资源脚本：1 24 "utf8.manifest"
acp.c           查看 GetACP() 并用 CreateFileA 建中文名文件
```

清单是一段标准的程序集声明，其中 `<windowsSettings>` 的 `activeCodePage` 指定进程的 ANSI 代码页：

```xml
<?xml version="1.0" encoding="UTF-8" standalone="yes"?>
<assembly xmlns="urn:schemas-microsoft-com:asm.v1" manifestVersion="1.0">
  <assemblyIdentity type="win32" name="MdrTutorial.AcpUtf8" version="1.0.0.0"/>
  <application xmlns="urn:schemas-microsoft-com:asm.v3">
    <windowsSettings>
      <activeCodePage xmlns="http://schemas.microsoft.com/SMI/2019/WindowsSettings">UTF-8</activeCodePage>
    </windowsSettings>
  </application>
</assembly>
```

资源脚本里的 `24` 是 RT_MANIFEST 的资源类型编号，`1` 是资源 ID，必须是 1。构建和对比：

::: terminal title="PowerShell - windres"

<span class="prompt">PS&gt;</span> cd utf8_manifest

<span class="prompt">PS&gt;</span> windres utf8.rc -O coff -o utf8.res.o

<span class="prompt">PS&gt;</span> gcc -std=c17 -Wall -Wextra acp.c -o acp.exe

<span class="prompt">PS&gt;</span> gcc -std=c17 -Wall -Wextra acp.c utf8.res.o -o acp_utf8.exe

<span class="prompt">PS&gt;</span> ./acp.exe

```text
GetACP()             = 936
GetOEMCP()           = 936
GetConsoleOutputCP() = 65001
目录里的 .txt 文件：
  鏁版嵁.txt U+93C1 U+7248 U+5D41 U+002E U+0074 U+0078 U+0074
```

<span class="prompt">PS&gt;</span> ./acp_utf8.exe

```text
GetACP()             = 65001
GetOEMCP()           = 65001
GetConsoleOutputCP() = 65001
目录里的 .txt 文件：
  数据.txt U+6570 U+636E U+002E U+0074 U+0078 U+0074
```

:::

同一个 `acp.c`，只因为链接了一个资源对象，`CreateFileA` 生成的文件名就从「鏁版嵁.txt」变成了「数据.txt」。清单生效后 `GetOEMCP()` 也返回 65001，这是这项设置的连带效果。

清单的作用范围是这个可执行文件本身。用带清单的父进程启动不带清单的子进程，子进程的 `GetACP()` 仍然是 936（`spawn_utf8.c` 与 `acp.exe` 放在同一目录）：

::: terminal title="PowerShell - 子进程不继承"

<span class="prompt">PS&gt;</span> ./spawn_utf8.exe

```text
parent GetACP() = 65001
GetACP()             = 936
GetOEMCP()           = 936
GetConsoleOutputCP() = 65001
目录里的 .txt 文件：
  鏁版嵁.txt U+93C1 U+7248 U+5D41 U+002E U+0074 U+0078 U+0074
```

:::

清单是每个 exe 各自的设置，链接进来的第三方程序不受影响。这对调用外部工具的程序有实际影响：父进程按 UTF-8 构造参数，子进程按 936 解释，参数照样会坏。

至于选哪一种，本页的建议是：**新代码用策略一，策略二只作为补充手段。** 理由有三条。程序内部统一 UTF-8 之后，只有调用 Win32 的那一层需要关心编码，跨平台的部分（标准 C、第三方库、网络协议）全部自然对齐；策略二把正确性寄托在清单上，漏掉一个 exe、链接错一次资源，行为就退回 936，而这种错误在开发机上（可能就是 936）根本看不出来；策略二还有覆盖不到的地方，比如 GDI 的文本输出和部分系统组件，微软自己的文档就指出 GDI 不支持按进程设置代码页。

两边都不要做的事：不要依赖用户机器的 ANSI 代码页，任何转换都显式写 `CP_UTF8`，不写 `CP_ACP`；不要写 `TCHAR`、`_T()` 那一套；不要在程序里缓存 `GetACP()` 的结果当作编码依据。

<Quiz
  type="multi"
  :options="[
    { label: '程序内部用 UTF-8，调用 W 函数前转换成 UTF-16', value: 'A' },
    { label: '用 TCHAR 与 _T() 让源码同时支持两种编译', value: 'B' },
    { label: '转换时显式指定 CP_UTF8', value: 'C' },
    { label: '根据 GetACP() 的返回值决定用哪种编码', value: 'D' }
  ]"
  :answer="['A', 'C']"
>

<template #question>

新写的跨平台程序，下面哪些做法是对的？

</template>

正确答案是 **A、C**。B 是 1990 年代的过渡写法，它让同一份源码按编译选项生成两个变体，新代码不需要这种灵活性；D 把行为绑在用户机器的设置上，同一份程序换台机器结果就变，应当避免。

</Quiz>

::: generate win-text-strategies kind=comparison concepts=win-text-wide-is-native

面向正在把一份已有的 Windows 程序改成支持中文的读者，对比两种策略在三种情形下的影响：已有大量 `CreateFileA`、`printf` 调用的旧代码，链接了按 A 版本接口工作的第三方库，以及程序自己启动子进程并传递中文参数。逐个情形说明改动量、需要留意的地方，以及哪种策略更省事；不要求总结全文。

:::

## 8. 习题

题面用到的程序都在 `code/系统编程/拓展A_Win32系统编程/A_2_字符与文本_ANSI_Unicode与UTF16/` 下。

### 8.1 巩固

<Exercise id="45201" :d="1" :w="2" multi>

(1) _[1.1]_ `CreateFileW` 与 `CreateFileA` 谁的参数是 UTF-16？不写后缀的 `CreateFile` 在不定义 `UNICODE` 时指向哪一个？

(2) _[1.1]_ 本机上 `GetACP()` 与 `GetOEMCP()` 都是 936。如果同一台机器把「非 Unicode 程序的语言」改成英文（美国），两个值各会变成多少？这一步会不会影响 `CreateFileW` 的行为？

(3) _[2.2]_ UTF-8 的「数据」是 `E6 95 B0 E6 8D AE`，按 CP936 解出来是哪三个字符、各是什么码位？用 `mojibake.c` 验证你的答案。

</Exercise>

<Exercise id="45202" :d="2" :w="2" multi>

(1) _[2.2]_ 下面这段代码在 936 的机器上会输出什么？逐字节说明原因。

```c
const wchar_t *w = L"中文";
char buf[16];
WideCharToMultiByte(CP_ACP, 0, w, -1, buf, 16, NULL, NULL);
printf("%d\n", (int)strlen(buf));
```

(2) _[2.2]_ 把 `CP_ACP` 换成 `CP_UTF8`，同一个 `strlen` 的结果是多少？为什么两个数不一样？

(3) _[1.1]_ `sizeof(wchar_t)` 在 Windows 与 Linux 上分别是多少？`wcslen(L"\U0001F600")` 在两边分别是多少？

</Exercise>

### 8.2 编程

<Exercise id="45203" :d="3" :w="3">

写一个 `u8len` 程序，用法 `u8len 文本...`：对每个命令行参数，按 UTF-8 规则数出它的字节数、码位数、UTF-16 代码单元数，再打印第一个码位的十六进制值。

要求：

- 自己按 UTF-8 的字节模式解析（首字节的高位决定后面跟几个续字节），不要调用 `MultiByteToWideChar` 来数码位；可以调用它来核对结果。
- 遇到非法字节序列时报告位置并跳过该字节，继续处理后面的内容。
- 参数用 `wmain` 接收，先转成 UTF-8 再计数，或者直接在宽字符上数单元。

验收：`u8len abc 数据 😀` 的输出分别是 `3 字节 / 3 码位 / 3 单元`、`6 字节 / 2 码位 / 2 单元`、`4 字节 / 1 码位 / 2 单元`，并且自己算的数与 `MultiByteToWideChar` 得到的单元数一致。

</Exercise>

<Exercise id="45204" :d="3" :w="3">

写一个 `dump_names` 程序：遍历当前目录下所有 `.txt` 文件，对每个文件名同时打印窄字符形式（`FindFirstFileA`）与宽字符形式（`FindFirstFileW`），并标出两者不一致的条目。

要求：

- 先用 `CreateFileA` 建三个文件：两个纯 ASCII 名（`a.txt`、`b.txt`），一个中文名（`数据.txt`）。
- 再用 `FindFirstFileA` 与 `FindFirstFileW` 各列一遍，按名字排序后并排打印，逐条比较。
- 统计并打印「窄字符形式无法表示」的文件数（提示：`FindFirstFileA` 对这类名字返回什么？先实测再下结论）。
- 打印中文文件名时用本页第 6 节的两行开头，并另做一个「只写 `printf("%ls")`、不调 `setlocale`」的版本，记录两者的差别。

验收：三个文件的宽字符形式全部正确；能说清楚窄字符那一遍为什么与宽字符那一遍对不上，并用码位证实；不调 `setlocale` 的版本里中文名显示成什么，说清原因。

</Exercise>

<Exercise id="45205" :d="3" :w="3">

给 `textconv.c` 加两个函数并写测试：

```c
/* 转换 src 的前 n 个字节，不要求以空字符结尾；成功返回 malloc 的缓冲区，
   并在 *out_len 里写回结果的代码单元数（不含结尾空字符）。 */
wchar_t *utf8_to_wide_n(const char *src, int n, int *out_len);

/* 同上，反方向。 */
char *wide_to_utf8_n(const wchar_t *src, int n, int *out_len);
```

要求：

- 内部仍然用两次调用的写法，但第一次调用时长度参数传 `n` 而不是 -1。
- 缓冲区多分配一个单元并手工补结尾空字符，保证结果可以直接当 C 字符串用。
- 用 `conv.c` 里那四条文本各测一遍（传 `strlen(s)`），确认结果与带 -1 的版本逐单元相同。
- 再测一段「前半是合法 UTF-8、后半是截断序列」的输入，说明带 `MB_ERR_INVALID_CHARS` 时函数返回什么。

验收：四条文本的逐单元比较全部相同；截断输入的测试能说清楚失败发生在哪一步、`GetLastError()` 是多少。

</Exercise>

### 8.3 综合与挑战

<Exercise id="45206" :d="4" :w="3" multi>

`args_main.c` 在空目录里丢掉表情符号参数，`wargs.exe` 的 `argc` 却是对的。

(1) _[3.2]_ 用 `_CRT_glob = 0`（或 `_dowildcard = 0`）在 `args_main.c` 里关掉通配符展开，重新运行，记录 `argc` 与 `argv[3]` 的字节。两种写法各试一次，两者是不是同一个开关？用 `nm` 或 `objdump -x` 看可执行文件里的符号，说明这两行定义为什么能影响到 CRT 的启动代码。

(2) _[3.2]_ 关掉展开之后，`args_main.exe "*.txt"` 收到的是什么？与不关时相比，程序看到的 `argc` 差多少？

(3) _[4.3*]_ 设计一个判断「这个参数是用户原样输入的，还是被展开出来的」的方法，只允许用 `argv` 里的信息。提示：展开出来的参数一定对应真实存在的文件。

</Exercise>

<Exercise id="45207" :d="4" :w="4">

写一个 `show` 小工具，用法 `show <文件>`：读入一个 UTF-8 文本文件，按行打印每行的字符数，并且在文件含非法 UTF-8 时报告第一个出错的行号与字节偏移。

要求：

- 用 W 版本 API 打开文件，或者用 `fopen` 读原始字节后自己转换，两条路选一条并说明理由。
- 逐行处理时不能假设「一行就是一个 `char *` 加 `\n`」这一层编码无关；给出你对「换行」的处理方式。
- 遇到非法字节时用 `MultiByteToWideChar` 加 `MB_ERR_INVALID_CHARS` 确认，并打印 `GetLastError()`。

验收：用 `conv.c` 的源码当输入（纯 ASCII）与一段自己造的中英混排文件各测一次，字符数正确；再造一个含孤立 `0x80` 的文件，报告的行号与偏移与实际位置一致。

</Exercise>

<Exercise id="45208" :d="5" :w="4">

把 `utf8_manifest/` 的演示扩成一个对比实验：

要求：

- 写一个 `roundtrip.c`，用 `CreateFileA` 创建名为「测试-数据.txt」的文件，再用 `FindFirstFileW` 读回名字并打印码位；同一份源码编译两次，一次带清单、一次不带。
- 再写一个 `child_args.c`，父进程用 `CreateProcessW` 把「测试」作为参数传给子进程，子进程用 `main` 接收并打印字节。分别在父进程带清单与不带清单时运行，记录四种组合的结果。
- 用 `objdump -x` 或资源查看工具确认清单确实嵌在 exe 里，贴出关键几行。
- 根据结果回答：清单对子进程有没有影响？如果父进程要把中文参数传对，正确的做法是什么？

验收：四组输出全部贴出，并给出「清单只作用于本 exe」这一结论的证据。

</Exercise>

<Exercise id="45209" :d="6" :w="5" scope="*">

设计并实现一个小型文本处理工具 `u8cat`，用法 `u8cat [--to-utf8|--to-utf16] <输入> <输出>`，要求：

- 输入输出都用 `CreateFileW` 打开，内部用 UTF-8 缓冲区处理，读写用 `ReadFile`/`WriteFile`，自己处理分块读取时被截断在多字节序列中间的情况（缓冲区尾部可能只剩半个字符）。
- 转换用本页的 `textconv.c`，但分块时不能依赖以空字符结尾——需要你在第 45205 题的基础上扩展。
- `--to-utf16` 时输出 UTF-16LE 带 BOM 的文件，`--to-utf8` 时输出不带 BOM 的 UTF-8 文件；用 `Format-Hex` 或 `xxd` 验证前几个字节。
- 输入含非法序列时，报告第一处出错的偏移，并以非零退出码结束。

验收：把一个 UTF-8 的中文文件转成 UTF-16LE 再转回来，两份文件用 `fc /b` 比较无差异；把块大小设为 5 字节重跑，结果不变。这道题需要先读完 A.4 的文件读写，选做。

</Exercise>

## 9. 参考资料

- Microsoft Learn，Unicode in the Windows API：<https://learn.microsoft.com/en-us/windows/win32/intl/unicode-in-the-windows-api>
- Microsoft Learn，Conventions for Function Prototypes：<https://learn.microsoft.com/en-us/windows/win32/intl/conventions-for-function-prototypes>
- Microsoft Learn，Working with Strings：<https://learn.microsoft.com/en-us/windows/win32/learnwin32/working-with-strings>
- Microsoft Learn，Use UTF-8 code pages in Windows apps：<https://learn.microsoft.com/en-us/windows/apps/design/globalizing/use-utf8-code-page>
- Microsoft Learn，MultiByteToWideChar：<https://learn.microsoft.com/en-us/windows/win32/api/stringapiset/nf-stringapiset-multibytetowidechar>
- Microsoft Learn，WideCharToMultiByte：<https://learn.microsoft.com/en-us/windows/win32/api/stringapiset/nf-stringapiset-widechartomultibyte>
- Microsoft Learn，Code Page Identifiers：<https://learn.microsoft.com/en-us/windows/win32/intl/code-page-identifiers>
- Microsoft Learn，setlocale、_wsetlocale：<https://learn.microsoft.com/en-us/cpp/c-runtime-library/reference/setlocale-wsetlocale>
- Microsoft Learn，SetConsoleOutputCP：<https://learn.microsoft.com/en-us/windows/console/setconsoleoutputcp>
- Microsoft Learn，Classic Console APIs versus Virtual Terminal Sequences（Unicode 一节）：<https://learn.microsoft.com/en-us/windows/console/classic-vs-vt>
- Microsoft Learn，Application manifests（activeCodePage）：<https://learn.microsoft.com/en-us/windows/win32/sbscs/application-manifests>
- GNU Binutils，windres：<https://sourceware.org/binutils/docs/binutils/windres.html>
- GCC 文档，x86 Windows Options（`-municode`）：<https://gcc.gnu.org/onlinedocs/gcc/x86-Windows-Options.html>
- Jeffrey Richter、Christophe Nasarre，*Windows via C/C++*，第 5 版，第 2 章（字符与字符串）。
- Johnson M. Hart，*Windows System Programming*，第 4 版，第 2 章（Unicode Strategies）。
