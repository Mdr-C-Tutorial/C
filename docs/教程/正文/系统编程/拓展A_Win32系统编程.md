# 拓展A：Windows 系统编程

同一个回声服务器，主线用一份几十行的 C 程序就能写完：`socket`、`bind`、`listen`、`accept`，再用 `poll` 或 `epoll` 等连接。把它搬到 Windows 上，麻烦从第一行开始：套接字库要先 `WSAStartup`，关闭要用 `closesocket`，错误码换成了 `WSA` 开头的一组；想让程序开机就运行、不依赖任何用户登录，还得把它写成服务。文件、进程、线程、同步、终端也各有一套接口。主线第四部分学过的 POSIX 接口在这里大多没有对应物，少数同名的语义也不同：`CreateFileW` 比 `open` 多一个共享模式，`CreateProcessW` 一次做完 `fork` 和 `exec` 两件事，进程间通信靠管道与共享内存，而不是统一的文件描述符。

本拓展按主线的问题顺序展开，每一页挑主线的一两章作对照：先讲 Windows 的模型，再讲接口，最后给出可以编译运行的示例。示例统一用 MinGW-w64 GCC 13.2（UCRT 运行库）编译，A.5 的 `__try`、`__except`、`__finally` 只有 MSVC 支持。基线是 Windows 10 1809 及以后的 x64 系统；更早的系统上 ConPTY 不可用，A.17 用不了，个别特性另有最低版本要求，在相应页面上标出。

## 1. 本章内容

18 页按侧栏顺序排列，前五页是共同基础，后面按方向分组。

