# A.14 异步 I/O 与重叠结构

在并发系统架构中，阻塞式同步 I/O 往往是吞吐量与响应延迟的最大瓶颈：在 [A.12](/教程/正文/系统编程/拓展A_Win32系统编程/A_12_管道与进程间通信) 中构建的管道服务器，为了服务多个连接，不得不为每个到来的客户端独立衍生一个工作线程，而绝大多数工作线程绝大部分时间仅仅被动挂起在 `ReadFile` 上等待网络报文；而在桌面与图形交互程序中，若让主界面线程直接承担一个 200 MB 日志文件的同步检索与读取，操作系统消息循环势必卡住数秒，甚至触发“程序无响应”告警。

Windows NT 内核从诞生伊始便将这两种痛点收归于同一套底座级原语——重叠 I/O（Overlapped I/O，即内核级完成驱动的异步 I/O 机制）。应用程序在向设备发起读写请求后无需等待物理硬件或对端进程完成数据交付，而是立刻夺回 CPU 控制权继续推进计算或其他逻辑；待内核 I/O 管理器真正完成数据在内存与硬件之间的传输后，再通过事件通知、上下文轮询或异步过程调用通知程序收取最终结果。理解这一模型，关键在于剖析其与 POSIX 经典 `epoll` / `poll` 就绪通知（Reactor 模式）在数据流向与控制反转上的本质分野、洞悉 `OVERLAPPED` 结构在用户态与内核驱动之间的契约分工，以及牢固确立异步生命周期管理——即在内核解挂之前绝不提前释放相关内存缓冲区的铁律。

## 1. 完成模型与就绪模型

主线教程[第 11 章](/教程/正文/系统编程/11_非阻塞IO与IO多路复用)所深入推导的 `poll` 与 `epoll`，在架构学分类上属于典型的「就绪通知模型」（Reactor 模式）：内核多路复用器的职责仅限于监测描述符的内部状态，并在某个套接字接收队列中有数据到达时向应用程序发出通告——“此刻发起读调用不会发生无谓阻塞”；至于从协议栈将字节流真正搬运至用户态内存的 `read` 操作，仍然必须由应用程序在随后的同步系统调用中显式执行。

与之形成鲜明对比的是，Windows 重叠 I/O 则属于纯粹的「完成通知模型」（Proactor 模式）：应用程序在发起调用的最初时刻，就必须将「希望从哪一物理偏移读取多少字节、直接存入用户空间的哪个内存缓冲区」这一完整意图连同缓冲区指针全盘托付给内核。内核 I/O 管理器将该请求封装为 I/O 请求包（IRP）派发给底层驱动；一旦硬件 DMA 完成或网络协议栈拼装完毕，数据早已悄然就位在应用程序预留的内存之中。

两张时序图的对比鲜明地揭示了数据搬运时机的异同：

```mermaid
sequenceDiagram
    participant T as 线程
    participant K as 内核
    T->>K: poll / epoll_wait 等待
    K-->>T: fd 可读
    T->>K: read(fd, buf, n)
    K-->>T: 数据复制到 buf，返回字节数
    Note over T,K: 数据在第二次调用里才复制到用户缓冲区
```

```mermaid
sequenceDiagram
    participant T as 线程
    participant K as 内核
    T->>K: ReadFile(h, buf, n, &ov)
    K-->>T: 返回 FALSE，ERROR_IO_PENDING
    Note over T: 线程去处理别的连接
    K-->>T: 操作完成，数据已在 buf 里，ov 的事件有信号
    T->>K: GetOverlappedResult 取字节数
```

在就绪模型下，单次完整的数据交互通常需要两次系统上下文切换（一次等待事件通知、一次执行同步读取）；而且由于就绪通告与实际读取之间存在微小的调度间隙，原本就绪的数据仍有可能在多线程竞态下被抢先读取。完成模型则从根本上消除了这类竞态，因为数据早已由内核在后台完成锁页并精准投递到位。这种优雅换来的工程代价，在于每一个尚未完成的在途请求（in-flight request），都必须在用户空间绑定一份具有足够长生命周期的 `OVERLAPPED` 元数据结构以及专属的物理数据缓冲区，且在该请求获得终结确认前，应用程序绝不能提前覆写或释放该内存块。

::: concept sys-ov-completion-model title="就绪通知与完成通知"

`select`、`poll`、`epoll` 报告的是描述符此刻的状态，读写仍由程序发起，数据在发起读写时才复制。重叠 I/O 把操作本身交给内核，函数返回时操作可能已经开始，完成时数据已经在调用者提供的缓冲区里，程序只需取回结果。Linux 的 `io_uring` 也属于完成通知，主线第 11 章第 8 节介绍过它。

:::

这两种模型在实际架构中并非水火不容。[A.13](/教程/正文/系统编程/拓展A_Win32系统编程/A_13_Winsock特有主题) 中探讨的 `WSAEventSelect` 借助内核事件报告套接字就绪，依然沿袭了就绪通知的脉络；而本篇所推演的重叠文件与重叠套接字则全面拥抱完成通知，并直接作为后续 [A.15](/教程/正文/系统编程/拓展A_Win32系统编程/A_15_IO完成端口与线程池) 工业级 I/O 完成端口（IOCP）的支撑基石。

## 2. 重叠句柄与 OVERLAPPED

句柄支不支持重叠 I/O，在打开时决定。`CreateFileW` 的 `dwFlagsAndAttributes` 加上 `FILE_FLAG_OVERLAPPED`，这个句柄上的读写就走异步路径：

```c
HANDLE h = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, NULL,
                       OPEN_EXISTING, FILE_FLAG_OVERLAPPED, NULL);
```

管道在 `CreateNamedPipeW` 的 `dwOpenMode` 里加同一个标志，见第 5 节。`socket` 创建的套接字默认就支持重叠 I/O，不需要额外设置。

有了重叠句柄，每次读写都要传一个 `OVERLAPPED` 结构：

```c
typedef struct _OVERLAPPED {
    ULONG_PTR Internal;
    ULONG_PTR InternalHigh;
    union {
        struct {
            DWORD Offset;
            DWORD OffsetHigh;
        };
        PVOID Pointer;
    };
    HANDLE hEvent;
} OVERLAPPED;
```

| 成员 | 谁写 | 含义 |
| --- | --- | --- |
| `Internal` | 系统 | 请求的状态码，发起时是 `STATUS_PENDING` |
| `InternalHigh` | 系统 | 成功完成时传输的字节数 |
| `Offset`、`OffsetHigh` | 程序 | 从文件的哪个字节开始，合成 64 位偏移 |
| `Pointer` | 系统 | 保留，初始化时清零即可 |
| `hEvent` | 程序 | 完成时被置为有信号的手动重置事件；不需要时可以留空 |

