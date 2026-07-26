# 源码阅读：Redis ae 事件循环

ae 是 Redis 内置的事件驱动库。整个实现只有约 500 行 C 代码（加上平台适配层也不超过 700 行），却撑起了 Redis 服务器的全部 I/O 和定时任务调度。作为对比，Node.js 底层的 libuv 为了实现同样的核心功能写了约 30,000 行。ae 的价值不在于功能多，而在于它用最少的代码把事件循环的本质讲清楚了。

> 本文基于 Redis 最新版本的 `src/ae.h`、`src/ae.c`、`src/ae_epoll.c`。许可证为 RSALv2 / SSPLv1 / AGPLv3 三选一。

## 1. 核心数据结构

ae 的全部状态集中在一个 `aeEventLoop` 结构体里：

```c
typedef struct aeEventLoop {
    int maxfd;                   /* 当前注册的最大文件描述符 */
    int setsize;                 /* 能跟踪的 fd 上限 */
    long long timeEventNextId;   /* 下一个时间事件的 ID */
    aeFileEvent *events;         /* 按 fd 索引的文件事件数组 */
    aeFiredEvent *fired;         /* 本轮 poll 返回的就绪事件 */
    aeTimeEvent *timeEventHead;  /* 时间事件链表头 */
    int stop;                    /* 非零则退出主循环 */
    void *apidata;               /* 平台 I/O 多路复用的私有数据 */
    aeBeforeSleepProc *beforesleep;
    aeBeforeSleepProc *aftersleep;
    int flags;
} aeEventLoop;
```

这个结构体管理两类事件：文件事件和时间事件。

::: concept ae-file-event title="文件事件：fd + mask + 回调"

文件事件用 `aeFileEvent` 表示，每个文件描述符对应数组中的一个槽位：

```c
typedef struct aeFileEvent {
    int mask;              /* AE_READABLE | AE_WRITABLE | AE_BARRIER */
    aeFileProc *rfileProc; /* 可读时的回调 */
    aeFileProc *wfileProc; /* 可写时的回调 */
    void *clientData;      /* 传给回调的用户数据 */
} aeFileEvent;
```

`events` 数组以 fd 为下标直接索引，`events[fd]` 就是该 fd 的事件信息。这避免了哈希表查找，O(1) 访问。代价是数组大小取决于 `setsize`（Redis 默认 `maxclients + 128`）。

:::

::: concept ae-time-event title="时间事件：链表 + 单调时钟"

时间事件用双向链表串联，每个节点记录触发时刻和回调：

```c
typedef struct aeTimeEvent {
    long long id;
    monotime when;                  /* 触发时刻（微秒级单调时钟） */
    aeTimeProc *timeProc;           /* 回调函数 */
    aeEventFinalizerProc *finalizerProc;
    void *clientData;
    struct aeTimeEvent *prev;
    struct aeTimeEvent *next;
    int refcount;                   /* 防止递归调用时提前释放 */
} aeTimeEvent;
```

Redis 的时间事件极少（通常只有 `serverCron` 一个），所以链表的 O(N) 遍历完全不是问题。源码注释里也提到了用跳表优化的可能，但明确说"Redis 目前不需要"。

:::

::: generate ae-data-structures-overview kind=explanation concepts=ae-file-event,ae-time-event

面向有一定 C 基础的读者，解释 ae 为什么选择"数组按 fd 索引"而不是哈希表来存放文件事件（fd 本身就是连续小整数），以及为什么时间事件用无序链表而不是最小堆（Redis 的时间事件数量极少，通常只有 serverCron 一个）。点明这是"按实际负载选数据结构"的典型例子。

:::

## 2. 主循环：aeMain 与 aeProcessEvents

整个事件循环的入口只有三行：

```c
void aeMain(aeEventLoop *eventLoop) {
    eventLoop->stop = 0;
    while (!eventLoop->stop) {
        aeProcessEvents(eventLoop, AE_ALL_EVENTS|
                                   AE_CALL_BEFORE_SLEEP|
                                   AE_CALL_AFTER_SLEEP);
    }
}
```

所有复杂度都在 `aeProcessEvents` 里。这个函数每次迭代做四件事：

::: concept ae-process-events title="aeProcessEvents 的四步流程"