1. [A.1 Win32 API 概览与 POSIX 对照](/教程/正文/系统编程/拓展A_Win32系统编程/A_1_Win32API概览与POSIX对照)：Windows API 分成哪几层，头文件、类型名与返回值各有什么约定，主线学过的每个 POSIX 概念对应本拓展的哪一页，最后给出完整的对照表。
2. [A.2 字符与文本：ANSI、Unicode 与 UTF-16](/教程/正文/系统编程/拓展A_Win32系统编程/A_2_字符与文本_ANSI_Unicode与UTF16)：`CreateFileA` 为什么把「数据.txt」写成「鏁版嵁.txt」，A 与 W 两套函数、代码页、UTF-16 与 UTF-8 的转换，以及命令行与控制台的编码问题。
3. [A.3 内核对象与句柄](/教程/正文/系统编程/拓展A_Win32系统编程/A_3_内核对象与句柄)：内核对象与句柄的关系，`CreateFileW` 与 `OpenEventW` 失败时的返回值为什么不同，命名对象怎样实现单实例，句柄泄漏怎样查。
4. [A.4 文件与目录操作](/教程/正文/系统编程/拓展A_Win32系统编程/A_4_文件与目录操作)：`CreateFileW` 的七个参数、共享模式、文件属性与时间、目录遍历与路径处理，以及临时文件加改名的原子替换和字节范围锁。
5. [A.5 错误处理与结构化异常处理（SEH）](/教程/正文/系统编程/拓展A_Win32系统编程/A_5_错误处理与结构化异常处理)：`GetLastError` 的取法与错误消息的格式化，几种错误码的对照，MSVC 下的结构化异常处理，以及 MinGW 下用向量化异常处理与未处理异常筛选器留下崩溃现场。
6. [A.6 内存管理与内存映射文件](/教程/正文/系统编程/拓展A_Win32系统编程/A_6_内存管理与内存映射文件)：地址空间的三种页面状态，`VirtualAlloc` 与堆的分工，文件映射对象与视图，以及命名的共享内存。
7. [A.7 动态链接库（DLL）](/教程/正文/系统编程/拓展A_Win32系统编程/A_7_动态链接库_DLL)：导出表与导入表，加载时链接与 `LoadLibraryW` 的运行时链接，`DllMain` 与加载器锁，以及 DLL 搜索顺序带来的安全问题。
8. [A.8 进程的创建、执行与等待](/教程/正文/系统编程/拓展A_Win32系统编程/A_8_进程的创建_执行与等待)：Windows 为什么没有 `fork`，`CreateProcessW` 的每个参数与命令行字符串的引用规则，等待子进程与取退出码，环境块、进程枚举、作业对象与并行启动。
9. [A.9 线程与线程本地存储](/教程/正文/系统编程/拓展A_Win32系统编程/A_9_线程与线程本地存储)：`CreateThread` 与 `_beginthreadex` 的差别，线程的 ID、栈、优先级与名字，以及 `_Thread_local`、TLS、FLS 三套让数据每个线程一份的接口。
10. [A.10 同步原语（一）：等待函数与内核对象](/教程/正文/系统编程/拓展A_Win32系统编程/A_10_同步原语_等待函数与内核对象)：有信号与无信号两种状态，等待函数，事件、互斥量、信号量、可等待计时器，跨进程通知与可警报等待。
11. [A.11 同步原语（二）：临界区、SRWLock 与条件变量](/教程/正文/系统编程/拓展A_Win32系统编程/A_11_同步原语_临界区_SRWLock与条件变量)：互锁函数、临界区对象、SRW 锁与条件变量，以及什么情况下该回到 A.10 的内核对象。
12. [A.12 管道与进程间通信](/教程/正文/系统编程/拓展A_Win32系统编程/A_12_管道与进程间通信)：匿名管道接管子进程的标准输入输出，命名管道做请求与应答，消息模式怎样保住每次写入的边界，以及管道的默认安全设置。
13. [A.13 Winsock 特有主题](/教程/正文/系统编程/拓展A_Win32系统编程/A_13_Winsock特有主题)：Winsock 的初始化、`SOCKET` 与文件描述符的差别、WSA 错误码、可移植的回声服务器，以及 `WSAPoll`、`WSAEventSelect`、`AcceptEx`、`AF_UNIX` 这些 Windows 扩展。
14. [A.14 异步 I/O 与重叠结构](/教程/正文/系统编程/拓展A_Win32系统编程/A_14_异步IO与重叠结构)：就绪通知与完成通知的差别，`OVERLAPPED` 结构，用事件、`GetOverlappedResult` 和完成例程三种方式收取结果，取消与超时。
15. [A.15 I/O 完成端口与线程池](/教程/正文/系统编程/拓展A_Win32系统编程/A_15_IO完成端口与线程池)：完成端口怎样把连接数与等待对象数解耦，完成包与完成键，用完成端口写回声服务器，以及 Windows 线程池的四类对象。
16. [A.16 安全与访问控制（ACL 与令牌）](/教程/正文/系统编程/拓展A_Win32系统编程/A_16_安全与访问控制_ACL与令牌)：一次访问检查有哪些参与者，SID、访问令牌、UAC 与完整性级别，安全描述符与 ACL，怎样为新对象指定 DACL，以及特权的正确用法。
17. [A.17 控制台与 ConPTY](/教程/正文/系统编程/拓展A_Win32系统编程/A_17_控制台与ConPTY)：控制台的输入缓冲区与屏幕缓冲区，控制台模式，虚拟终端序列，`ReadConsoleInputW`，Ctrl+C 与控制事件，以及用 ConPTY 托管控制台程序。
18. [A.18 Windows 服务、注册表与系统设施](/教程/正文/系统编程/拓展A_Win32系统编程/A_18_Windows服务_注册表与系统设施)：服务的结构、在控制台里调试服务、安装与管理，注册表的读写与清理，以及已知文件夹、事件日志等系统设施。

## 2. 学习顺序建议

A.1～A.5 是共同基础，按顺序读完再选方向。A.1 给出分层与对照表，A.2 讲字符，A.3 讲句柄，A.4 讲文件，A.5 讲错误与异常；后面每一页都会用到这五页里的结论。