在将 `OVERLAPPED` 结构体传递给 API 之前，必须使用 `ZeroMemory` 或结构体字面量进行全零初始化。成员 `hEvent` 既可以置空为 `NULL`，也可以赋予一个显式创建的手动重置事件对象（Manual-reset Event）。当异步操作被底层驱动接管时，系统会自动将该事件重置为无信号；一旦 I/O 操作全盘履约完毕，内核便会将此事件置为有信号状态。在工程实践中，为了确保多轮状态迭代的严密性，通常建议在将既有事件句柄绑定到下一轮重叠操作前，主动执行一次 `ResetEvent` 以扫清上下文残留。

与传统同步 I/O 相比，`Offset` 成员承载着根本性的语义飞跃：开启了异步属性的重叠文件句柄在内核层面彻底废弃了所谓“当前文件读写游标”（File Pointer）的概念。调用 `SetFilePointerEx` 所改动的内核游标，对重叠读写完全不产生任何制约；每一次异步读写的物理起始扇区，必须且只能由 `OVERLAPPED` 结构内部的 `Offset` 与 `OffsetHigh` 字段显式声明。这一特性使得单一文件句柄天然支持全并发的随机访问：多个工作线程可以同时对同一个文件句柄发起数十个读写操作，只要各自的 `OVERLAPPED` 指定了不同的逻辑偏移，各路 I/O 便能在底层驱动调度下完全并行推进。如下的简短测试直观验证了这一游标脱耦特性：

```c
pos.QuadPart = 10;
SetFilePointerEx(h, pos, NULL, FILE_BEGIN);
printf("SetFilePointerEx moved the file pointer to 10\n");

ov.hEvent = CreateEventW(NULL, TRUE, FALSE, NULL);
ov.Offset = 0;
if (!ReadFile(h, buf, 4, &n, &ov) && GetLastError() == ERROR_IO_PENDING) {
    WaitForSingleObject(ov.hEvent, INFINITE);
    GetOverlappedResult(h, &ov, &n, FALSE);
}
printf("overlapped read with Offset = 0 returned %lu bytes: %c%c%c%c\n", n,
       buf[0], buf[1], buf[2], buf[3]);
```

文件内容是 `ABCDEFGHIJKLMNOPQRSTUVWXYZ` 开头，运行结果：

::: terminal title="PowerShell - offset_demo"

<span class="prompt">PS&gt;</span> gcc -std=c17 -Wall -Wextra -municode -o offset_demo.exe offset_demo.c

<span class="prompt">PS&gt;</span> ./offset_demo.exe alphabet.txt

SetFilePointerEx moved the file pointer to 10

overlapped read with Offset = 0 returned 4 bytes: ABCD

:::

读回来的是文件开头。`SetFilePointerEx` 把游标移到了 10，重叠读仍从 `ov.Offset` 指定的 0 取数据，两处互不干扰。同一个句柄上可以同时有多个读，各自带自己的偏移。

::: concept sys-ov-lifetime title="每个未完成的操作各自一份 OVERLAPPED 和缓冲区"

一个未完成的重叠操作在完成之前，系统会一直使用传给它的 `OVERLAPPED` 结构和数据缓冲区。这两块内存必须保持有效、不能被复用，直到操作完成。因此每个同时进行的操作都要有自己的结构，不能在栈上定义一个反复使用，也不能在发起之后提前释放。结构里的 `Internal`、`InternalHigh` 由系统改写，程序不要碰；`Offset`、`OffsetHigh` 由程序填写，系统不会更新它们。

:::

事件只需要在同一个句柄上有多个操作同时进行时使用。不需要事件时，操作完成会把文件句柄本身置为有信号；第 7 节会看到，这种做法在多个操作同时进行时分不清是谁完成了。

## 3. 发起与完成

### 3.1 ReadFile 的三种结果

在一个重叠句柄上调用 `ReadFile`，返回值和错误码组合起来有三种情况：

| 返回 | `GetLastError()` | 含义 | 接下来做什么 |
| --- | --- | --- | --- |
| `TRUE` | 无意义 | 操作在函数返回之前就完成了，数据已在缓冲区 | 直接使用缓冲区 |
| `FALSE` | `ERROR_IO_PENDING`（997） | 操作已发起，尚未完成 | 等事件或 `GetOverlappedResult` |
| `FALSE` | 其他 | 调用失败，错误码说明原因 | 按错误码处理 |

在这三种分支中，返回 `FALSE` 且伴随错误码 `ERROR_IO_PENDING`（数值为 997）的分支最容易被初学者误判为失败。在 Windows I/O 子系统的语义中，997 本质上是状态码 `STATUS_PENDING` 的用户态投影，明确代表该 I/O 请求已被内核驱动成功收录并在后台调度执行，只是物理传输尚未结束。第三类分支才代表真正的调用级失败（例如传入了非法句柄、或者申请的逻辑读取偏移已彻底超出文件物理末尾并引发 `ERROR_HANDLE_EOF`，即 38 号错误）。然而在异步驱动中，即便是文件越界等错误，也可能先由 `ReadFile` 正常受理并返回 997，最终在异步完成阶段才由 `GetOverlappedResult` 正式爆出；因此，一切最终的成败判定均必须以后续的结果查询调用为准。

此外，直接返回 `TRUE` 的第一类快速路径绝对不可在代码逻辑中疏漏或略过。当内核文件系统缓存（System Cache）中已经预先命中了全部目标数据、或者对端管道早已预存了就绪字节时，`ReadFile` 往往在当前调用返回前便直接在内核态完成了全量内存拷贝。此时内核不会将该操作推入异步等待链表，关联事件（若已提供）会立即被置位，操作直接同步兑现。若应用程序粗暴地假设所有重叠调用必须返回 `ERROR_IO_PENDING`，便会导致这部分已经就位的数据被直接遗漏，从而引发业务死锁。下面的 `sync_complete.c` 演示了一个在命名管道上必然触发同步完成的场景：客户端在服务端发起读之前，便已先手将数据完整压入了管道缓冲区。

```c
ov.hEvent = CreateEventW(NULL, TRUE, FALSE, NULL);
ok = ReadFile(srv, buf, BUF_SIZE, &n, &ov);
printf("ReadFile returned %s, n = %lu, data = %.*s\n",
       ok ? "TRUE" : "FALSE", n, (int)n, buf);
```

::: terminal title="PowerShell - sync_complete"

<span class="prompt">PS&gt;</span> gcc -std=c17 -Wall -Wextra -o sync_complete.exe sync_complete.c

<span class="prompt">PS&gt;</span> ./sync_complete.exe

client connected

client wrote 12 bytes, the server has not asked for them yet

ReadFile returned TRUE, n = 12, data = already here