1. **调用 beforesleep 钩子** -- Redis 在这里做连接应答、刷写 AOF 缓冲区等工作。
2. **计算 poll 超时** -- 遍历时间事件链表，找到最近一个定时器的剩余时间，作为 `aeApiPoll` 的超时参数。没有定时器则无限等待；设了 `AE_DONT_WAIT` 则超时为 0（立即返回）。
3. **调用 aeApiPoll 阻塞等待** -- 底层调用 `epoll_wait` / `kevent` / `select`，返回就绪的文件描述符。
4. **分发事件** -- 先遍历就绪的文件事件并调用回调，再处理到期的时间事件。

:::

超时计算的核心是 `usUntilEarliestTimer`：

```c
static int64_t usUntilEarliestTimer(aeEventLoop *eventLoop) {
    aeTimeEvent *te = eventLoop->timeEventHead;
    if (te == NULL) return -1;

    aeTimeEvent *earliest = NULL;
    while (te) {
        if ((!earliest || te->when < earliest->when) && te->id != AE_DELETED_EVENT_ID)
            earliest = te;
        te = te->next;
    }
    if (earliest == NULL) return -1;

    monotime now = getMonotonicUs();
    return (now >= earliest->when) ? 0 : earliest->when - now;
}
```

这里用单调时钟（`getMonotonicUs`）而不是 `gettimeofday`，避免系统时间被调整（NTP 跳变等）导致定时器错乱。

::: generate ae-timeout-calculation kind=example concepts=ae-process-events

面向读者，用一个具体的场景走一遍 aeProcessEvents 的超时计算过程：假设当前有两个时间事件，分别在 50ms 和 200ms 后触发，同时有若干文件事件在监听。描述 aeProcessEvents 如何选择 50ms 作为 poll 超时，epoll_wait 如何在这 50ms 内既能被文件事件唤醒、又能在超时后让时间事件得到处理。

:::

## 3. 文件事件的注册与分发

### 3.1 注册

`aeCreateFileEvent` 把一个 fd 的读或写回调注册到事件循环中：

```c
int aeCreateFileEvent(aeEventLoop *eventLoop, int fd, int mask,
        aeFileProc *proc, void *clientData)
{
    if (fd >= eventLoop->setsize) {
        errno = ERANGE;
        return AE_ERR;
    }

    aeFileEvent *fe = &eventLoop->events[fd];

    if (aeApiAddEvent(eventLoop, fd, mask) == -1)
        return AE_ERR;
    fe->mask |= mask;
    if (mask & AE_READABLE) fe->rfileProc = proc;
    if (mask & AE_WRITABLE) fe->wfileProc = proc;
    fe->clientData = clientData;
    if (fd > eventLoop->maxfd)
        eventLoop->maxfd = fd;
    return AE_OK;
}
```

注意 `fe->mask |= mask` 是按位或，一个 fd 可以同时监听读和写，各自有独立的回调函数。`aeApiAddEvent` 在底层调用 `epoll_ctl(EPOLL_CTL_ADD)` 或 `EPOLL_CTL_MOD`（如果该 fd 已经注册过）。

### 3.2 分发与 AE_BARRIER

文件事件的分发在 `aeProcessEvents` 的循环里：

```c
for (j = 0; j < numevents; j++) {
    int fd = eventLoop->fired[j].fd;
    aeFileEvent *fe = &eventLoop->events[fd];
    int mask = eventLoop->fired[j].mask;

    int invert = fe->mask & AE_BARRIER;

    /* 正常顺序：先读后写 */
    if (!invert && fe->mask & mask & AE_READABLE) {
        fe->rfileProc(eventLoop, fd, fe->clientData, mask);
    }
    if (fe->mask & mask & AE_WRITABLE) {
        if (!fired || fe->wfileProc != fe->rfileProc)
            fe->wfileProc(eventLoop, fd, fe->clientData, mask);
    }
    /* AE_BARRIER：先写后读 */
    if (invert && fe->mask & mask & AE_READABLE) {
        fe->rfileProc(eventLoop, fd, fe->clientData, mask);
    }
}
```

默认顺序是先调用读回调、再调用写回调——这样可以在读取客户端命令后立即发送回复。但 `AE_BARRIER` 标志会反转顺序：先写后读。Redis 在需要先把数据 `fsync` 到磁盘再回复客户端时使用这个机制。

还有一个细节：`fe->wfileProc != fe->rfileProc` 这个检查防止同一个回调被调用两次（当读写使用同一个函数时）。

## 4. 时间事件的管理