- 文件与内存：A.4 → A.6 → A.7。先会打开文件、遍历目录，再学地址空间、堆与映射文件，最后把代码装进 DLL。
- 进程与线程：A.8 → A.9 → A.10 → A.11。进程创建、线程创建、等待与同步，顺序不要换：A.10 的等待函数作用在 A.8、A.9 引入的进程与线程对象上。
- 通信与 I/O：A.12 → A.13 → A.14 → A.15。管道、套接字、重叠 I/O、完成端口，正好是从阻塞读到完成通知的一条线。
- 安全与系统服务：A.16 → A.17 → A.18。访问控制、控制台与服务，最后把回声服务器包装成服务 `MdrEcho`。

只写网络服务器的读者可以只走 A.1～A.3、A.5、A.9～A.11、A.13～A.15、A.18；需要本机通信时再补 A.12。做命令行工具或插件宿主的读者，A.4、A.6、A.7 与 A.17 不能跳。

### 2.1 阅读依赖

箭头从先读的页指向后读的页。图里只画主要的依赖，同一组内部的顺序与上面列的一致。

```mermaid
flowchart TB
    A1["A.1 概览与对照"] --> A2["A.2 字符与文本"]
    A2 --> A3["A.3 内核对象与句柄"]
    A3 --> A4["A.4 文件与目录"]
    A4 --> A5["A.5 错误处理与 SEH"]
    A4 --> A6["A.6 内存与映射文件"]
    A6 --> A7["A.7 DLL"]
    A5 --> A8["A.8 进程"]
    A8 --> A9["A.9 线程与 TLS"]
    A9 --> A10["A.10 等待函数与内核对象"]
    A10 --> A11["A.11 临界区与条件变量"]
    A9 --> A12["A.12 管道与 IPC"]
    A10 --> A12
    A1 --> A13["A.13 Winsock"]
    A5 --> A13
    A10 --> A13
    A10 --> A14["A.14 异步 I/O"]
    A12 --> A14
    A14 --> A15["A.15 IOCP 与线程池"]
    A13 --> A15
    A3 --> A16["A.16 安全与访问控制"]
    A12 --> A16
    A8 --> A17["A.17 控制台与 ConPTY"]
    A12 --> A17
    A15 --> A18["A.18 服务与注册表"]
    A16 --> A18
    A17 --> A18
```

### 2.2 与主线各章的对应

下表按主线的章序排，列出每一章在本拓展里接续的页面，第三列是两边写法上最要紧的一处差别。只有第 9 章没有对照页：TCP 与 UDP 的行为与平台无关，那一章的内容换成 Winsock 之后一条都不变。

| 主线章 | 对照页 | 主要差别 |
| --- | --- | --- |
| [1. 平台边界、POSIX 与系统调用](/教程/正文/系统编程/1_平台边界_POSIX与系统调用) | A.1 | 应用代码调用的是 DLL 导出的函数，`syscall` 指令藏在 `ntdll.dll` 里，程序不直接发系统调用 |
| [2. 文件描述符与非缓冲 I/O](/教程/正文/系统编程/2_文件描述符与非缓冲IO) | A.3、A.4 | 句柄是不透明值，不能像描述符那样做算术；文件句柄带共享模式 |
| [3. 文件系统、目录与权限](/教程/正文/系统编程/3_文件系统_目录与权限) | A.4、A.16 | 目录遍历用 `FindFirstFileExW` 一组函数，权限写在安全描述符里 |
| [4. 虚拟内存与内存映射](/教程/正文/系统编程/4_虚拟内存与内存映射) | A.6 | 保留与提交分开，分配粒度 64 KiB，映射文件要先建对象再取视图 |
| [5. 进程的创建、执行与回收](/教程/正文/系统编程/5_进程的创建_执行与回收) | A.8 | 没有 `fork`，`CreateProcessW` 一步完成创建与加载 |
| [6. 信号、时间与定时器](/教程/正文/系统编程/6_信号_时间与定时器) | A.5、A.10、A.17 | 没有与 POSIX 信号对应的通用机制，控制事件、结构化异常、可等待计时器与 APC 分担了它的角色 |
| [7. 进程间通信](/教程/正文/系统编程/7_进程间通信) | A.12 | 匿名管道、命名管道与共享内存各自独立，没有统一的描述符 |
| [8. 操作系统线程与同步](/教程/正文/系统编程/8_操作系统线程与同步) | A.9、A.10、A.11 | 内核对象与用户态原语两套并存，等待函数能同时处理多种对象 |
| [9. 网络基础](/教程/正文/系统编程/9_网络基础) | 无 | 协议本身与平台无关，这一章不必对照；套接字接口见 A.13 |
| [10. 套接字编程](/教程/正文/系统编程/10_套接字编程) | A.13 | 要先 `WSAStartup`，`SOCKET` 不是文件描述符，关闭用 `closesocket` |
| [11. 非阻塞 I/O 与 I/O 多路复用](/教程/正文/系统编程/11_非阻塞IO与IO多路复用) | A.13、A.14 | `select` 与 `WSAPoll` 仍在，重叠 I/O 提供了另一条完成通知的路 |
| [12. 事件循环](/教程/正文/系统编程/12_事件循环) | A.14、A.15 | 完成端口把等待队列交给内核，线程数不再随连接数增长 |
| [13. 终端、伪终端与作业控制](/教程/正文/系统编程/13_终端_伪终端与作业控制) | A.17 | 控制台模式取代 termios，颜色与光标用虚拟终端序列，ConPTY 对应 pty |
| [14. 系统程序的健壮性与安全边界](/教程/正文/系统编程/14_系统程序的健壮性与安全边界) | A.5、A.16 | 崩溃处理靠结构化异常与未处理异常筛选器，访问控制按令牌与 ACL 判定 |