:::

一个只处理 `ERROR_IO_PENDING` 分支的程序，在这里会直接跳过读到的数据。

与之相对，起点在文件末尾或更后面的读不会成功，最终的错误码是 38。下面这个程序先读文件开头，再从文件末尾处发起一次读：

```c
ok = ReadFile(h, buf, sizeof buf, &n, &ov);
err = ok ? 0 : GetLastError();
printf("read at offset 0: ");
if (ok) {
    printf("TRUE, %lu bytes already in the buffer\n", n);
} else if (err == ERROR_IO_PENDING) {
    printf("FALSE, error %lu (ERROR_IO_PENDING)\n", err);
    WaitForSingleObject(ev, INFINITE);
    ok = GetOverlappedResult(h, &ov, &n, FALSE);
    ...
}

ov.Offset = size.LowPart;
ov.OffsetHigh = size.HighPart;
n = 0;
ok = ReadFile(h, buf, sizeof buf, &n, &ov);
```

::: terminal title="PowerShell - ov_basic"

<span class="prompt">PS&gt;</span> gcc -std=c17 -Wall -Wextra -municode -o ov_basic.exe ov_basic.c

<span class="prompt">PS&gt;</span> ./ov_basic.exe small.txt

file size: 26 bytes

read at offset 0: FALSE, error 997 (ERROR_IO_PENDING)

after the event: 26 bytes

read starting at offset 26 (the end of the file): FALSE, error 997

after the event: error 38 (ERROR_HANDLE_EOF)

:::

26 字节的文件，第一次读也返回 997。文件已经完整地躺在系统缓存里，读仍然要经过异步路径：重叠句柄上的操作几乎总是先返回 `ERROR_IO_PENDING`，同步完成是少数。第二行说明：起点在文件末尾或之后，`ReadFile` 仍可能先返回 997，38 要等 `GetOverlappedResult` 才报出来。起点在末尾之前、请求越过末尾时不算错误，读到多少算多少。

### 3.2 用 GetOverlappedResult 取结果

`GetOverlappedResult` 有两种用法：

```c
BOOL GetOverlappedResult(HANDLE hFile, LPOVERLAPPED lpOverlapped,
                         LPDWORD lpNumberOfBytesTransferred, BOOL bWait);
```

`bWait` 为 `TRUE` 时，函数在操作完成之前不返回，等效于先等 `hEvent` 再取结果。改用事件等待的写法是 `WaitForSingleObject(ov.hEvent, INFINITE)` 之后调用 `GetOverlappedResult(..., FALSE)`，区别在于等待可以带超时，也可以和别的对象一起用 `WaitForMultipleObjects` 等。

`bWait` 为 `FALSE` 且操作没完成时，函数返回 `FALSE`，错误码是 `ERROR_IO_INCOMPLETE`（996）。这个码可以用来轮询，也可以用来区分「已经完成」和「还在进行」。

事件只说明操作结束了，成功与否要看 `GetOverlappedResult` 的返回值和错误码。本节开头的程序正是靠它拿到 38。

### 3.3 同时进行多块读

重叠 I/O 省事的地方在这里：同一个文件分四块同时读，一个线程用一次等待就能收全部结果。`ov_chunks.c` 把文件按四等分切开，按 2、0、3、1 的顺序发起读，每块自带一个事件：

```c
static const int issue_order[CHUNKS] = {2, 0, 3, 1};
...
for (i = 0; i < CHUNKS; i++) {
    int k = issue_order[i];
    LONGLONG off = chunk * k;
    ...
    ov[k].Offset = (DWORD)off;
    ov[k].OffsetHigh = (DWORD)(off >> 32);
    ov[k].hEvent = events[k];
    if (ReadFile(h, buf[k], want, &n, &ov[k])) {
        printf("chunk %d completed inside ReadFile, %lu bytes\n", k, n);
        SetEvent(events[k]);
    } else if (GetLastError() != ERROR_IO_PENDING) {
        ...
    }
    pending++;
}

while (pending > 0) {
    wait_rc = WaitForMultipleObjects(CHUNKS, events, FALSE, INFINITE);
    i = (int)(wait_rc - WAIT_OBJECT_0);
    ok = GetOverlappedResult(h, &ov[i], &n, FALSE);
    ...
    ResetEvent(events[i]);
    pending--;
}
```

::: terminal title="PowerShell - ov_chunks"

<span class="prompt">PS&gt;</span> gcc -std=c17 -Wall -Wextra -municode -o ov_chunks.exe ov_chunks.c

<span class="prompt">PS&gt;</span> ./ov_chunks.exe chunks.dat

file size: 16777216 bytes, 4194304 bytes per chunk

issue  chunk 2: offset 8388608, 4194304 bytes

issue  chunk 0: offset 0, 4194304 bytes

issue  chunk 3: offset 12582912, 4194304 bytes

issue  chunk 1: offset 4194304, 4194304 bytes

done #1: chunk 0 (offset 0) 4194304 bytes, starts with 'A'

done #2: chunk 1 (offset 4194304) 4194304 bytes, starts with 'A'

done #3: chunk 2 (offset 8388608) 4194304 bytes, starts with 'A'

done #4: chunk 3 (offset 12582912) 4194304 bytes, starts with 'A'

:::

发起顺序是 2、0、3、1，完成顺序却是 0、1、2、3。`WaitForMultipleObjects` 返回的是「哪个事件有信号」，和发起顺序无关；这台机器上四个块都命中缓存，完成顺序由内核对请求的排队决定，多次运行可能不同，程序不应该依赖它。整个 16 MiB 文件分四块同时读完，四块缓冲区合计 16 MiB，块数再多就要把内存预算算进去。

`WaitForMultipleObjects` 单次最多等 64 个对象。要同时跟踪的操作超过这个数，靠数组加事件就撑不住了，这正是 [A.15](/教程/正文/系统编程/拓展A_Win32系统编程/A_15_IO完成端口与线程池) 的完成端口要解决的问题。

<Quiz
  :options="[
    { label: '读失败了，文件可能损坏', value: 'A' },
    { label: '读已经发起，只是还没有完成', value: 'B' },
    { label: '已经到了文件末尾', value: 'C' },
    { label: '这个句柄不支持重叠 I/O', value: 'D' }
  ]"
  answer="B"
>

<template #question>

重叠句柄上 `ReadFile` 返回 `FALSE`，`GetLastError()` 为 `ERROR_IO_PENDING`，说明什么？

</template>

正确答案是 **B**。997 表示操作已经被内核接受、正在处理，函数提前返回了。文件末尾是 38（`ERROR_HANDLE_EOF`），句柄不支持重叠会给出别的错误码。997 只表示操作尚未完成，不代表失败。

</Quiz>

## 4. 完成例程与可警报等待

