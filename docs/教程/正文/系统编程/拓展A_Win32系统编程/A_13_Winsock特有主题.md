# A.13 Winsock 特有主题

[第 10 章](/教程/正文/系统编程/10_套接字编程)的 TCP 回声服务器在 Linux 上编译运行没有问题：`socket`、`bind`、`listen`、`accept`、`recv`、`send`、`close` 一组调用，几十行代码。把这份源码原样拿到 Windows 上，问题会一个接一个出来。头文件 `<sys/socket.h>`、`<netinet/in.h>`、`<arpa/inet.h>` 一个都找不到；换成 Winsock 的头文件之后，链接器报 `undefined reference to '__imp_socket'`；加了库链接，程序能起来了，`socket` 却返回失败，错误码 10093 的说明是「应用程序没有调用 WSAStartup」；把初始化补上，收发都正常了，可连接关不掉：`close` 返回 -1，套接字还活着，对端一直等不到结束。

Winsock（Windows Sockets）是 Windows 上的套接字接口，它照着 BSD 套接字的形状设计，但有自己的一套初始化、类型、错误码和关闭方式。这一页按上面这条排错顺序，把移植时要改的地方逐条讲清楚，最后得到一份在 Windows 和 Linux 上都能编译运行的回声服务器。再往后讲 Windows 在 BSD 之外多出来的东西：`WSAPoll`、把套接字变成可等待对象的 `WSAEventSelect`，以及重叠套接字和 `AF_UNIX`。

本页所有程序在本机 MinGW-w64 GCC 13.2 上编译运行，输出照实粘贴。可移植的那一份另外在一台 Linux 主机上用 GCC 14.2 编译验证过。端口一律取 23456 附近，只绑 `127.0.0.1`。

## 1. 头文件、初始化与链接

Winsock 的头文件有两个：`<winsock2.h>` 声明函数、类型和常量，`<ws2tcpip.h>` 补上 `getaddrinfo`、`getnameinfo`、`inet_pton` 这些较新的地址转换函数。两者都必须在 `<windows.h>` 之前包含，因为 `<windows.h>` 会带上 Winsock 1.1 的 `<winsock.h>`，先包含它再包含 `<winsock2.h>` 会出现两套定义。MinGW 的头文件对顺序错位给出了明确的警告：

```c
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
```

`WIN32_LEAN_AND_MEAN` 让 `<windows.h>` 少包含一批用不到的头文件（加密、DDE、RPC 等），编译更快，也减少与其他头文件撞名的机会。本拓展的各页都写这一行。

顺序写错时，编译不报错，只给一条警告：

```text
In file included from order_test.c:2:
.../include/winsock2.h:15:2: warning: #warning Please include winsock2.h before windows.h [-Wcpp]
```

警告之后程序可能照常编译链接，但两套声明混在一起，`fd_set`、`timeval` 这类同名结构会取到哪一份取决于包含顺序，行为不可预料。把顺序改对。

链接要加 `-lws2_32`。Winsock 的函数全在 `ws2_32.dll` 里，导入库是 `libws2_32.a`，MinGW 的默认链接列表里没有它。漏掉时的报错很直白：

```text
ld.exe: ...wsa_init.c:(.text+0x164): undefined reference to `__imp_socket'
ld.exe: ...wsa_init.c:(.text+0x197): undefined reference to `__imp_WSAGetLastError'
ld.exe: ...wsa_init.c:(.text+0x1bd): undefined reference to `__imp_WSAStartup'
```

`__imp_` 前缀表示编译器把调用编成了对导入地址表的间接跳转，链接器找不到对应的表项。看到 `__imp_socket`、`__imp_closesocket`，第一反应就是检查命令行里有没有 `-lws2_32`。

头文件和库都齐了，还差运行期的初始化。每个使用 Winsock 的进程都要在任何套接字调用之前调用一次 `WSAStartup`，结束时调用 `WSACleanup`：

```c
WSADATA wsa;
int rc = WSAStartup(MAKEWORD(2, 2), &wsa);
if (rc != 0) {
    fprintf(stderr, "WSAStartup failed: %d\n", rc);
    return EXIT_FAILURE;
}
```

`MAKEWORD(2, 2)` 把主版本和次版本拼成一个 16 位值，请求 Winsock 2.2。`WSAStartup` 把错误码直接放在返回值里，判断它只看返回值，不要再去调 `WSAGetLastError`。第二个参数是输出：`wVersion` 是实际使用的版本，`wHighVersion` 是系统支持的最高版本，`szDescription` 是一段描述文字。本机请求 2.2 得到的是 `version 2.2, high 2.2` 和 `"WinSock 2.0"`；请求 1.1 时 `wVersion` 就是 1.1，`wHighVersion` 仍是 2.2，说明版本是协商出来的，程序要按 `wVersion` 而不是按自己请求的版本去用接口。调用成功后，进程里就要一直记着在退出前配对调用 `WSACleanup`。

初始化是可嵌套的，`WSAStartup` 每调用一次内部计数加一，`WSACleanup` 减一，减到零才真正卸载。这个设计是为了让一个进程里的多个库各自调用一次而互不干扰。完整程序在 `code/系统编程/拓展A_Win32系统编程/A_13_Winsock特有主题/wsa_init.c`，它把忘记初始化、重复初始化、重复清理三种情况都走了一遍：

::: terminal title="PowerShell - wsa_init"

<span class="prompt">PS&gt;</span> gcc -std=c17 -Wall -Wextra wsa_init.c -o wsa_init.exe -lws2_32

<span class="prompt">PS&gt;</span> ./wsa_init.exe

```text
sizeof(SOCKET) = 8
before WSAStartup
  socket failed: 10093 (应用程序没有调用 WSAStartup，或者 WSAStartup 失败。)
WSAStartup #1 = 0, version 2.2, high 2.2, "WinSock 2.0"
after WSAStartup #1 -> socket ok: 268
WSAStartup #2 = 0
WSACleanup #1 = 0
after one WSACleanup -> socket ok: 268
WSACleanup #2 = 0
after two WSACleanup
  socket failed: 10093 (应用程序没有调用 WSAStartup，或者 WSAStartup 失败。)
WSACleanup #3 = -1
  WSACleanup failed: 10093 (应用程序没有调用 WSAStartup，或者 WSAStartup 失败。)