各页开头都会说明它对应主线的哪一章、先读哪几页；正文遇到与 POSIX 写法不同的地方，会直接给出两边的片段对照。

### 2.3 编译环境与常用库

示例统一在 Windows 10 1809 及以后的 x64 系统上，用 MinGW-w64 GCC 13.2（UCRT 运行库）以 `gcc -std=c17 -Wall -Wextra` 编译；A.5 的 `__try`、`__except`、`__finally` 只有 MSVC 认，那几个程序用 Visual Studio 的 `cl` 编译。各页的输出都是在本机运行得到的，系统错误消息随系统界面语言变化。

动手之前先编译一个自检程序，确认编译器装好、系统参数与书中一致。后面各页反复用到这几个数字：页面大小决定一次读写多少字节合适，分配粒度决定映射文件时偏移怎么对齐，代码页决定 A 版本函数怎样解释窄字符串，`wchar_t` 的宽度决定 UTF-16 的每个码元占几个字节。

```c
/* envcheck.c：打印与后续各页有关的系统参数。 */
#include <stdio.h>
#include <windows.h>

int main(void) {
    SYSTEM_INFO si;

    GetSystemInfo(&si);
    printf("page size:            %lu\n", (unsigned long)si.dwPageSize);
    printf("allocation granularity: %lu\n", (unsigned long)si.dwAllocationGranularity);
    printf("logical processors:   %lu\n", (unsigned long)si.dwNumberOfProcessors);
    printf("ANSI code page:       %lu\n", (unsigned long)GetACP());
    printf("OEM code page:        %lu\n", (unsigned long)GetOEMCP());
    printf("console output CP:    %lu\n", (unsigned long)GetConsoleOutputCP());
    printf("sizeof(wchar_t):      %llu\n", (unsigned long long)sizeof(wchar_t));
    printf("sizeof(HANDLE):       %llu\n", (unsigned long long)sizeof(HANDLE));
    return 0;
}
```

::: terminal title="PowerShell - envcheck"

<span class="prompt">PS&gt;</span> gcc -std=c17 -Wall -Wextra envcheck.c -o envcheck.exe

<span class="prompt">PS&gt;</span> ./envcheck.exe

```text
page size:            4096
allocation granularity: 65536
logical processors:   16
ANSI code page:       936
OEM code page:        936
console output CP:    936
sizeof(wchar_t):      2
sizeof(HANDLE):       8
```

:::