事件加等待的方式要程序自己盯着事件。另一条路是把「完成后做什么」直接交给系统：`ReadFileEx` 和 `WriteFileEx` 接收一个完成例程（completion routine），操作完成时系统调用它。

```c
BOOL ReadFileEx(HANDLE hFile, LPVOID lpBuffer, DWORD nNumberOfBytesToRead,
                LPOVERLAPPED lpOverlapped,
                LPOVERLAPPED_COMPLETION_ROUTINE lpCompletionRoutine);

typedef VOID (CALLBACK *LPOVERLAPPED_COMPLETION_ROUTINE)(
    DWORD dwErrorCode, DWORD dwNumberOfBytesTransfered, LPOVERLAPPED lpOverlapped);
```

完成例程回调所提供的三项参数设计得极为克制而高效：操作最终的错误码（零代表成功）、实际传输的字节总数、以及最初发起异步请求时传入的 `OVERLAPPED` 结构体指针。这其中，`lpOverlapped` 指针构成了找回业务上下文的关键锚点。在标准 C 系统设计中，通常会将 `OVERLAPPED` 放置在一个承载连接或文件上下文的更大结构体（如 `struct connection`）的首个字段位置；回调函数仅需借助 `(struct connection *)lpOverlapped` 指针强转（或者通过 `CONTAINING_RECORD` 宏），便能在零额外哈希查找开销下瞬间找回该操作关联的全部用户数据，后续 [A.15](/教程/正文/系统编程/拓展A_Win32系统编程/A_15_IO完成端口与线程池) 涉及 IOCP 时同样会深度依赖这一优雅的侵入式数据结构手法。

在扩展 API 下，`ReadFileEx` 会完全忽略 `OVERLAPPED` 中的 `hEvent` 成员，开发者甚至可以将其挪作应用程序私有的整型标签或指针存储区。这带来了显著的资源节约：应用程序不再需要为每个在途操作预先向内核申请有限的手动重置事件对象，所有关于「由谁完成」的路由工作均由回调参数中的结构体指针直接承载。

与常规事件通知不同，完成例程在底层的分发严格依托异步过程调用（APC）机制，绝不会随意漂移至其他无关的工作线程中运行。内核会将完成回调严格追加排入最初调用 `ReadFileEx` 的那个特定线程的内核 APC 队列中；且该线程只有在主动进入所谓「可警报等待状态」（Alertable Wait State）时，内核才会顺次调度执行队列中的所有排队 APC，如 [A.10](/教程/正文/系统编程/拓展A_Win32系统编程/A_10_同步原语_等待函数与内核对象) 第 8 节所述。当任何扩展等待调用（如 `SleepEx`、`WaitForSingleObjectEx` 等并将警报开关设为 `TRUE`）因调度并执行完一批回调而解除等待时，其返回值将固定命中特定状态码 `WAIT_IO_COMPLETION`（192）。

这个差别用 `Sleep` 和 `SleepEx` 一对比就出来了：

```c
if (!ReadFileEx(h, g_buf1, BUF_SIZE, &ov1, on_read)) { ... }
printf("first read issued, now Sleep(500), a non-alertable wait\n");
Sleep(500);
printf("after Sleep: completion routines run = %d\n", g_runs);

if (!ReadFileEx(h, g_buf2, BUF_SIZE, &ov2, on_read)) { ... }
printf("second read issued, now SleepEx(500, TRUE)\n");
rc = SleepEx(500, TRUE);
```

::: terminal title="PowerShell - no_alert"

<span class="prompt">PS&gt;</span> gcc -std=c17 -Wall -Wextra -municode -o no_alert.exe no_alert.c

<span class="prompt">PS&gt;</span> ./no_alert.exe chunks.dat

first read issued, now Sleep(500), a non-alertable wait

after Sleep: completion routines run = 0

second read issued, now SleepEx(500, TRUE)

  completion routine ran: 65536 bytes, error 0

  completion routine ran: 65536 bytes, error 0

SleepEx returned 192 (WAIT_IO_COMPLETION)

after SleepEx: completion routines run = 2

:::

第一次读早就在 `Sleep` 期间完成了，完成例程却一次也没跑。线程进入可警报等待后，队列里积压的两个完成例程一起执行，等待只返回一次 `WAIT_IO_COMPLETION`。这解释了为什么用完成例程的程序主循环总是一个 `SleepEx(INFINITE, TRUE)` 或 `WaitForSingleObjectEx(..., TRUE)`。

::: warning 完成例程只跑在发起 I/O 的线程里

`ReadFileEx` 的完成例程排队在调用 `ReadFileEx` 的那个线程上。线程不进入可警报等待，完成例程就一直不执行；别的线程进入可警报等待也没用。需要跨线程交付结果时用完成端口。

:::

把读写串起来，完成例程可以组成一条流水线。`readex_copy.c` 是一个用完成例程写的文件复制程序：读完成例程发起写，写完成例程发起下一次读，主线程只负责 `SleepEx`。

```c
static void CALLBACK read_done(DWORD err, DWORD bytes, LPOVERLAPPED ov) {
    copy_state *st = &g_state;
    LONGLONG at = ((LONGLONG)ov->OffsetHigh << 32) | ov->Offset;

    st->reads++;
    if (err != 0) { ... }
    printf("read  #%d: %lu bytes at offset %lld\n", st->reads, bytes,
           (long long)at);
    if (bytes == 0) {
        st->finished = 1;
        return;
    }
    st->write_ov.Offset = ov->Offset;
    st->write_ov.OffsetHigh = ov->OffsetHigh;
    if (!WriteFileEx(st->dst, st->buf, bytes, &st->write_ov, write_done)) { ... }
}

static void CALLBACK write_done(DWORD err, DWORD bytes, LPOVERLAPPED ov) {
    copy_state *st = &g_state;
    LONGLONG at = ((LONGLONG)ov->OffsetHigh << 32) | ov->Offset;

    st->writes++;
    ...
    st->offset = at + bytes;
    issue_read(st);
}
```

::: terminal title="PowerShell - readex_copy"

<span class="prompt">PS&gt;</span> gcc -std=c17 -Wall -Wextra -municode -o readex_copy.exe readex_copy.c

<span class="prompt">PS&gt;</span> ./readex_copy.exe four.dat four_copy.dat

copying 262144 bytes

read  #1: 65536 bytes at offset 0

write #1: 65536 bytes at offset 0

read  #2: 65536 bytes at offset 65536

write #2: 65536 bytes at offset 65536

read  #3: 65536 bytes at offset 131072

write #3: 65536 bytes at offset 131072

read  #4: 65536 bytes at offset 196608

write #4: 65536 bytes at offset 196608

done: 4 reads, 4 writes

:::