```

:::

这一段的结果值得记住：两次 `WSAStartup` 之后计数为 2，第一次 `WSACleanup` 把它减到 1，套接字照常可用；第二次减到 0，套接字立刻失败；第三次 `WSACleanup` 本身返回 -1，错误码 10093。也就是说清理多了不会崩溃，但会悄悄把整个进程的 Winsock 关掉，之后任何套接字调用都报「没有调用 WSAStartup」。这类错误在多线程程序里尤其难查：一个线程多调了一次清理，另一个线程的服务器就再也起不来。

::: tip 把初始化和清理放进一对函数

Winsock 的初始化和清理是进程级的，而套接字的使用往往是模块级的。工程上的做法是在程序入口调用一次 `WSAStartup`，把返回值当成启动失败处理；需要初始化的库不要各自调用，而是由宿主保证已经初始化。C 运行库的 `atexit` 里挂一次 `WSACleanup` 也可以，但要确保退出路径都走得到。

:::

## 2. SOCKET 不是文件描述符

Linux 上套接字就是一个文件描述符，`read`、`write`、`close`、`poll` 全都能直接用。Windows 不是：套接字是 Winsock 自己的一类句柄，与内核的文件句柄、与 C 运行库的文件描述符都不是一回事。

::: concept wsa-socket-handle title="SOCKET 的类型与失败值"

`SOCKET` 是无符号整数类型（`UINT_PTR`），x64 上 8 字节，取值范围从 0 到 `INVALID_SOCKET - 1`。创建失败时返回 `INVALID_SOCKET`（值为 `(SOCKET)(~0)`，x64 上是 18446744073709551615），其他 Winsock 函数失败时返回 `SOCKET_ERROR`（值为 -1）。判断失败要用这两个常量，不要写 `s < 0`。关闭套接字用 `closesocket`，不能用 C 运行库的 `close` 或 `_close`，也不能用 `CloseHandle`。

:::

用 `unsigned long long` 加 `%llu` 来输出套接字句柄正合适，因为它是无符号的 64 位值（MinGW 的 `printf` 按微软规则检查格式，`%zu` 会报错，见 [A.1](/教程/正文/系统编程/拓展A_Win32系统编程/A_1_Win32API概览与POSIX对照)）。拿 POSIX 的写法直接套，编译器会逐条拦下来：

```c
SOCKET s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
if (s < 0) {              /* warning: comparison of unsigned expression in '< 0' is always false */
    ...
}
if (s == -1) {            /* warning: comparison of integer expressions of different signedness */
    ...
}
```

`-Wall -Wextra` 下第一条报 `-Wtype-limits`，第二条报 `-Wsign-compare`。这两条警告都是真问题：`s < 0` 永远为假，程序会带着一个无效句柄继续往下走。

`close` 的问题更隐蔽。下面的程序建立一对连上的套接字，在服务器一侧对连接套接字调用 C 运行库的 `close`：

::: terminal title="PowerShell - close_demo"

<span class="prompt">PS&gt;</span> gcc -std=c17 -Wall -Wextra close_demo.c -o close_demo.exe -lws2_32

<span class="prompt">PS&gt;</span> ./close_demo.exe

```text
accepted socket = 268 (int 装得下吗: 是)
close(268) = -1, errno = 9
send after close = 12
client recv = 12: still alive
closesocket = 0
```

:::

`close` 返回 -1、`errno` 为 9（`EBADF`），因为 268 在 C 运行库的文件描述符表里根本不存在。套接字本身毫发无损，`send` 照样发出 12 字节，对端完整收到。这就是开头说的最后一种情况：程序不报错，连接关不掉，对端一直等。换成 `_close` 同样返回 -1、`errno` 为 9。只有 `closesocket` 才会真正释放它。

`CloseHandle` 更危险，因为它看起来成功了。本机实测对一个套接字调用 `CloseHandle` 返回 TRUE，之后再用这个句柄做套接字操作（`ioctlsocket`）报 10038，而对端那边 `recv` 拿到的是 10054（连接被复位），不是正常的 0。也就是说句柄被拆掉了，却没有走正常的关闭序列，已经排队的数据和半关闭的握手全部被跳过。这个错误在排查时特别费时间，因为服务端什么都不报。

两个 API 的对应关系整理成一张表，移植时照着改：

| POSIX | Winsock | 说明 |
| --- | --- | --- |
| `close(fd)` | `closesocket(s)` | 释放套接字，别用 `close` |
| `read` / `write` | `recv` / `send` | 缓冲区参数是 `char *`，不是 `void *` |
| `fcntl(fd, F_SETFL, O_NONBLOCK)` | `ioctlsocket(s, FIONBIO, &on)` | `on` 是 `u_long`，1 为非阻塞 |
| `fcntl(fd, F_GETFL)` 判断阻塞 | `ioctlsocket(s, FIONBIO, &on)` 读回 | 同上 |
| `setsockopt(..., &tv, sizeof tv)` 设超时 | `setsockopt(..., &ms, sizeof ms)` | `SO_RCVTIMEO` 取 `DWORD` 毫秒 |
| `shutdown(fd, SHUT_WR)` | `shutdown(s, SD_SEND)` | 半关闭，值不同 |
| `errno` / `EAGAIN` | `WSAGetLastError()` / `WSAEWOULDBLOCK` | 见下一节 |

`send` 和 `recv` 的第二个参数类型是 `const char *` 和 `char *`。传一个 `int *` 过去，GCC 报 `-Wincompatible-pointer-types`；传 `size_t` 当长度，`-Wconversion` 下会提示可能截断。写跨平台代码时，缓冲区声明成 `char` 数组，长度用 `int` 或者显式转换。

还有一点容易忽略：套接字句柄不能交给 `ReadFile` 或 `WriteFile`。本机实测 `ReadFile` 对一个已连接的套接字返回 FALSE、错误 87（`ERROR_INVALID_PARAMETER`）。要跨平台地用「像文件一样读写」的接口，就得自己包一层，在 Windows 分支里转成 `recv`/`send`。

<Quiz
  type="single"
  :options="[
    { label: 'close(fd)', value: 'A' },
    { label: 'CloseHandle(h)', value: 'B' },
    { label: 'closesocket(s)', value: 'C' },
    { label: 'fclose(fp)', value: 'D' }
  ]"
  answer="C"
>

<template #question>

在 Winsock 里释放一个 `SOCKET` 应该调用哪个函数？

</template>

正确答案是 **C**。`SOCKET` 是 Winsock 自己的句柄，只有 `closesocket` 会释放它。

- `close` 和 `_close` 操作的是 C 运行库的文件描述符表，本机实测返回 -1、`errno` 为 9，套接字不受影响。
- `CloseHandle` 操作的是内核句柄表。本机实测它在这个套接字上返回 TRUE，句柄被拆掉，后续套接字操作报 10038，对端则收到连接复位（10054）而不是正常的结束。
- `fclose` 参数是 `FILE *`，与套接字无关。

</Quiz>

::: compare left-title="POSIX 写法" right-title="Winsock 写法"

```c
int fd = socket(AF_INET, SOCK_STREAM, 0);
if (fd < 0) {
    perror("socket");
}
int flags = fcntl(fd, F_GETFL, 0);
fcntl(fd, F_SETFL, flags | O_NONBLOCK);
struct timeval tv = {.tv_sec = 1, .tv_usec = 0};
setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof tv);
ssize_t n = read(fd, buf, sizeof buf);
close(fd);
```

---

```c
SOCKET s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
if (s == INVALID_SOCKET) {
    printf("socket: %d\n", WSAGetLastError());
}
u_long on = 1;
ioctlsocket(s, FIONBIO, &on);
DWORD ms = 1000;
setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, (const char *)&ms, sizeof ms);
int n = recv(s, buf, (int)sizeof buf, 0);
closesocket(s);
```

:::

## 3. 错误码

POSIX 用 `errno` 报告错误，Winsock 不用。套接字调用失败后，错误码要用 `WSAGetLastError()` 取；`errno` 保持原样，通常是 0。这一点在混合使用 CRT 文件操作和套接字的程序里特别容易出错：`connect` 连一个没人监听的端口失败后，`perror` 打印的是 `connect: No error`，`errno` 仍然是 0。

::: terminal title="PowerShell - sock_errors"

<span class="prompt">PS&gt;</span> gcc -std=c17 -Wall -Wextra sock_errors.c -o sock_errors.exe -lws2_32

<span class="prompt">PS&gt;</span> ./sock_errors.exe

```text
1) 连接一个没人监听的端口
  WSAGetLastError(): 10061 (由于目标计算机积极拒绝，无法连接。)
  errno = 0
2) 两个套接字绑定同一端口（第二个不设任何选项）
  bind b: 10048 (通常每个套接字地址(协议/网络地址/端口)只允许使用一次。)
3) 同一个套接字重复绑定
  bind a again: 10022 (提供了一个无效的参数。)