这台机器上的值分别是 4 KiB 的页面、64 KiB 的分配粒度、16 个逻辑处理器，代码页 936（简体中文），`wchar_t` 2 字节、句柄 8 字节。页面大小与分配粒度在常见的 x64 系统上都是这两个值，处理器个数因机器而异。代码页不是 936 的读者，运行 A.2 的程序时看到的现象会与书上不同，那一页会解释代码页在其中起的作用。

::: tip 编译命令与要加的库

通用的编译命令是：

```text
gcc -std=c17 -Wall -Wextra -o 程序.exe 程序.c
```

用 `wmain` 接收命令行参数的示例再加 `-municode`。按需在命令行末尾追加库：Winsock 加 `-lws2_32`，直接调用 `AcceptEx` 加 `-lmswsock`，`WaitOnAddress` 加 `-lsynchronization`，`SHGetKnownFolderPath` 加 `-lole32 -luuid`，`MiniDumpWriteDump` 加 `-ldbghelp`。kernel32、user32、advapi32、shell32 是 MinGW 默认链接的，注册表、安全与服务的调用不必另外加库。A.17 的 ConPTY 示例要加 `-D_WIN32_WINNT=0x0A00 -DNTDDI_VERSION=0x0A000006`，否则 `HPCON` 类型没有定义。库只写在命令行上，不写 `#pragma comment(lib, ...)`：GCC 会忽略它并给出警告，程序照样链接不上。

:::

各页较长的程序放在 `code/系统编程/拓展A_Win32系统编程/` 下与页面同名的子目录里，正文只摘关键片段并给出路径；A.7 的 DLL 示例和 A.8 的多文件示例附 `Makefile`，用 `mingw32-make` 构建。

## 3. 习题

四道题按顺序做，最后一道要求把主线的一页搬过来。前两题只用本页与各页开头的环境说明，第三题开始需要装好 MinGW-w64。

<Exercise id="45001" :d="1" :w="1">

把本页的 `envcheck.c` 抄下来编译运行（源码就在第 2.3 节），然后回答：

1. 输出里的 `page size` 与 `allocation granularity` 分别由哪一页用到、用来做什么？各写一句。
2. `ANSI code page` 是 936 时，A 版本函数把「数据」解释成什么编码？换成 65001 又是什么？
3. `sizeof(wchar_t)` 是 2，由此推出 `L"中文"` 占几个字节、几个代码单元；`L"\U0001F600"` 呢？
4. 把程序里的 `GetACP()` 换成 `GetConsoleOutputCP()`，在 PowerShell 里先执行 `chcp 65001` 再运行，两个数字是否相同？这说明两个函数各代表什么。

</Exercise>

<Exercise id="45002" :d="3" :w="2">

写一个 `sysprobe.exe`，一次打印六个数字或字符串，每个都对应本拓展的一页：

1. 系统版本，用 `ntdll.dll` 里的 `RtlGetVersion`（`GetProcAddress` 取地址，`winternl.h` 给出 `RTL_OSVERSIONINFOW`，A.18）；
2. 页面大小与分配粒度（`GetNativeSystemInfo`，A.6）；
3. 当前进程的会话 ID（`ProcessIdToSessionId`，A.12）；
4. 令牌是否提权（`TokenElevation`）与完整性级别 RID（`TokenIntegrityLevel`，A.16）；
5. 当前进程的句柄数（`GetProcessHandleCount`，A.3）。

输出固定成六行 `键=值`，最后一行是这六项分别出自哪一页。要求用 `gcc -std=c17 -Wall -Wextra` 编译无警告。

验收（本机一次运行，数值随机器和登录方式变化）：`version=10.0.26300`、页面 4096、粒度 65536、会话 ID 1、普通未提权 PowerShell 下 `elevated=no`、完整性 RID `0x2000`（中）、句柄数是几十到几百之间的一个数。回答：为什么 `GetVersionExW` 在这台机器上会给出 `6.2 build 9200`，而 `RtlGetVersion` 给出真实版本？

</Exercise>

<Exercise id="45003" :d="3" :w="2">