::: terminal title="PowerShell - fc"

<span class="prompt">PS&gt;</span> fc.exe /b four.dat four_copy.dat

Comparing files four.dat and FOUR_COPY.DAT

FC: no differences encountered

:::

这个版本一次只有一个读和一个写在进行，吞吐和 A.1 的 `cpw` 没有区别，收益在于主线程从头到尾没有阻塞在读写上。读完成例程里再发起下一次读之前，缓冲区的数据已经由写操作取走，因此这里一个缓冲区就够；要做双缓冲流水，准备两个读缓冲、两个写缓冲，在读完成例程里交替使用即可。

<Quiz
  :options="[
    { label: 'I/O 完成时，系统立刻在任意线程里调用它', value: 'A' },
    { label: '发起 I/O 的线程进入可警报等待时', value: 'B' },
    { label: '调用 GetOverlappedResult 时', value: 'C' },
    { label: '进程退出时', value: 'D' }
  ]"
  answer="B"
>

<template #question>

`ReadFileEx` 的完成例程什么时候运行？

</template>

正确答案是 **B**。完成例程是排队在发起线程上的 APC，只有该线程调用 `SleepEx`、`WaitForSingleObjectEx` 这类函数并传入 `TRUE` 时才执行。`GetOverlappedResult` 不会触发它。

</Quiz>

## 5. 管道上的重叠 I/O

把重叠 I/O 用到命名管道上，单线程就能同时服务多个客户端。每个实例有自己的管道句柄、自己的事件、自己的状态，主线程用 `WaitForMultipleObjects` 等所有实例的事件，谁有信号就处理谁。

[A.12](/教程/正文/系统编程/拓展A_Win32系统编程/A_12_管道与进程间通信) 的服务器为每个客户端开一个线程。本节写出的 `ov_pipe_server.c` 与它不同：一个实例一个状态机，全部在一个线程里。

```mermaid
stateDiagram-v2
    [*] --> 连接中
    连接中 --> 读: 客户端连上
    读 --> 写: 读到一条消息
    写 --> 读: 写完
    读 --> 连接中: 客户端关闭或出错
    写 --> 连接中: 写出错
```

每个状态对应一次重叠操作。状态机推进的规则是固定的：发起操作后如果返回 `ERROR_IO_PENDING`，就把控制权交回主循环；如果操作同步完成，就当场处理、继续发起下一个，直到某个操作真的挂起为止。`run()` 负责后者，`resume()` 在事件有信号之后取结果、推进状态，再调用 `run()`。

```c
static void run(pipe_inst *p) {
    for (;;) {
        DWORD n = 0;
        DWORD err;
        BOOL ok;

        if (p->state == ST_DEAD) {
            return;
        }
        ResetEvent(p->ev);
        switch (p->state) {
        case ST_CONNECTING:
            ok = ConnectNamedPipe(p->h, &p->ov);
            if (ok) {
                /* 客户端在调用时已经连上：直接进入读状态 */
                p->state = ST_READING;
                continue;
            }
            err = GetLastError();
            if (err == ERROR_PIPE_CONNECTED) {
                p->state = ST_READING;
                continue;
            }
            if (err == ERROR_IO_PENDING) {
                p->pending = 1;
                return;
            }
            p->state = ST_DEAD;
            return;
        case ST_READING:
            ok = ReadFile(p->h, p->buf, BUF_SIZE, &n, &p->ov);
            if (!ok && GetLastError() == ERROR_IO_PENDING) {
                p->pending = 1;
                return;
            }
            /* 其余情况：读到数据就转去写，出错就掉线 */
            ...
        }
    }
}
```

`ConnectNamedPipe` 有一个专门的重叠语义：客户端可能在调用之前就连上了，此时函数返回 `FALSE`，错误码是 `ERROR_PIPE_CONNECTED`（535），按成功处理即可。

服务器启动后，两个客户端分别连上，各发三条消息，中间各有停顿：

::: terminal title="PowerShell - 服务器与两个客户端"

<span class="prompt">PS&gt;</span> Start-Process -FilePath .\ov_pipe_server.exe -ArgumentList 10 -NoNewWindow -RedirectStandardOutput server.log

<span class="prompt">PS&gt;</span> .\pipe_client.exe -d 500 alpha beta gamma

sent "alpha", got "alpha"

sent "beta", got "beta"

sent "gamma", got "gamma"

<span class="prompt">PS&gt;</span> .\pipe_client.exe -d 500 one two three

sent "one", got "one"

sent "two", got "two"

sent "three", got "three"

:::

服务器日志（`server.log`）：

::: terminal title="PowerShell - Get-Content server.log"

```text
serving 2 instances of the mdr_echo pipe for 10 seconds
instance 0: client connected
instance 0: read 5 bytes
instance 0: wrote 5 bytes back
instance 1: client connected
instance 1: read 3 bytes
instance 1: wrote 3 bytes back
instance 0: read 4 bytes
instance 0: wrote 4 bytes back
instance 1: read 3 bytes
instance 1: wrote 3 bytes back
instance 0: read 5 bytes
instance 0: wrote 5 bytes back
instance 0: client went away (error 109)
instance 1: read 5 bytes
instance 1: wrote 5 bytes back
instance 1: client went away (error 109)
timer expired, shutting down
instance 0: cancelled operation ended with error 995
instance 1: cancelled operation ended with error 995
server stopped
```

:::

两个实例的读写交替出现在日志里，说明一个线程确实在交替推进两个客户端。客户端关闭后读失败、错误 109（`ERROR_BROKEN_PIPE`），这是 A.12 讲过的管道结束条件。最后两行来自第 6 节的取消逻辑，服务器退出前要先把还挂着的读取消掉。

上面用的 `pipe_client.c` 是为了演示交错而写的；A.12 的 `pipe_echo_client.c` 不用改动也能连上这个服务器，同样收发正常。

每个实例只有一个事件，因为同一时刻每个实例只允许有一个操作在进行。这是有意的：同一个实例上如果同时有连接、读、写三个操作，它们共用一个事件，事件有信号时无法判断是哪一个完成了。Microsoft 的示例程序也是这么做的，注释里写得很直白。要是确实想让一个实例并行做多个操作，就把事件数量加到与并发操作数相同，让每个 `OVERLAPPED` 用自己的事件。

实例数一多，这种写法就到了尽头：`WaitForMultipleObjects` 最多等 64 个对象，事件到 63 个以后再加实例就得换批次等待；每完成一个操作还要遍历一次数组判断是谁。A.15 的 I/O 完成端口用一个队列替代整组事件，这条上限就不存在了。

<Quiz
  :options="[
    { label: '调用失败，客户端没有连上', value: 'A' },
    { label: '客户端在调用之前已经连上，按成功处理', value: 'B' },
    { label: '管道已经断开，需要重新创建', value: 'C' },
    { label: '需要先调用 DisconnectNamedPipe', value: 'D' }
  ]"
  answer="B"