4) 对已关闭的套接字调用 send
  send: 10038 (在一个非套接字上尝试了一个操作。)
5) 同一次失败里分别用两个函数取错误码
  WSAGetLastError = 10061, GetLastError = 10061, 相同 = 1
```

:::

第 5 组是本机实测的对照：同一次 `connect` 失败之后，`WSAGetLastError` 与 `GetLastError` 返回同一个值。文档给 Winsock 指定的入口是 `WSAGetLastError`，代码里只该用这一个，不要依赖两者当前恰好一致。

错误码都在 10000 以上，常量带 `WSA` 前缀，定义在 `<winsock2.h>` 里。常用的几个：

| 常量 | 值 | 中文说明（本机） | 出现在什么时候 |
| --- | --- | --- | --- |
| `WSAEWOULDBLOCK` | 10035 | 无法立即完成一个非阻止性套接字操作。 | 非阻塞套接字上没有数据 |
| `WSAEINVAL` | 10022 | 提供了一个无效的参数。 | 参数不对，或套接字已绑定 |
| `WSAENOTSOCK` | 10038 | 在一个非套接字上尝试了一个操作。 | 句柄无效 |
| `WSAEACCES` | 10013 | 以一种访问权限不允许的方式做了一个访问套接字的尝试。 | 端口被独占，抢绑被拒 |
| `WSAEADDRINUSE` | 10048 | 通常每个套接字地址(协议/网络地址/端口)只允许使用一次。 | 地址已被占用 |
| `WSAECONNRESET` | 10054 | 远程主机强迫关闭了一个现有的连接。 | 对端复位连接 |
| `WSAECONNABORTED` | 10053 | 你的主机中的软件中止了一个已建立的连接。 | 本机中止连接 |
| `WSAETIMEDOUT` | 10060 | 由于连接方在一段时间后没有正确答复或连接的主机没有反应，连接尝试失败。 | 超时 |
| `WSAECONNREFUSED` | 10061 | 由于目标计算机积极拒绝，无法连接。 | 端口没人监听 |
| `WSAEHOSTUNREACH` | 10065 | 套接字操作尝试一个无法连接的主机。 | 路由不可达 |
| `WSANOTINITIALISED` | 10093 | 应用程序没有调用 WSAStartup，或者 WSAStartup 失败。 | 忘了初始化 |

这些文字由 `FormatMessageW` 从系统消息表里取，[A.5](/教程/正文/系统编程/拓展A_Win32系统编程/A_5_错误处理与结构化异常处理) 里写的 `print_error` 直接就能用：把错误码当 `DWORD` 传给 `FormatMessageW`，`FORMAT_MESSAGE_FROM_SYSTEM` 会找到对应条目。`sock_errors.c` 开头那个 `print_wsa_error` 就是同一套代码的简化版。消息随系统界面语言变化，本机是中文，英文系统上会得到英文。

移植时把错误判断集中到两个小函数里，正文里的代码就干净了：

```c
static int sock_errno(void) {
#ifdef _WIN32
    return WSAGetLastError();
#else
    return errno;
#endif
}

static int sock_would_block(int e) {
#ifdef _WIN32
    return e == WSAEWOULDBLOCK;
#else
    return e == EAGAIN || e == EWOULDBLOCK || e == EINTR;
#endif
}
```

POSIX 侧把 `EINTR` 也算作「重来一次」，因为被信号打断的 `recv` 会返回它；Windows 上 Winsock 的阻塞调用被 APC 打断时的行为另有规定，一般不产生 `EINTR`。这个差异放进宏里，调用点就只需要写一次 `if (sock_would_block(sock_errno()))`。

第 3 组还说明了一个容易误判的现象：同一个套接字重复 `bind` 报的是 10022 而不是 10048。10048 是「地址被别人占了」，10022 是「这个套接字已经绑过了」。两者的处理方式不同，前者要换端口，后者是程序逻辑错了。

<Quiz
  type="single"
  :options="[
    { label: 'errno，与 POSIX 一致', value: 'A' },
    { label: 'WSAGetLastError()，Winsock 专用', value: 'B' },
    { label: 'strerror(errno)', value: 'C' },
    { label: 'h_errno', value: 'D' }
  ]"
  answer="B"
>

<template #question>

Winsock 函数失败之后，取错误码应该调用什么？

</template>

正确答案是 **B**。Winsock 的错误码有自己的编号空间（10000 以上）和自己的入口函数。

- A 错：`errno` 不被 Winsock 设置。本机实测 `connect` 失败后它仍是 0，`perror` 打印 `connect: No error`。
- C 错：`strerror` 只认识 C 运行库的 `errno` 编号，拿 10061 去查会得到「Unknown error」。
- D 错：`h_errno` 是旧式主机名解析函数的错误码，与套接字操作无关。

</Quiz>

## 4. 可移植的回声服务器

把前三节的东西合起来，就得到一份两边都能编译的回声服务器。思路是把平台差异收进一个 `#ifdef _WIN32` 分支，正文逻辑写成平台无关的。完整源码在 `code/系统编程/拓展A_Win32系统编程/A_13_Winsock特有主题/echo_portable.c`，骨架是这样的：

```c
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
typedef SOCKET sock_t;
#define sock_close(s) closesocket(s)
#define sock_invalid INVALID_SOCKET
#define SOCK_ERR SOCKET_ERROR
#else
#define _POSIX_C_SOURCE 200809L
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <signal.h>
#include <sys/socket.h>
#include <unistd.h>
typedef int sock_t;
#define sock_close(s) close(s)
#define sock_invalid (-1)
#define SOCK_ERR (-1)
#endif
```

类型用 `sock_t` 统一，关闭用 `sock_close`，失败值用 `sock_invalid`。`_POSIX_C_SOURCE` 只在不带 `_WIN32` 的分支里定义，Windows 分支看不到它。`signal(SIGPIPE, SIG_IGN)` 也只在 POSIX 分支里写。Windows 没有 `SIGPIPE`，向已经关闭的连接 `send` 只会返回错误码，不会让进程退出；本机实测对端优雅关闭后第一次 `send` 往往还能成功（数据进了缓冲区），第二次返回 10053（`WSAECONNABORTED`），对端复位的情况下则是 10054（`WSAECONNRESET`）。

监听套接字的建立是差异最大的地方。POSIX 上服务器要给 `SO_REUSEADDR`，Windows 上恰恰相反：

```c
#ifdef _WIN32
        /* Windows 服务器独占端口，防止别的进程用 SO_REUSEADDR 抢绑。 */
        BOOL exclusive = TRUE;
        if (setsockopt(fd, SOL_SOCKET, SO_EXCLUSIVEADDRUSE,
                       (const char *)&exclusive, sizeof exclusive) == SOCK_ERR) {
            sock_close(fd);
            fd = sock_invalid;
            continue;
        }
#else
        int one = 1;
        if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof one) == SOCK_ERR) {
            sock_close(fd);
            fd = sock_invalid;
            continue;
        }
#endif
```

这个分支不是对称的美化，两边选项的语义根本不同。BSD 的 `SO_REUSEADDR` 解决的是「重启时端口上还留着 `TIME_WAIT`」；Winsock 的 `SO_REUSEADDR` 是让人可以绑到别人已经占着的地址上，绑成之后谁来处理新连接没有保证。本机实测的重启行为更是说明了这一点：服务器关闭后端口上残留着 `TIME_WAIT` 连接，默认选项、`SO_REUSEADDR`、`SO_EXCLUSIVEADDRUSE` 三种情况立刻重绑全部成功，Windows 根本不需要 `SO_REUSEADDR` 来帮忙重启。