本页 2.3 节的 tip 列了六个「按需追加的命令行选项」。逐条验证，每条记下不加时的错误原文：

1. `WaitOnAddress`（A.11）不加 `-lsynchronization`；
2. 直接调用 `AcceptEx`（A.13）不加 `-lmswsock`，`-lws2_32` 照加；
3. `SHGetKnownFolderPath`（A.18）不加 `-lole32 -luuid`；
4. `MiniDumpWriteDump`（A.5）不加 `-ldbghelp`；
5. `wmain`（A.2）不加 `-municode`；
6. `CreatePseudoConsole`（A.17）不加 `-D_WIN32_WINNT=0x0A00 -DNTDDI_VERSION=0x0A000006`。

每个程序三五行为够。然后回答：

- 第 5 条报的是 `undefined reference to 'WinMain'`，为什么这个错误与 `wmain` 看起来毫无关系？
- 第 6 条报的是编译错误还是链接错误，错误文本里出现的是哪个类型？
- 把 `#pragma comment(lib, "ws2_32")` 写进源码代替命令行，GCC 给出什么警告，程序能不能链接成功？
- 库参数写在源文件之前会怎样？用 `gcc -lws2_32 ws.c -o ws` 与 `gcc ws.c -o ws -lws2_32` 各试一次。

</Exercise>

<Exercise id="45004" :d="5" :w="3">

把主线[第 2 章](/教程/正文/系统编程/2_文件描述符与非缓冲IO)的 `code/系统编程/2_文件描述符与非缓冲IO/copybuf.c` 搬到 Windows。第一遍什么也不改，直接 `gcc -std=c17 -Wall -Wextra copybuf.c -o copybuf.exe`，记下报错；把两处 `O_CLOEXEC` 删掉再编译，这次能链接，但结果不对。

要求：

1. 生成一个约 10 KiB 的二进制文件，内容是 0 到 255 各 40 次再跟 `\r\n\x1a` 三个字节；用它做源文件。
2. 先用改过 `O_CLOEXEC` 的版本复制一遍，用 `cmp` 与源文件比，记录第一处不同的偏移，并用 `od -An -tx1` 把源文件与结果在那一带的字节各打印 16 个。
3. 用 `ReadFile`/`WriteFile` 重写一版 `copyw.exe`，用 `CreateFileW` 打开，复制结果必须与源文件逐字节相同。
4. 不重写，只给 CRT 版本加两行 `_setmode(in, _O_BINARY)` 与 `_setmode(out, _O_BINARY)`（在 `open` 之后各调一次），也能修好；说明修的是哪一层。
5. 用一句半的话说明：为什么文本模式的差异在纯文本文件上看不出来，一遇到 `0x0A`、`0x1A` 就暴露。

验收（本机实测）：原版报 `'O_CLOEXEC' undeclared`；删掉后复制出的文件在偏移 10 处开始不同，源文件该处是 `0a`，结果是 `0d 0a`；输出文件只剩 27 字节（`0x1A` 之前的 26 个源字节，加上那个 `0x0D`），源文件是 10243 字节；`copyw.exe` 的结果 `cmp` 无差异。

</Exercise>

## 4. 参考资料

- Microsoft Learn，Windows API index：<https://learn.microsoft.com/en-us/windows/win32/apiindex/windows-api-list>
- Johnson M. Hart，**_Windows System Programming_**，第 4 版，Addison-Wesley，2010，ISBN 978-0-321-65774-9（章序与 Windows/UNIX 对照见附录 B）
- Jeffrey Richter、Christophe Nasarre，**_Windows via C/C++_**，第 5 版，Microsoft Press，ISBN 978-0-7356-6377-0
- Pavel Yosifovich，**_Windows 10 System Programming, Part 1_**：<https://leanpub.com/windows10systemprogramming>
- Pavel Yosifovich 等，**_Windows Internals_**，第 7 版 Part 1，Microsoft Press，ISBN 978-0-7356-8378-8（进程、线程、内存管理与对象管理器）
- MinGW-w64 项目主页：<https://www.mingw-w64.org/>