>

<template #question>

重叠的 `ConnectNamedPipe` 返回 `FALSE`，错误码为 `ERROR_PIPE_CONNECTED`（535），应该怎么处理？

</template>

正确答案是 **B**。这个错误码表示客户端在 `ConnectNamedPipe` 被调用之前就已经连上了，操作其实已经完成，直接把状态推进到读即可。把它当成失败丢掉，客户端会一直等不到回应。

</Quiz>

## 6. 异步操作的取消与超时边界

在 Win32 异步设计中，重叠 I/O 发起接口自身并不提供显式的毫秒级超时形参。要在应用层建立「等待某操作至多持续指定时长、超时则安全回滚」的契约，必须将事件定时等待与 I/O 取消机制（Cancellation）结合运用：首先借助可定时的等待函数监听关联的完成事件；一旦等待因耗尽时限而返回超时，再调用 `CancelIoEx` 向底层发起取消请求。

```c
BOOL CancelIoEx(HANDLE hFile, LPOVERLAPPED lpOverlapped);
```

若向参数 `lpOverlapped` 传入 `NULL`，内核将尝试中止当前进程在该句柄上挂载的全部在途异步请求；若显式指向某特定 `OVERLAPPED` 实例，则仅精准取消这单一操作。与早年仅支持取消调用线程自身 I/O 的旧式 `CancelIo` 相比，`CancelIoEx` 能够跨越线程边界取消由其他工作线程派发的在途操作，具备更高的普适性。

需要极其警惕的是：I/O 取消操作本身依然是异步进行的。调用 `CancelIoEx` 返回成功，仅仅代表取消请求已被内核成功受理并置入处理队列；硬件驱动在完全中断并收缩在途 DMA 传输时仍需耗费微小的物理时间。直到该操作真正完成退出，并通过 `GetOverlappedResult` 明确返回 `FALSE` 且错误码确认为 `ERROR_OPERATION_ABORTED`（995）之前，与之绑定的 `OVERLAPPED` 结构与物理内存缓冲区依然受到驱动锁页保护，绝不能提前释放或覆写。

`cancel_demo.c` 在一个没有客户端连接的管道上发起重叠的 `ConnectNamedPipe`，等 300 毫秒后取消：

```c
if (ConnectNamedPipe(h, &ov)) {
    printf("ConnectNamedPipe returned TRUE\n");
} else {
    printf("ConnectNamedPipe returned FALSE, error %lu\n", GetLastError());
}

rc = WaitForSingleObject(ev, 300);
if (rc != WAIT_TIMEOUT) { ... }
printf("waited 300 ms: WAIT_TIMEOUT, no client\n");

if (!CancelIoEx(h, &ov)) {
    printf("CancelIoEx failed: %lu\n", GetLastError());
} else {
    printf("CancelIoEx succeeded\n");
}
if (GetOverlappedResult(h, &ov, &n, TRUE)) {
    printf("GetOverlappedResult returned TRUE, %lu bytes\n", n);
} else {
    printf("GetOverlappedResult returned FALSE, error %lu "
           "(ERROR_OPERATION_ABORTED)\n",
           GetLastError());
}
```

::: terminal title="PowerShell - cancel_demo"

<span class="prompt">PS&gt;</span> gcc -std=c17 -Wall -Wextra -o cancel_demo.exe cancel_demo.c

<span class="prompt">PS&gt;</span> ./cancel_demo.exe

ConnectNamedPipe returned FALSE, error 997

waited 300 ms: WAIT_TIMEOUT, no client

CancelIoEx succeeded

GetOverlappedResult returned FALSE, error 995 (ERROR_OPERATION_ABORTED)

:::

`GetOverlappedResult` 的 `bWait` 传 `TRUE`，是因为取消请求发出之后操作可能还没结束，这里需要等它收尾。超时和取消合起来就是一个「带超时的重叠操作」：等待超时之后调用取消，再等操作结束。第 8 节的习题会把这个过程包成一个函数。

如果操作在取消之前就完成了，`CancelIoEx` 找不到要取消的请求，返回 `FALSE`、错误码 `ERROR_NOT_FOUND`（1168）。这个返回值表示操作在取消之前已经完成，随后的 `GetOverlappedResult` 会报告真实结果。

同步 I/O 也能取消，用的是 `CancelSynchronousIo(线程句柄)`，它作用于另一个线程里阻塞在同步读写上的操作。这个接口在关闭服务器时把卡在 `ReadFile` 上的工作线程叫醒时有用，A.12 的线程版服务器就可以这样收尾。

## 7. 写错了会怎样

重叠 I/O 的错误几乎都围绕生存期。前两条最典型。

::: compare left-title="在函数里用局部 OVERLAPPED 和缓冲区" right-title="放进生存期足够长的结构体"

```c
static void issue_read_bad(void) {
    char buf[256];
    OVERLAPPED ov = {0};

    ReadFile(g_pipe, buf, sizeof buf, NULL, &ov);
    /* 函数返回，buf 和 ov 都失效，
       但读还在进行 */
}
```

---

```c
struct conn {
    OVERLAPPED ov;
    char buf[256];
    int state;
};

static void issue_read(struct conn *c) {
    ReadFile(g_pipe, c->buf, sizeof c->buf, NULL, &c->ov);
    /* c 在操作完成前一直有效 */
}
```

:::

左边这种写法编译器不会报错，运行时也不一定立刻出问题。`stale_buffer.c` 把两次调用安排成相同的栈帧形状：第一次发起读后返回，第二次调用在同一个位置分配同样大小的缓冲区并填满 `'L'`，然后等读完成。

::: terminal title="PowerShell - stale_buffer"

<span class="prompt">PS&gt;</span> gcc -std=c17 -Wall -Wextra -O0 -o stale_buffer.exe stale_buffer.c

<span class="prompt">PS&gt;</span> ./stale_buffer.exe

main: waiting for the client to connect

main: client connected

issue_read_bad: read issued into buf at 0000008329BFFC70, function returns

live_frame: buf at 0000008329BFFC60, filled with 'L'

client: wrote 17 bytes

live_frame: after the read completed, buf is: LLLLLLLLLLLLLLLLclient s

:::

两次打印的地址每次运行都不同，这里贴的是其中一次。

`live_frame` 的缓冲区被写进了 `client s`：读完成时系统把数据写进了一块属于别人的栈内存。真实程序里这块内存可能已经被另一个函数、另一个线程使用，后果从数据错乱到崩溃都有可能，而错误现场离引发问题的那行代码很远。用 `/GS` 或栈保护编译只能提高撞见的概率，根治办法是让 `OVERLAPPED` 和缓冲区跟着连接对象走。