::: concept wsa-bind-options title="Windows 上 SO_REUSEADDR 与 SO_EXCLUSIVEADDRUSE"

在 Windows 上，`SO_REUSEADDR` 允许第二个套接字绑定到已被别的套接字绑定的地址和端口。绑定之后这些套接字的行为不确定，新连接由谁接受没有保证。本机实测：先绑 `127.0.0.1` 的套接字用默认选项时，后来者用 `SO_REUSEADDR` 绑同一地址失败并返回 `WSAEACCES`（10013）；先绑通配地址 `0.0.0.0` 的套接字不设选项时，后来者用 `SO_REUSEADDR` 绑 `127.0.0.1` 成功，回环上的新连接被后来者接走；两个套接字都设 `SO_REUSEADDR` 时也能绑在同一地址上。服务器要在 `bind` 之前设置 `SO_EXCLUSIVEADDRUSE` 独占端口：绑通配地址的独占套接字会让后来的抢绑失败，返回 `WSAEACCES`。两个选项都必须在 `bind` 之前设置才生效。

:::

把这件事演示出来只需要两个监听套接字：一个绑 `0.0.0.0`、用默认选项扮演服务，另一个绑 `127.0.0.1`、设 `SO_REUSEADDR` 扮演入侵者，然后从本机连一次，看谁接走了连接。完整程序是 `reuse_hijack.c`：

::: terminal title="PowerShell - reuse_hijack"

<span class="prompt">PS&gt;</span> gcc -std=c17 -Wall -Wextra reuse_hijack.c -o reuse_hijack.exe -lws2_32

<span class="prompt">PS&gt;</span> ./reuse_hijack.exe

```text
service: default + 0.0.0.0 -> bind ok
attacker: SO_REUSEADDR + 127.0.0.1 -> bind ok (error 0)
connection answered by: attacker
service: SO_EXCLUSIVEADDRUSE + 0.0.0.0 -> bind ok
attacker: SO_REUSEADDR + 127.0.0.1 -> bind failed (error 10013)
```

:::

第一组里服务什么都没设，绑在通配地址上，入侵者用 `SO_REUSEADDR` 绑回环地址，两个 `bind` 都成功，回环上的连接全部被入侵者接走。第二组里服务加了 `SO_EXCLUSIVEADDRUSE`，入侵者连 `bind` 都过不去，错误 10013。这就是 Microsoft 建议所有服务器都设 `SO_EXCLUSIVEADDRUSE` 的原因，也是移植时把 `SO_REUSEADDR` 照抄过来的危险之处：在 POSIX 上它是重启的便利，在 Windows 上它是把自己的端口交给别人。

客户端连上之后，服务器读到 EOF 就该关闭。Windows 的关闭序列与 POSIX 有一个必须写对的细节：

```c
static void close_gracefully(sock_t c) {
#ifdef _WIN32
    shutdown(c, SD_SEND);
#else
    shutdown(c, SHUT_WR);
#endif
    char buf[512];
    while (recv(c, buf, (int)sizeof buf, 0) > 0) {
    }
    sock_close(c);
}
```

先半关闭发送方向，再循环读到 0，最后才 `closesocket`。顺序反了会怎样？直接 `closesocket` 会隐式发起关闭序列，已经排队但没发出去的数据可能被丢弃，对端读到的是截断的响应。Microsoft 的文档明确建议不要依赖 `closesocket` 的隐式关闭，而是显式 `shutdown(SD_SEND)` 之后把剩余数据读干净。

半关闭本身的行为值得实测一遍。下面的程序让客户端发完 10 字节后 `shutdown(SD_SEND)`，客户端再试一次 `send`：

::: terminal title="PowerShell - shutdown_demo"

<span class="prompt">PS&gt;</span> gcc -std=c17 -Wall -Wextra shutdown_demo.c -o shutdown_demo.exe -lws2_32

<span class="prompt">PS&gt;</span> ./shutdown_demo.exe

```text
FIONREAD before recv: 10
shutdown(SD_SEND) = 0
send after shutdown(SD_SEND) = -1, error 10058
server recv 10 bytes: 0123456789
server recv again = 0 (0 = EOF)
client recv after reply: 5 bytes: reply
client recv again = 0 (0 = EOF)
```

:::

`shutdown(SD_SEND)` 之后发送方向关闭，再 `send` 得到 10058（`WSAESHUTDOWN`）；接收方向仍然有效，对端发来的回复照收，直到读到 0。`FIONREAD` 是 Winsock 的另一个常用 ioctl，给出接收缓冲区里可读的字节数，[A.14](/教程/正文/系统编程/拓展A_Win32系统编程/A_14_异步IO与重叠结构) 讲重叠 I/O 时会再用到它。

服务器编译运行，另开一个窗口用配套的 `echo_client.exe` 连一次。服务器的窗口先打印监听地址，等客户端连上、收发完毕之后，再补上后两行：

::: terminal title="PowerShell - echo_server"

<span class="prompt">PS&gt;</span> netstat -ano | Select-String ":23456 "

<span class="prompt">PS&gt;</span> gcc -std=c17 -Wall -Wextra echo_portable.c -o echo_server.exe -lws2_32

<span class="prompt">PS&gt;</span> .\echo_server.exe 23456

```text
listening on 127.0.0.1:23456
connection accepted
connection closed
```

:::

预设的 `netstat` 查询没有输出，说明端口空闲。客户端那一边：

::: terminal title="PowerShell - echo_client"

<span class="prompt">PS&gt;</span> gcc -std=c17 -Wall -Wextra echo_client.c -o echo_client.exe -lws2_32

<span class="prompt">PS&gt;</span> .\echo_client.exe 127.0.0.1 23456 "hello winsock" "second line"

```text
hello winsock
second line
```

:::

客户端把两行原样收回后正常退出，退出码 0。`echo_client` 发完之后 `shutdown(SD_SEND)`，服务器读到 0，走 `close_gracefully` 的收尾流程。

同一份源码在 Linux 上编译。把 `echo_portable.c` 和 `echo_client.c` 传到 Linux 主机，编译命令里没有 `-lws2_32`：

::: terminal title="zsh - echo_server"

<span class="prompt">\$</span> gcc -std=c17 -Wall -Wextra echo_portable.c -o echo_server

<span class="prompt">\$</span> gcc -std=c17 -Wall -Wextra echo_client.c -o echo_client

<span class="prompt">\$</span> ss -ltn "sport = :23456"

State Recv-Q Send-Q Local Address:Port  Peer Address:Port

<span class="prompt">\$</span> ./echo_server 23456

listening on 127.0.0.1:23456

:::

另一个终端里运行客户端：

::: terminal title="zsh - echo_client"

<span class="prompt">\$</span> ./echo_client 127.0.0.1 23456 "hello from linux" "second line"

hello from linux

second line

:::

客户端收到的两行与发送的内容一致。服务器的窗口里随后出现 `connection accepted` 和 `connection closed`，与 Windows 上的输出逐字相同。两边在 `-Wall -Wextra` 下都没有警告，同一份源码在两个系统上跑出了一样的行为。

<Quiz
  type="single"
  :options="[
    { label: 'SO_REUSEADDR，和 Linux 服务器一样', value: 'A' },
    { label: 'SO_EXCLUSIVEADDRUSE，独占端口', value: 'B' },
    { label: 'SO_LINGER，设置超时', value: 'C' },
    { label: 'SO_KEEPALIVE，保活', value: 'D' }
  ]"
  answer="B"
>

<template #question>