### 4.1 创建

```c
long long aeCreateTimeEvent(aeEventLoop *eventLoop, long long milliseconds,
        aeTimeProc *proc, void *clientData,
        aeEventFinalizerProc *finalizerProc)
{
    long long id = eventLoop->timeEventNextId++;
    aeTimeEvent *te = zmalloc(sizeof(*te));

    te->id = id;
    te->when = getMonotonicUs() + milliseconds * 1000;
    te->timeProc = proc;
    te->finalizerProc = finalizerProc;
    te->clientData = clientData;
    te->prev = NULL;
    te->next = eventLoop->timeEventHead;
    te->refcount = 0;
    if (te->next)
        te->next->prev = te;
    eventLoop->timeEventHead = te;
    return id;
}
```

新事件插入链表头部，O(1)。注意 `milliseconds` 是相对时间（"从现在起多少毫秒后"），内部转换为绝对的单调时钟时间戳。

### 4.2 处理与重调度

`processTimeEvents` 遍历链表，执行所有到期的事件：

```c
if (te->when <= now) {
    int retval;
    te->refcount++;
    retval = te->timeProc(eventLoop, id, te->clientData);
    te->refcount--;

    if (retval != AE_NOMORE) {
        te->when = now + (monotime)retval * 1000;  /* 重调度 */
    } else {
        te->id = AE_DELETED_EVENT_ID;               /* 标记删除 */
    }
}
```

回调的返回值决定了事件的命运：返回一个正整数表示"过多少毫秒后再次触发"（周期性事件），返回 `AE_NOMORE`（-1）表示"执行完就删除"（一次性事件）。Redis 的 `serverCron` 返回 `1000/server.hz`，实现每秒 10 次（默认）的周期调度。

删除用的是惰性策略：先把 `id` 设为 `AE_DELETED_EVENT_ID`，下一轮遍历时才真正释放内存并从链表中摘除。`refcount` 机制防止在递归调用时释放正在使用的节点。

## 5. 平台抽象层

ae 最精巧的设计之一是 I/O 多路复用的平台适配。`ae.c` 顶部用编译期 `#ifdef` 选择最优的实现：

```c
#ifdef HAVE_EVPORT
#include "ae_evport.c"
#else
    #ifdef HAVE_EPOLL
    #include "ae_epoll.c"
    #else
        #ifdef HAVE_KQUEUE
        #include "ae_kqueue.c"
        #else
        #include "ae_select.c"
        #endif
    #endif
#endif
```

按性能从高到低排列：evport (Solaris) > epoll (Linux) > kqueue (macOS/BSD) > select (兜底)。每个文件实现同一组 `static` 函数接口：

| 函数 | 职责 |
|:---|:---|
| `aeApiCreate` | 初始化底层数据结构（如 `epoll_create`） |
| `aeApiResize` | 调整容量 |
| `aeApiFree` | 释放资源 |
| `aeApiAddEvent` | 注册 fd 监听（如 `epoll_ctl`） |
| `aeApiDelEvent` | 取消监听 |
| `aeApiPoll` | 阻塞等待事件（如 `epoll_wait`） |
| `aeApiName` | 返回名字（`"epoll"`、`"kqueue"` 等） |

::: concept ae-platform-abstraction title="用 #include .c 实现编译期多态"

ae 没有用函数指针表做运行时分发，而是直接 `#include` 整个 `.c` 文件。被包含的文件里所有函数都声明为 `static`，编译后直接内联到 `ae.c` 的编译单元中。这意味着：

- 零运行时开销：没有间接调用，编译器可以内联优化。
- 接口靠约定：没有抽象基类或函数指针结构体，每个平台文件必须实现同名的 `static` 函数。
- 一个二进制只包含一套实现：不像运行时策略模式那样把所有平台代码都编译进去。

:::

::: generate ae-multiplexing-apis kind=comparison concepts=ae-platform-abstraction

面向有一定系统编程基础的读者，对比 epoll (Linux)、kqueue (macOS/BSD)、select (POSIX) 三种 I/O 多路复用机制的核心区别：接口形态（epoll 分 create/ctl/wait 三步，kqueue 用 changelist 批量操作，select 用 fd_set 位图）、性能特征（epoll/kqueue 是 O(就绪数) 而非 O(总监听数)，select 有 FD_SETSIZE 限制）、适用场景。说明为什么 ae 选择按这个优先级排列它们。