注意地址只差 16 字节：`issue_read_bad` 返回之后，`live_frame` 的帧几乎正好覆盖了它。这正是这类错误难以复现的原因：换个编译器、换个优化级别，覆盖关系就变了。

第二条错误是把同一个 `OVERLAPPED` 同时用于两个操作。`reuse_ov.c` 在偏移 0 和 81920 处连续发起两个读，两次都用同一个 `ov`：

::: terminal title="PowerShell - reuse_ov"

<span class="prompt">PS&gt;</span> gcc -std=c17 -Wall -Wextra -municode -o reuse_ov.exe reuse_ov.c

<span class="prompt">PS&gt;</span> ./reuse_ov.exe chunks.dat

first  read at offset 0: FALSE, error 997

second read at offset 81920, same OVERLAPPED: FALSE, error 997

GetOverlappedResult: TRUE, 65536 bytes at offset 81920

:::

两个读都挂起了，可结构只有一个。事件有信号时，`GetOverlappedResult` 报告的是后一次操作的结果：偏移 81920、65536 字节。第一次读的完成信息被第二次覆盖，程序完全不知道第一部分读完没有，就把它当成完成了。要同时读两块，就必须用两个 `OVERLAPPED`，这是第 3 节 `ov_chunks.c` 每个块各自一个结构的原因。

第三条错误在等待的对象上。不用事件时，操作完成会把文件句柄置为有信号，于是「等文件句柄」看起来可行。`wait_handle.c` 在一个管道上同时发起两个读，都不带事件，然后等句柄本身：

::: terminal title="PowerShell - wait_handle"

<span class="prompt">PS&gt;</span> gcc -std=c17 -Wall -Wextra -o wait_handle.exe wait_handle.c

<span class="prompt">PS&gt;</span> ./wait_handle.exe

server: client connected

server: two reads in flight, neither OVERLAPPED has an event

writer: wrote "first", keeps the pipe open

server: waiting on the pipe handle returned 0

server: ov1 done

server: ov2 not done (error 996)

writer: wrote "second"

writer: closed the pipe

server: client thread finished

server: ov2 completed with 6 bytes: second

server: done

:::

第一次等待返回时只知道「某个读完成了」，不知道是哪个；`ov1` 完成了，`ov2` 仍然返回 996（`ERROR_IO_INCOMPLETE`）。等第二次数据到达，`ov2` 才完成。如果程序在第一次等待之后就去处理 `ov2` 的数据，读到的就是尚未填入的缓冲区。文件和管道句柄在有信号和无信号之间来回切换，两个操作共用一个信号源，信息量不足以区分它们。多个操作同时进行时，等各自的事件，或者用完成端口。

前两条错误都不会被编译器和运行时拦住。能让它早点暴露的办法只有两个：把 `OVERLAPPED` 和缓冲区放进随连接一起分配的结构体（第 5 节的做法），以及用 AddressSanitizer 之类的工具在调试构建里检查堆上的越界与释放后使用（见[编译器诊断与程序检查](/教程/正文/开发与工程基础/13_编译器诊断与程序检查)）。

<Quiz
  :options="[
    { label: '没有影响，栈帧不会被复用', value: 'A' },
    { label: '操作在完成时写入已经失效的栈内存，造成数据错乱或崩溃', value: 'B' },
    { label: '系统会自动取消这个操作', value: 'C' },
    { label: '编译时报错', value: 'D' }
  ]"
  answer="B"
>

<template #question>

在一个函数里用局部 `OVERLAPPED` 发起重叠读，函数在操作完成前返回，会怎样？

</template>

正确答案是 **B**。系统在操作完成时要往 `OVERLAPPED` 里写状态，往缓冲区里写数据；栈帧早已被后来的调用覆盖，写进去的是别人的内存。编译器不会为这种情况报错。

</Quiz>

## 8. 习题

### 8.1 巩固

<Exercise id="44401" :d="2" :w="2" multi>

`ov_basic.c` 里有两处对 `ERROR_IO_PENDING` 的处理，`sync_complete.c` 里则完全没有走到这个分支。

(1) _[1.1]_ 把 `sync_complete.c` 改成服务器先发起读、客户端后写，重新运行，写出现在会打印哪些行，并解释 `ReadFile` 的返回值为什么变了。

(2) _[2.2]_ `ov_basic.c` 第二次读从文件末尾开始，`ReadFile` 先返回 997，之后才报 38。把 `ov.Offset` 改成文件大小减 1，重新运行，预测并验证输出，说明「起点在末尾之前、请求越过末尾」与「起点在末尾之后」的差别。

(3) _[2.1]_ 用两三句话说明：主线第 11 章的 `epoll` 循环里，「判断哪个连接可读」和「把数据读进缓冲区」分别发生在什么时候；换成重叠 I/O 之后，哪一步不再由程序做。不需要写代码。

</Exercise>

<Exercise id="44402" :d="2" :w="1">

不看正文，写出 `OVERLAPPED` 里哪些成员由程序填写、哪些由系统改写，并说明 `Offset` 与 `SetFilePointerEx` 的关系。然后用 `offset_demo.c` 验证你的说法：把 `SetFilePointerEx` 的偏移改成 10，把 `ov.Offset` 改成 5，预测读回来的是哪四个字节。

</Exercise>

<Exercise id="44403" :d="3" :w="3">

写一个 `ov_grep.exe <file> <pattern>`：把文件分成 8 块，每块用一次重叠读同时发起，等全部完成后在各块内查找 `pattern`（按字节匹配，不要求处理跨块的情况），最后打印每块的匹配次数与总数。

验收：

- 自己造一个至少 8 MiB 的文件，在其中写入已知次数的目标串（用程序写，不要用现成的日志文件碰运气）；
- 输出形如 `chunk 3: 2 matches`，最后一行 `total: 17 matches`；
- 程序用一次 `WaitForMultipleObjects` 收全部完成，不能一个块一个块地等；
- `-Wall -Wextra` 无警告。

</Exercise>

### 8.2 编程

<Exercise id="44404" :d="3" :w="3">

把 `readex_copy.c` 改成双缓冲：两个读缓冲区、两个写缓冲区，读完成例程把数据交给空闲的写缓冲，写完成例程把缓冲标记为空闲并立刻发起下一次读。要求任意时刻最多有一个读和一个写在途，但读与写可以重叠进行。

验收：复制一个 64 MiB 的文件，`fc /b` 与原文件一致；在输出里能看到 `read #n` 与 `write #n-1` 交错出现，说明写还没有完成时下一次读已经发出。把缓冲区改回一个，重跑并比较读写的交错情况。

</Exercise>

<Exercise id="44405" :d="3" :w="3">