Windows 上的服务器为了防止端口被别的进程用 `SO_REUSEADDR` 抢绑，应该在 `bind` 之前设置哪个选项？

</template>

正确答案是 **B**。`SO_EXCLUSIVEADDRUSE` 独占端口，之后的抢绑返回 `WSAEACCES`（10013）。

- A 错：Windows 的 `SO_REUSEADDR` 语义与 BSD 相反，它允许抢绑，正是要防的东西。本页 `reuse_hijack.exe` 的第二组演示了设了独占之后抢绑失败。
- C 错：`SO_LINGER` 控制关闭时的等待行为，与端口归属无关。
- D 错：`SO_KEEPALIVE` 让内核定期探测空闲连接，与端口归属无关。

</Quiz>

## 5. select 与 WSAPoll

Winsock 有 `select`，但它的 `fd_set` 与 POSIX 的完全不同。POSIX 的 `fd_set` 是位图，`FD_SETSIZE` 是位图的位数；Winsock 的 `fd_set` 是一个结构体，里面是套接字数组和计数：

```c
typedef struct fd_set {
    u_int fd_count;
    SOCKET fd_array[FD_SETSIZE];
} fd_set;
```

`FD_SETSIZE` 默认是 64，可以在包含 `<winsock2.h>` 之前重定义。本机实测：默认时 `sizeof(fd_set)` 是 520（4 字节的计数，加 64 个 8 字节句柄，再按 8 字节对齐），重定义为 1024 之后变成 8200。这两个数字也说明了另一个事实：Winsock 的 `select` 不是位图操作，它能容纳的套接字个数受数组长度限制，但不要求套接字编号小于某个值。

`select` 的第一个参数在 Winsock 里被忽略，文档原话是「included only for compatibility with Berkeley sockets」。传 0 就行。三个 `fd_set` 里只能放套接字，放别的句柄会返回 `WSAENOTSOCK`。用一个小程序把这几点摆出来：

::: terminal title="PowerShell - select_fdset"

<span class="prompt">PS&gt;</span> gcc -std=c17 -Wall -Wextra select_fdset.c -o select_fdset.exe -lws2_32

<span class="prompt">PS&gt;</span> ./select_fdset.exe

```text
FD_SETSIZE = 64
sizeof(fd_set) = 520, sizeof(SOCKET) = 8
after FD_SET: fd_count = 1, fd_array[0] = 264
select(0, ...) = 1, FD_ISSET = 1
recv = 4: ping
select timeout = 0, fd_count after return = 0
select on a pipe handle = -1, WSAGetLastError = 10038
```

:::

`fd_array[0]` 的值每次运行都不同，这里只是说明里面存的是套接字本身。超时返回 0 时 `fd_count` 变成 0，因为返回时集合被改写成只剩就绪项，这一点与 POSIX 相同。最后一行是重点：把匿名管道的读端句柄塞进 `fd_set`，`select` 直接返回 -1、错误 10038。在 Linux 上管道可以放进 `select`，移植过来的代码如果靠这个同时等管道和套接字，在 Windows 上必须换设计，通常是把管道换成 `WSAEventSelect`（下一节）或者干脆用命名管道加重叠 I/O（[A.14](/教程/正文/系统编程/拓展A_Win32系统编程/A_14_异步IO与重叠结构)）。

新代码在 Windows 上应该用 `WSAPoll`，它与 POSIX 的 `poll` 形状一致：

```c
int WSAPoll(LPWSAPOLLFD fdArray, ULONG fds, INT timeout);
```

`WSAPOLLFD` 就是 `pollfd`，有 `fd`、`events`、`revents` 三个成员。事件标志的名字不同：读用 `POLLRDNORM`，写用 `POLLWRNORM`；`POLLIN` 是 `POLLRDNORM` 与 `POLLRDBAND`（带外数据）的组合，`POLLOUT` 与 `POLLWRNORM` 同值。返回时 `revents` 里可能出现 `POLLERR`、`POLLHUP`、`POLLNVAL`，即使没有在 `events` 里请求。本机实测的取值是 `POLLIN` 0x0300、`POLLRDNORM` 0x0100、`POLLWRNORM` 0x0010、`POLLERR` 0x0001、`POLLHUP` 0x0002、`POLLNVAL` 0x0004。

几个与 `poll` 不同、必须知道的地方：

- `POLLPRI` 不被 Microsoft 提供者支持，在 `events` 里写了它，`WSAPoll` 直接失败并返回 10022。
- `fd` 为负数或 `INVALID_SOCKET` 的项不参与等待，返回时它的 `revents` 被置为 `POLLNVAL`。本机实测：一项 `fd` 是有效套接字、一项是 `(SOCKET)-5` 时，`WSAPoll` 返回 0，第二项 `revents` 为 0x4。这一点比 `poll` 方便，数组不用压缩，删掉的连接把 `fd` 设成 `INVALID_SOCKET` 即可。但整个数组里至少要有一个有效套接字，全是无效值时函数返回 -1、错误 10038。
- 对端半关闭时报告 `POLLHUP`。如果接收缓冲区里还有没读走的数据，`POLLRDNORM` 和 `POLLHUP` 会一起置位。先看到 `POLLHUP` 就关闭套接字，缓冲区里的数据就丢了。

最后一条本机单独验证过：对端发 12 字节后 `shutdown(SD_SEND)`，本端 `WSAPoll` 返回 `revents = 0x0102`，即 `POLLRDNORM | POLLHUP`；接着 `recv` 拿到完整的 12 字节，再 `recv` 才返回 0。正确的处理顺序是：只要 `revents` 里有 `POLLRDNORM`，就先循环 `recv` 到 `WSAEWOULDBLOCK`；`recv` 返回 0 或者 `POLLHUP` 且数据已读完，才关闭。

`wsapoll_server.c` 是一个用 `WSAPoll` 单线程服务多个客户端的行回声服务器，结构照搬第 11 章的 `poll` 版本：数组第 0 项是监听套接字，后面每项一个连接，读到的整行转成大写发回，收到 `QUIT` 关闭连接。空闲 5 秒没有连接时它自己退出，方便演示：

::: terminal title="PowerShell - wsapoll_server"

<span class="prompt">PS&gt;</span> gcc -std=c17 -Wall -Wextra wsapoll_server.c -o wsapoll_server.exe -lws2_32

<span class="prompt">PS&gt;</span> .\wsapoll_server.exe 23462

```text
listening on 127.0.0.1:23462
client 1 connected
client 2 connected
connection 2 closed
connection 1 closed
idle for 5 s, exiting
```

:::

两个客户端几乎同时连上，各自发一行（`hello from alpha` 和 `hello from beta`），分别收到大写回显 `HELLO FROM ALPHA`、`HELLO FROM BETA`，服务器用一个线程把两条连接都服务完。完整程序里负责分帧的 `consume_lines` 和 `handle_line` 与第 11 章 `poll` 版本的思路一致，差别只在类型和错误码。

<Quiz
  type="single"
  :options="[
    { label: '监听套接字', value: 'A' },
    { label: '已连接的 TCP 套接字', value: 'B' },
    { label: 'UDP 套接字', value: 'C' },
    { label: '匿名管道的读端句柄', value: 'D' }
  ]"
  answer="D"
>

<template #question>

Winsock 的 `select` 不能等待下列哪一种对象？

</template>

正确答案是 **D**。Winsock 的 `select` 只接受套接字，`fd_set` 里放别的句柄会返回 `WSAENOTSOCK`（10038），本页 `select_fdset.exe` 实测就是这个结果。