:::

以 epoll 为例，`aeApiPoll` 的实现：

```c
static int aeApiPoll(aeEventLoop *eventLoop, struct timeval *tvp) {
    aeApiState *state = eventLoop->apidata;
    int retval, numevents = 0;

    retval = epoll_wait(state->epfd, state->events, eventLoop->setsize,
            tvp ? (tvp->tv_sec*1000 + (tvp->tv_usec + 999)/1000) : -1);
    if (retval > 0) {
        numevents = retval;
        for (int j = 0; j < numevents; j++) {
            int mask = 0;
            struct epoll_event *e = state->events + j;

            if (e->events & EPOLLIN) mask |= AE_READABLE;
            if (e->events & EPOLLOUT) mask |= AE_WRITABLE;
            if (e->events & EPOLLERR) mask |= AE_WRITABLE|AE_READABLE;
            if (e->events & EPOLLHUP) mask |= AE_WRITABLE|AE_READABLE;
            eventLoop->fired[j].fd = e->data.fd;
            eventLoop->fired[j].mask = mask;
        }
    }
    return numevents;
}
```

它把 `epoll_wait` 返回的 `EPOLLIN`/`EPOLLOUT` 翻译成 ae 自己的 `AE_READABLE`/`AE_WRITABLE`，写入 `fired` 数组供上层分发。注意错误事件（`EPOLLERR`、`EPOLLHUP`）被同时映射为可读和可写——这样无论上层注册的是哪种回调，都能收到通知并做清理。

## 6. beforesleep / aftersleep 钩子

```c
void aeSetBeforeSleepProc(aeEventLoop *eventLoop, aeBeforeSleepProc *beforesleep) {
    eventLoop->beforesleep = beforesleep;
}

void aeSetAfterSleepProc(aeEventLoop *eventLoop, aeBeforeSleepProc *aftersleep) {
    eventLoop->aftersleep = aftersleep;
}
```

在 `aeProcessEvents` 中，beforesleep 在调用 `aeApiPoll` 之前执行，aftersleep 在返回之后执行。Redis 用 beforesleep 做大量工作：处理被阻塞的客户端、把待发送的回复写出去、刷写 AOF 缓冲区、处理集群消息等。aftersleep 则用来更新统计信息。

这两个钩子让 ae 保持通用的同时，给 Redis 提供了在事件循环关键节点插入业务逻辑的能力。

## 7. Redis 网络层如何接入 ae

ae 本身不涉及网络，但理解 Redis 如何使用它有助于看清全貌：

::: generate ae-redis-networking kind=example concepts=ae-file-event,ae-process-events

面向读者，描述一个客户端请求在 Redis 中从建立连接到收到回复的完整流程：Redis 启动时用 aeCreateFileEvent 把监听 socket 注册为可读事件，回调是 acceptTcpHandler；客户端连接到达时 acceptTcpHandler 被调用，它 accept 新连接并用 aeCreateFileEvent 把新 fd 注册可读事件，回调是 readQueryFromClient；客户端发送命令时 readQueryFromClient 被调用，解析并执行命令，把回复写入输出缓冲区，然后注册可写事件，回调是 sendReplyToClient（或在 beforesleep 中直接尝试写出）；可写事件触发时回复被发送给客户端。整个过程都在单线程的 aeMain 循环中完成。

:::

## 8. 设计启示

ae 值得学习的不是某个技巧，而是它的设计态度：

- **不做过度抽象**。没有泛型事件类型、没有观察者模式、没有注册中心。文件事件就是数组，时间事件就是链表。
- **按实际负载选方案**。Redis 的时间事件只有一两个，所以无序链表比最小堆更合适——代码更简单，而性能差异为零。
- **编译期决定，运行时不付代价**。平台适配用 `#include` 而非函数指针，整个 I/O 调用链可以被编译器内联。

::: generate ae-vs-libuv kind=comparison concepts=ae-process-events,ae-platform-abstraction

面向读者，简要对比 ae 与 libuv/libev 在设计哲学上的差异：ae 是 Redis 专用的、极简的、单线程的事件循环，不处理 DNS、线程池、文件系统操作等；libuv 是通用的跨平台异步 I/O 库，支持线程池、异步文件 I/O、信号处理等，但复杂度和代码量高出一个数量级。说明"专用 vs 通用"不是好坏之分，而是设计目标的差异。

:::