把 `ov_pipe_server.c` 扩展到 4 个实例，并加一条规则：客户端发来的消息若以 `quit` 开头，服务器回一条 `bye` 后主动断开这个客户端，重新进入连接状态。

验收：

- 同时启动 3 个 `pipe_client.exe`，每个发若干条消息，互不阻塞；
- 其中一个客户端发 `quit`，收到 `bye` 后连接结束；
- 服务器日志里能看到 4 个实例的编号和状态变化；
- 实例数组、事件数组、`WaitForMultipleObjects` 的对象数都由同一个常量派生出，改动实例数只改这一处。

</Exercise>

<Exercise id="44406" :d="4" :w="3">

写一个函数 `int ov_wait(HANDLE h, OVERLAPPED *ov, DWORD ms)`：等待 `ov->hEvent` 最多 `ms` 毫秒，超时则用 `CancelIoEx` 取消并等操作结束。返回值约定：1 表示操作完成（再用 `GetOverlappedResult` 判成败），0 表示超时并已取消，-1 表示等待本身出错。

用它改写 `cancel_demo.c`，然后构造三种情况逐一验证：

- 没有客户端连接，超时后取消，得到 995；
- 客户端在第 100 毫秒连上，函数返回 1，连接成功；
- 客户端在调用之前就连上，`ConnectNamedPipe` 当场返回 535，函数应当识别出这种情况并返回 1。

提示：第三种情况下 `hEvent` 没有被系统置位，直接等 300 毫秒会白等一场。

</Exercise>

<Exercise id="44407" :d="5" :w="4">

下面这个函数试图在主线程里读一个文件的两块，然后分别处理。它有三处与重叠 I/O 有关的缺陷：

```c
static HANDLE g_h;
static OVERLAPPED g_ov[2];

void read_two(void) {
    char *buf0 = malloc(65536);
    char *buf1 = malloc(65536);
    DWORD n;
    int i;

    for (i = 0; i < 2; i++) {
        g_ov[i].Offset = (DWORD)(i * 65536);
        g_ov[i].hEvent = NULL;
        ReadFile(g_h, i == 0 ? buf0 : buf1, 65536, &n, &g_ov[0]);
    }
    WaitForSingleObject(g_h, INFINITE);
    printf("%lu %lu\n", n, n);
    free(buf0);
    free(buf1);
}
```

(1) _[4.3]_ 指出三处缺陷，说明每一处会导致什么现象。

(2) _[4.3]_ 写出修正后的版本：让两块各自有独立的 `OVERLAPPED`、各自有事件，两个缓冲区活到全部完成之后，等待用 `WaitForMultipleObjects`，并且正确处理同步完成的分支。

(3) _[5.4]_ 用管道而不是文件搭一个能实际复现其中「缓冲区提前释放」的测试程序：读完成前把缓冲区释放并把指针置空，观察程序的输出或崩溃情况（`-O0` 与 `-O2` 各跑一次，说明结果为什么不同）。

</Exercise>

### 8.3 综合与挑战

<Exercise id="44408" :d="6" :w="5">

写一个 `ov_copy_many.exe <dest_dir> <file>...`：命令行给出若干源文件，程序用单线程同时复制所有文件，每个文件一个状态机（发起读、读完成后发起写、写完成后推进偏移），所有文件的事件放进同一个 `WaitForMultipleObjects` 数组，结束时打印文件数、总字节数，以及每个文件完成时相对开始的毫秒数。

验收：

- 至少 4 个源文件，其中两个大于 8 MiB，`fc /b` 全部一致；
- 输出里能看到不同文件的读写在时间上交错；
- 用 `QueryPerformanceCounter` 计时，说明总耗时明显小于逐个同步复制的时间；
- 源文件数超过 63 时给出明确提示（`WaitForMultipleObjects` 的上限），而不是静默出错。

</Exercise>

<Exercise id="44409" :d="7" :w="5" scope="*">

同一份 256 MiB 文件，用四种方式完整读一遍并计算校验和：同步 `ReadFile` 循环、重叠分块（16 块并发，事件等待）、`ReadFileEx` 完成例程链式读取、以及 A.6 的内存映射逐块遍历。每种方式跑 5 次，报告平均耗时与四次校验和是否一致，并回答：

- 四种方式在这台机器上的差距来自哪里？缓存命中与磁盘读取对结果的影响有多大（把文件换成一个刚生成、尚未进入缓存的大文件再测一次）？
- 完成例程链式读取只有一次读在途，为什么仍然可能比同步循环快或慢？
- 内存映射方式在什么情况下反而不合适？

注意：测量期间不要运行其他重负载程序；每个程序读同一个文件时用相同的块大小（建议 64 KiB 或 1 MiB），否则比较没有意义。

</Exercise>

## 9. 参考资料

- Microsoft Learn：Synchronous and Asynchronous I/O，https://learn.microsoft.com/en-us/windows/win32/fileio/synchronous-and-asynchronous-i-o
- Microsoft Learn：Synchronization and Overlapped Input and Output，https://learn.microsoft.com/en-us/windows/win32/sync/synchronization-and-overlapped-input-and-output
- Microsoft Learn：OVERLAPPED structure，https://learn.microsoft.com/en-us/windows/win32/api/minwinbase/ns-minwinbase-overlapped
- Microsoft Learn：ReadFile function，https://learn.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-readfile
- Microsoft Learn：ReadFileEx function，https://learn.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-readfileex
- Microsoft Learn：GetOverlappedResult function，https://learn.microsoft.com/en-us/windows/win32/api/ioapiset/nf-ioapiset-getoverlappedresult
- Microsoft Learn：Testing for the End of a File，https://learn.microsoft.com/en-us/windows/win32/fileio/testing-for-the-end-of-a-file
- Microsoft Learn：Alertable I/O，https://learn.microsoft.com/en-us/windows/win32/fileio/alertable-i-o
- Microsoft Learn：Named Pipe Server Using Overlapped I/O，https://learn.microsoft.com/en-us/windows/win32/ipc/named-pipe-server-using-overlapped-i-o
- Microsoft Learn：Canceling Pending I/O Operations，https://learn.microsoft.com/en-us/windows/win32/fileio/canceling-pending-i-o-operations
- Microsoft Learn：CancelIoEx function，https://learn.microsoft.com/en-us/windows/win32/fileio/cancelioex-func
- Johnson M. Hart, *Windows System Programming*, 4th ed., Addison-Wesley, 2010，第 14 章。
- Jeffrey Richter, Christophe Nasarre, *Windows via C/C++*, 5th ed., Microsoft Press, 2007，第 10 章。
- Pavel Yosifovich, *Windows 10 System Programming, Part 1*，第 11 章，https://leanpub.com/windows10systemprogramming