- A、B、C 都是套接字：监听套接字可读表示有待接受的连接，已连接的 TCP 套接字可读表示有数据或对端关闭，UDP 套接字可读表示有数据报。
- D 错：管道是内核句柄，不是套接字。Linux 上可以把它放进 `select`，Winsock 不行。

</Quiz>

## 6. WSAEventSelect

`select` 和 `WSAPoll` 都属于就绪通知：程序主动去问「谁准备好了」。Winsock 还提供另一种做法，把套接字和一个事件对象绑定，让套接字本身变成可等待的对象，然后用 [A.10](/教程/正文/系统编程/拓展A_Win32系统编程/A_10_同步原语_等待函数与内核对象) 讲的等待函数等它。这就是 `WSAEventSelect`。

```c
WSAEVENT ev = WSACreateEvent();
WSAEventSelect(listener, ev, FD_ACCEPT | FD_CLOSE);
...
DWORD rc = WaitForMultipleObjects(2, handles, FALSE, INFINITE);
if (rc == WAIT_OBJECT_0 + 1) {
    WSANETWORKEVENTS ne;
    WSAEnumNetworkEvents(listener, ev, &ne);
    if (ne.lNetworkEvents & FD_ACCEPT) {
        /* accept 循环 */
    }
}
```

三步各有分工。`WSACreateEvent` 建一个手动重置事件对象。`WSAEventSelect` 把事件和一组网络事件关联起来，`FD_ACCEPT` 表示有待接受的连接，`FD_READ` 表示可以读，`FD_WRITE` 表示可以写，`FD_CLOSE` 表示对端关闭。等待函数返回后，`WSAEnumNetworkEvents` 把「到底发生了哪些事件、每个事件有没有出错」取出来，同时把事件重置。**不要自己调用 `ResetEvent` 重置这个事件**，`WSAEnumNetworkEvents` 会原子地完成重置和事件记录的清除。本机实测的顺序是：连接到达后 `WaitForSingleObject` 返回 0，`lNetworkEvents` 为 `FD_ACCEPT`（0x08）；调用 `WSAEnumNetworkEvents` 之后立刻再等同一个事件，返回 258（`WAIT_TIMEOUT`），说明事件确实已经复位。

网络事件的报告还带一层「使能」：`FD_ACCEPT` 在被 `accept` 调用之后才重新使能，`FD_READ` 在被 `recv` 调用之后才重新使能。所以事件触发后的处理不能只做一半，否则这个事件不会再报第二次。

这个调用还有一个副作用：`WSAEventSelect` 自动把套接字设为非阻塞，不管 `lNetworkEvents` 写了什么。本机实测：普通套接字上的 `recv` 在没有数据时一直阻塞，调用 `WSAEventSelect` 之后同一个 `recv` 立刻返回 -1、错误 10035（`WSAEWOULDBLOCK`）。所以事件触发后的读循环必须按非阻塞的方式写，读到 `WSAEWOULDBLOCK` 为止。

事件里的错误码用 `iErrorCode` 数组按 `FD_XXX_BIT` 下标取。`FD_CLOSE` 的错误码区分正常关闭和异常断开：0 表示对端优雅关闭，10053（`WSAECONNABORTED`）表示连接被中止。本机用 `SO_LINGER` 设为 0 制造复位，拿到的正是 10053。

`eventselect_server.c` 把这一套放进一个完整的服务器：监听套接字关联 `FD_ACCEPT`，每个接受的连接再各自关联一个事件，主线程等一个数组，第 0 项是停止事件，第 1 项是监听事件，后面是各个连接的事件。另起一个线程睡 3 秒后 `SetEvent` 停止事件，主循环退出：

::: terminal title="PowerShell - eventselect_server"

<span class="prompt">PS&gt;</span> gcc -std=c17 -Wall -Wextra eventselect_server.c -o eventselect_server.exe -lws2_32

<span class="prompt">PS&gt;</span> .\eventselect_server.exe 23467

```text
listening on 127.0.0.1:23467, will stop after 3 s
[accept] conn 1 connected
[conn 1] echoed 13 bytes
[conn 1] echoed 6 bytes
[close] conn 1 closed
[stopper] stop event signaled
stop event signaled, shutting down
```

:::

客户端发了两行（`event select` 加换行是 13 字节，`hello` 加换行是 6 字节），服务器每行转成大写回显，客户端断开后收到 `FD_CLOSE`。3 秒到，停止事件置位，`WaitForMultipleObjects` 返回 0，主循环退出。整个服务器只有一个线程在等服务，停止线程只负责在时间到了之后置位一个事件。这种「一个等待函数等所有东西」的结构是 Windows 上事件驱动程序的常见写法，也是 [A.14](/教程/正文/系统编程/拓展A_Win32系统编程/A_14_异步IO与重叠结构) 的重叠 I/O 和 [A.15](/教程/正文/系统编程/拓展A_Win32系统编程/A_15_IO完成端口与线程池) 的完成端口的基础。

`WSAAsyncSelect` 是同一功能的另一种形式，它把网络事件变成窗口消息投递到指定窗口，只适合有消息循环的 GUI 程序。控制台程序没有消息循环，用不了它。

::: generate wsa-io-models kind=comparison concepts=wsa-socket-handle

面向已经读过第 11 章、熟悉 `poll` 与 `epoll` 的读者，对照四个模型：`select`、`WSAPoll`、`WSAEventSelect`、I/O 完成端口。逐一说明每个模型怎么表达「谁准备好了」、能同时等多少对象、有没有每次调用都要重建的数组、等待时能不能顺便等非套接字对象，以及各自的适用规模。最后给出选择顺序：小规模多路复用用什么，需要和事件、线程一起等用什么，高并发服务器用什么。

:::

## 7. Windows 独有的扩展

Winsock 在 BSD 接口之外还有一批扩展，本页只把入口讲清楚，细节留给后面两页。

**重叠套接字。** `socket` 创建的套接字默认就带重叠属性，可以直接把 `OVERLAPPED` 结构传给 `WSARecv`、`WSASend`、`WSAIoctl`。本机实测对一个 `socket()` 返回的句柄调用 `WSARecv`，立刻返回 -1、错误 997（`WSA_IO_PENDING`），数据到达后事件置位，`WSAGetOverlappedResult` 取回结果。要让套接字不带这个属性，得用 `WSASocketW` 并且不传 `WSA_FLAG_OVERLAPPED`；实际开发中几乎总是希望它带着。[A.14](/教程/正文/系统编程/拓展A_Win32系统编程/A_14_异步IO与重叠结构) 讲重叠结构，[A.15](/教程/正文/系统编程/拓展A_Win32系统编程/A_15_IO完成端口与线程池) 讲完成端口。

**AcceptEx 一类的扩展函数。** `AcceptEx`、`ConnectEx`、`TransmitFile`、`GetAcceptExSockaddrs` 不在 `ws2_32.dll` 的常规导出表里，要先取函数指针。两种取法：直接链接 `mswsock` 导入库，或者用 `WSAIoctl` 的 `SIO_GET_EXTENSION_FUNCTION_POINTER`。后者更通用，因为不同提供者的实现可以不同：

```c
LPFN_ACCEPTEX accept_ex = NULL;
GUID guid = WSAID_ACCEPTEX;
DWORD bytes = 0;
int rc = WSAIoctl(s, SIO_GET_EXTENSION_FUNCTION_POINTER, &guid, sizeof guid,
                  &accept_ex, sizeof accept_ex, &bytes, NULL, NULL);
```

本机运行 `acceptex_ptr.c` 的结果是 `rc = 0, bytes = 8`，指针非空。用这种方法取得的函数指针不需要链接 `mswsock`；直接调用 `AcceptEx` 这个符号才需要 `-lmswsock`。[A.15](/教程/正文/系统编程/拓展A_Win32系统编程/A_15_IO完成端口与线程池) 会用它写真正的异步接受。

**AF_UNIX。** 从 Windows 10 1803 起，Winsock 支持 `AF_UNIX` 地址族，用 `<afunix.h>` 里的 `sockaddr_un`，`sun_path` 是 108 字节的 UTF-8 路径。它只支持 `SOCK_STREAM`：本机实测 `socket(AF_UNIX, SOCK_DGRAM, 0)` 失败，错误 10047（「使用了与请求的协议不兼容的地址」）。`bind` 到路径时会在文件系统里创建一个套接字文件，同一个路径第二次 `bind` 返回 10048，所以重启前要先删除它。没有 `socketpair`，不能传文件描述符，也没有 `SCM_CREDENTIALS`；要取对端进程号用 `WSAIoctl` 的 `SIO_AF_UNIX_GETPEERPID`，本机实测能取到。

`afunix_demo.c` 在一个进程里同时当服务器和客户端：绑定路径，另起线程连上来发一行，服务器转成大写发回：

::: terminal title="PowerShell - afunix_demo"

<span class="prompt">PS&gt;</span> gcc -std=c17 -Wall -Wextra afunix_demo.c -o afunix_demo.exe -lws2_32

<span class="prompt">PS&gt;</span> ./afunix_demo.exe

```text
bind path: mdr_afunix.sock (sun_path capacity 108)
listening on AF_UNIX socket
server received: hello over af_unix
client received: HELLO OVER AF_UNIX
```

:::

AF_UNIX 的意义在于它是本机 IPC 里少有的能同时用在 Windows 和 Linux 上的接口：Linux 上的 `AF_UNIX` 有更长的历史、更多的特性（数据报、传描述符、抽象地址），但流式套接字这一小块完全一致。要在两个平台上跑同一份本机通信代码，AF_UNIX 比命名管道和 Unix 域套接字的组合省事。代价是 Windows 上的实现晚、特性少，用到数据报或者传描述符时还是得分平台。

**WSADuplicateSocketW。** 一个进程可以把套接字句柄复制给另一个进程使用，做法是 `WSADuplicateSocketW` 生成一个协议信息结构，通过别的 IPC 通道传给目标进程，目标进程用 `WSASocketW` 按这个结构重建句柄。两个句柄指向同一个底层套接字，引用计数管理，最后一个关闭才真正释放。这条路径主要用于把监听套接字交给子进程做多进程服务器，日常代码很少直接用它。

## 8. 习题

题面用到的程序都在 `code/系统编程/拓展A_Win32系统编程/A_13_Winsock特有主题/` 下。

### 8.1 巩固

<Exercise id="46301" :d="2" :w="2" multi>

(1) _[1.1]_ 下面这段代码在 Windows 上编译运行，`socket` 一定失败。指出它缺了什么，并写出补上之后的最小改动。

```c
SOCKET s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
if (s < 0) {
    printf("%d\n", errno);
    return 1;
}
```

(2) _[1.2]_ 把上面失败分支里的两处错误报告改成 Winsock 的写法，打印错误码和系统给出的文字。用 `wsa_init.c` 的 `print_wsa_error` 作参考。

(3) _[2.2]_ 同一段代码里 `s < 0` 会触发哪条编译警告？换成 `s == -1` 之后又触发哪条？两种写法各自的后果是什么？

</Exercise>

<Exercise id="46302" :d="2" :w="2" multi>

(1) _[2.2]_ 把下面这段 POSIX 代码逐行改写成 Winsock，保持功能完全一致（包括非阻塞和 1 秒接收超时）：

```c
int fd = socket(AF_INET, SOCK_STREAM, 0);
int flags = fcntl(fd, F_GETFL, 0);
fcntl(fd, F_SETFL, flags | O_NONBLOCK);
struct timeval tv = {.tv_sec = 1, .tv_usec = 0};
setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof tv);
ssize_t n = read(fd, buf, sizeof buf);
if (n < 0 && errno == EAGAIN) {
    puts("no data");
}
close(fd);
```

(2) _[3.2]_ 改写后的代码在 `recv` 超时时返回什么？错误码是多少？用 `setsockopt` 设 500 毫秒超时实测一次，贴出耗时和错误码。

(3) _[3.2]_ 把「判断暂时性错误」写成 `sock_would_block`，要求在 Windows 分支识别 `WSAEWOULDBLOCK`，在 POSIX 分支识别 `EAGAIN`、`EWOULDBLOCK` 和 `EINTR`。

</Exercise>

### 8.2 编程

<Exercise id="46303" :d="3" :w="3">

写一个两平台通用的 `echo_client`，用法 `echo_client HOST PORT [消息...]`：

- 每条消息单独成行发送，全部发完后 `shutdown` 发送方向；
- 循环 `recv` 直到返回 0，把收到的字节原样打印；
- 编译命令在 Windows 上加 `-lws2_32`，Linux 上不加；
- 连接失败时打印 `WSAGetLastError` 或 `errno` 的错误码。

验收：在 Windows 上连本页的 `echo_server.exe 23456`，发送两行，输出与输入一致；在 Linux 上用同一条源码编译后连同一个服务器，输出相同。再连一个没人监听的端口，确认两边打印的错误码分别是 10061 和 `ECONNREFUSED`（111）。

</Exercise>

<Exercise id="46304" :d="4" :w="4">

写一个 `poll_echo PORT`，用 `WSAPoll` 单线程服务至少 3 个并发客户端，要求：

- 每行回显时加序号，格式为 `序号: 大写内容`；
- 单个客户端一次发送多行时，所有行都要按顺序回显；
- 客户端半关闭后，把接收缓冲区里剩下的数据全部处理完再关闭，不允许丢行；
- 连接断开时释放套接字，数组里把 `fd` 置为 `INVALID_SOCKET`，不压缩数组。

验收：写一个脚本，同时启动 3 个客户端，每个连上后发 100 行（每行内容不同），全部收到后退出。服务器必须回显 300 行，且每个客户端的行号连续、内容与发送顺序一致。再用一个只发一半就 `shutdown` 的客户端，确认服务器没有丢最后半行之外的数据。

</Exercise>

<Exercise id="46305" :d="4" :w="4">

把上面的服务器改成 `event_echo PORT SECONDS`，用 `WSAEventSelect` 而不是 `WSAPoll`：

- 监听套接字关联 `FD_ACCEPT | FD_CLOSE`，每个连接关联 `FD_READ | FD_CLOSE`；
- 主线程用 `WaitForMultipleObjects` 同时等一个停止事件和所有套接字事件；
- 收到 `FD_CLOSE` 时先读完剩余数据再关闭；
- 到达 `SECONDS` 秒后由一个辅助线程置位停止事件，服务器退出并打印处理的连接数。

验收：运行 `event_echo 23470 5`，在 5 秒内用两个客户端各连一次并发一行，服务器要把两行都回显，并在第 5 秒准时退出、打印 `connections = 2`。把读取循环里的 `WSAEnumNetworkEvents` 去掉再跑一次，说明为什么会卡死或者重复处理。

</Exercise>

<Exercise id="46306" :d="4" :w="3">

写一对 AF_UNIX 程序 `unix_server PATH` 和 `unix_client PATH`：

- 服务器 `bind` 到路径，接受一个连接，把收到的每一行转成大写发回，客户端关闭后退出并删除套接字文件；
- 客户端连接后发 3 行，读回 3 行打印；
- 两边都要处理「路径已存在」的情况：启动时如果文件存在，先删除再绑定，并说明为什么必须删。

验收：连续运行两次，第二次不能因为 10048 失败。再用 `socket(AF_UNIX, SOCK_DGRAM, 0)` 试一次，贴出 `WSAGetLastError` 的结果并解释。

</Exercise>

### 8.3 综合与挑战

<Exercise id="46307" :d="6" :w="5">

设计一个小头文件 `sockcompat.h`，把本页讲的平台差异封装起来，要求：

- 提供 `sock_t`、`sock_init()`、`sock_cleanup()`、`sock_close()`、`sock_invalid`、`SOCK_ERR`；
- 提供 `sock_error()`、`sock_would_block(e)`、`sock_set_nonblocking(s)`、`sock_set_recv_timeout(s, ms)`；
- 提供 `sock_listen(host, port)` 和 `sock_connect(host, port)`，内部处理 `SO_EXCLUSIVEADDRUSE` 与 `SO_REUSEADDR` 的差异；
- 一个头文件加一个 `.c`，不要用宏包住整个函数体以外的实现细节泄漏到调用方。

用它改写 `echo_portable.c`，要求改写后的版本里 `#ifdef _WIN32` 只出现在头文件和实现文件里，`main` 与业务逻辑完全干净。写一个 `Makefile` 或两行编译命令，在 Windows 与 Linux 上分别编译，`-Wall -Wextra` 零警告。最后写一个 `selftest` 程序，依次验证：连接被拒、端口占用、非阻塞读返回 `WOULDBLOCK`、接收超时，四项都打印 `PASS`。

</Exercise>

<Exercise id="46308" :d="6" :w="4">

本页的 `reuse_hijack.c` 在同一进程里演示了抢绑。把它扩展成两个独立进程的版本：

1. `svc.exe PORT`：绑定 `0.0.0.0` 并监听，打印每次接受的连接，运行 5 秒后退出；
2. `hijack.exe PORT`：在服务运行期间用 `SO_REUSEADDR` 抢绑 `127.0.0.1` 的同一端口，成功后也监听并打印接受的连接；
3. `probe.exe PORT`：连一次回环地址，打印是哪个进程接走了连接（靠服务端回送自己的名字）。

要求测三种组合：(a) 服务默认选项、入侵者 `SO_REUSEADDR`；(b) 服务 `SO_EXCLUSIVEADDRUSE`、入侵者 `SO_REUSEADDR`；(c) 两边都设 `SO_REUSEADDR`。记录每次的 `bind` 结果和连接归属，整理成表格。最后回答：哪一种组合下入侵者能拿到连接，为什么服务加了独占之后入侵者连 `bind` 都失败，以及 (c) 的结果说明两个都设 `SO_REUSEADDR` 时连接归属是否可预测。

</Exercise>

<Exercise id="46309" :d="7" :w="5" scope="*">

在 `echo_portable.c` 的基础上做一个「同时等套接字、管道和定时器」的进程内服务：

- 主循环要能同时处理：监听套接字上的新连接、已连接套接字上的数据、一个由 `CreatePipe` 创建的管道上的命令、以及每 500 毫秒触发一次的定时器；
- 命令通过管道发进来，支持 `stats`（打印连接数和总字节数）、`quit`（退出）；
- 定时器每次触发时打印一行统计。

提示：`WSAPoll` 和 `WSAEventSelect` 都只能等套接字，管道和定时器要用 [A.10](/教程/正文/系统编程/拓展A_Win32系统编程/A_10_同步原语_等待函数与内核对象) 的事件对象。可行的做法是用 `WSAEventSelect` 把套接字变成事件，再由一个线程读管道并 `SetEvent`，主线程统一 `WaitForMultipleObjects`。要说明为什么不能把管道句柄直接塞进 `WSAPoll`，并用实测的错误码佐证。Windows 之外的分支可以用 `poll` 同时等管道和套接字，把两个版本的行为对齐。

</Exercise>

## 9. 参考资料

- Microsoft Learn：Getting Started with Winsock，https://learn.microsoft.com/en-us/windows/win32/winsock/getting-started-with-winsock
- Microsoft Learn：Initializing Winsock，https://learn.microsoft.com/en-us/windows/win32/winsock/initializing-winsock
- Microsoft Learn：Porting Socket Applications to Winsock，https://learn.microsoft.com/en-us/windows/win32/winsock/porting-socket-applications-to-winsock
- Microsoft Learn：Socket Data Type，https://learn.microsoft.com/en-us/windows/win32/winsock/socket-data-type-2
- Microsoft Learn：Select, FD_SET, and FD_XXX Macros，https://learn.microsoft.com/en-us/windows/win32/winsock/select-and-fd---2
- Microsoft Learn：select function，https://learn.microsoft.com/en-us/windows/win32/api/winsock2/nf-winsock2-select
- Microsoft Learn：Error Codes - errno, h_errno and WSAGetLastError，https://learn.microsoft.com/en-us/windows/win32/winsock/error-codes-errno-h-errno-and-wsagetlasterror-2
- Microsoft Learn：Windows Sockets Error Codes，https://learn.microsoft.com/en-us/windows/win32/winsock/windows-sockets-error-codes-2
- Microsoft Learn：Using SO_REUSEADDR and SO_EXCLUSIVEADDRUSE，https://learn.microsoft.com/en-us/windows/win32/winsock/using-so-reuseaddr-and-so-exclusiveaddruse
- Microsoft Learn：Graceful Shutdown, Linger Options, and Socket Closure，https://learn.microsoft.com/en-us/windows/win32/winsock/graceful-shutdown-linger-options-and-socket-closure-2
- Microsoft Learn：bind function，https://learn.microsoft.com/en-us/windows/win32/api/winsock/nf-winsock-bind
- Microsoft Learn：socket function，https://learn.microsoft.com/en-us/windows/win32/api/winsock2/nf-winsock2-socket
- Microsoft Learn：WSAPoll function，https://learn.microsoft.com/en-us/windows/win32/api/winsock2/nf-winsock2-wsapoll
- Microsoft Learn：WSAEventSelect function，https://learn.microsoft.com/en-us/windows/win32/api/winsock2/nf-winsock2-wsaeventselect
- Microsoft Learn：WSAEnumNetworkEvents function，https://learn.microsoft.com/en-us/windows/win32/api/winsock2/nf-winsock2-wsaenumnetworkevents
- Microsoft Learn：AcceptEx function，https://learn.microsoft.com/en-us/windows/win32/api/mswsock/nf-mswsock-acceptex
- Microsoft Learn：WSAIoctl function，https://learn.microsoft.com/en-us/windows/win32/api/winsock2/nf-winsock2-wsaioctl
- Microsoft Learn：WSADuplicateSocketW function，https://learn.microsoft.com/en-us/windows/win32/api/winsock2/nf-winsock2-wsaduplicatesocketw
- Microsoft Command Line Blog：AF_UNIX comes to Windows，https://devblogs.microsoft.com/commandline/af_unix-comes-to-windows/
- Beej's Guide to Network Programming，https://beej.us/guide/bgnet/
- Johnson M. Hart, *Windows System Programming*, 4th ed., Addison-Wesley, 2010，第 12 章。
- Jeffrey Richter, Christophe Nasarre, *Windows via C/C++*, 5th ed., Microsoft Press, 2007。
- 本教程[第 10 章](/教程/正文/系统编程/10_套接字编程)、[第 11 章](/教程/正文/系统编程/11_非阻塞IO与IO多路复用)：套接字与多路复用的主线内容。
