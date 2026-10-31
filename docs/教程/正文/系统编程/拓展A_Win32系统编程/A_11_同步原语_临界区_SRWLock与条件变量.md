# A.11 同步原语（二）：临界区、SRWLock 与条件量

在上一篇 [A.10](/教程/正文/系统编程/拓展A_Win32系统编程/A_10_同步原语_等待函数与内核对象) 中，可等待内核对象展现了跨进程共享与多目标统一等待的强大通用性。但这种通用性伴随着不可忽视的上下文开销：每一次调用 `WaitForSingleObject` 本质上都是一次完整的系统调用（system call）。即便利基路径上根本不存在任何并发争用，CPU 也必须强行保存寄存器现场、自用户态陷落至内核空间完成状态判定，随后再度穿梭返回。对于在单一进程内部被数以万计的工作线程高频读写的计数器、路由配置表或任务队列而言，这类微秒级的内核往返累积起来足以吞噬掉大量的吞吐算力。

为此，Windows 在用户空间体系内提供了另一组高度轻量化的进程内专用原语：由硬件原子指令直接支撑的互锁函数（Interlocked Functions）、兼具自旋优化与递归特性的临界区对象（Critical Section Object）、尺寸仅为一个指针的读写锁原语 SRW 锁（Slim Reader/Writer Lock），以及用于协调生产与消费状态流转的条件量（Condition Variable，即 `CONDITION_VARIABLE`）。在不存在激烈争用的快速路径（fast-path）上，它们完全依托用户态内存指令即可完成锁的检视与夺取，执行成本通常仅为数纳秒。掌握这组原语的内存模型、消除潜在的数据竞争，并厘清用户态原语与内核对象之间的适用边界，是构建高吞吐并发组件的关键功底。

## 1. 数据竞争

首先通过一个最基础的并发失真场景来审视共享状态的脆弱性。两个线程并发地向同一个 `int` 类型对象各自累加一百万次，整个过程不引入任何同步屏障：

```c
/* race.c：两个线程各把同一个普通 int 加 1000000 次。 */
#include <stdio.h>
#include <windows.h>

#define ITERS 1000000

static int counter = 0;
static volatile LONG g_start = 0;

static DWORD WINAPI worker(LPVOID arg) {
    (void)arg;
    /* 等主线程放行，保证两个线程真正同时开跑 */
    while (InterlockedCompareExchange(&g_start, 0, 0) == 0) {
        Sleep(0);
    }
    for (int i = 0; i < ITERS; i++) {
        counter++;
    }
    return 0;
}

int main(void) {
    HANDLE threads[2];

    for (int i = 0; i < 2; i++) {
        threads[i] = CreateThread(NULL, 0, worker, NULL, 0, NULL);
        if (threads[i] == NULL) {
            printf("CreateThread failed: %lu\n", GetLastError());
            return 1;
        }
    }
    InterlockedExchange(&g_start, 1);
    WaitForMultipleObjects(2, threads, TRUE, INFINITE);
    for (int i = 0; i < 2; i++) {
        CloseHandle(threads[i]);
    }

    printf("counter = %d, expected %d\n", counter, 2 * ITERS);
    return 0;
}
```

`g_start` 只是一个发令枪，让两个线程尽可能同时进入循环，线程的创建与等待见 [A.9](/教程/正文/系统编程/拓展A_Win32系统编程/A_9_线程与线程本地存储)。编译时不加优化选项，运行三次：

::: terminal title="PowerShell - race"

```text
PS> gcc -std=c17 -Wall -Wextra -O0 race.c -o race.exe
PS> ./race.exe
counter = 1038126, expected 2000000
PS> ./race.exe
counter = 1011566, expected 2000000
PS> ./race.exe
counter = 1039657, expected 2000000
```

:::

运行结果不仅每次呈现出截然不同的数值，而且无一例外显著低于理论预期值 2,000,000，几乎有接近半数的递增操作在并发碰撞中凭空蒸发。究其根源，在于高级语言中看似简洁的 `counter++` 表达式在底层指令流水线中并非单个不可分割的操作，而是由「从主内存读取当前值加载至 CPU 寄存器」、「在寄存器内完成算术加一」以及「将计算结果写回目标内存地址」三个独立的机器指令周期组成。当两个并发执行的线程在时间切片上交错穿插时，完全可能几乎同时从内存读入相同的值 100，各自独立在私有寄存器中将其增加至 101，并前脚接后脚地覆写回相同的全局内存地址，导致两次实质性的自增被折叠为一次。这种由非同步并发读写引发的状态不一致即为经典的数据竞争（data race）。在 C11 标准中，任何未加保护的数据竞争均被直接界定为未定义行为（Undefined Behavior），主线教程第一部分 [23.2](/教程/正文/语法和标准库/23_原子操作与并发/23_2_原子操作) 已对此作出过详尽的底层推导。

值得玩味的是，若将同一个未同步的源码开启 `-O2` 高级优化后重新编译，实测连续六次输出均稳定命中 2,000,000 的正确数值。但切莫由此产生多线程并发已被编译器“自动修复”的错觉：查验其生成的反汇编代码便能洞悉原委——优化器将整个包含一百万次循环累加的计算块直接折叠优化成了一条单步常量加法指令 `addl $0xf4240, counter`。由于每个线程在整个执行周期中只需耗费几个纳秒执行这一条指令，两台流水线在绝对物理时间上刚好交叠碰撞的概率被压缩至微乎其微。然而系统的逻辑正确性并没有获得丝毫改善，它仅仅是将易于捕获的宽窗口竞争掩蔽成了只有在极端高负荷生产环境下才会偶然爆发的深层隐患。若在工程验证中仅基于 Release 发行配置进行黑盒验收，极易使此类致命缺陷潜伏逃逸。

## 2. 互锁函数

最省事的修法是换一个能一次完成「读、加、写」的加法。Windows 在 `<windows.h>` 里提供了一组互锁函数，最常用的是这三个：

```c
LONG InterlockedIncrement(LONG volatile *value);            /* 加一，返回新值 */
LONG InterlockedExchangeAdd(LONG volatile *value, LONG n);  /* 加 n，返回旧值 */
LONG InterlockedCompareExchange(LONG volatile *dest, LONG exchange, LONG comparand);
```

`InterlockedCompareExchange` 做的是：如果 `*dest` 等于 `comparand`，就把 `exchange` 写进去；无论是否写入，都返回 `*dest` 原来的值。它是这三个里最通用的一个，另外两个都能用它写出来。

返回值容易记混，直接跑一遍：

```c
/* interlocked_ret.c：x 从 5 开始，逐个看返回值与 x 的变化。 */
LONG x = 5;
ret = InterlockedIncrement(&x);                 /* -> 6, x = 6 */
ret = InterlockedExchangeAdd(&x, 3);            /* -> 6, x = 9 */
ret = InterlockedCompareExchange(&x, 100, 99);  /* -> 9, x = 9，不相等不写 */
ret = InterlockedCompareExchange(&x, 100, 9);   /* -> 9, x = 100，相等才写 */
```

::: terminal title="PowerShell - interlocked_ret"

```text
PS> gcc -std=c17 -Wall -Wextra interlocked_ret.c -o interlocked_ret.exe
PS> ./interlocked_ret.exe
x = 5
InterlockedIncrement(&x)        -> 6, x = 6
InterlockedExchangeAdd(&x, 3)   -> 6, x = 9
InterlockedCompareExchange(99)  -> 9, x = 9
InterlockedCompareExchange(9)   -> 9, x = 100
```

:::

`InterlockedIncrement` 返回新值，另外两个返回旧值。写循环时这个区别常常用得上：`InterlockedIncrement` 的返回值正好可以当作业编号，`InterlockedExchangeAdd` 的旧值正好是插入前的长度。

把 `race.c` 里的 `counter++` 换成 `InterlockedIncrement(&counter)`，计数对象声明为 `volatile LONG`，其余不动（完整程序在 `code/系统编程/拓展A_Win32系统编程/A_11_同步原语_临界区_SRWLock与条件变量/interlocked.c`），再跑三次：

::: terminal title="PowerShell - interlocked"

```text
PS> gcc -std=c17 -Wall -Wextra interlocked.c -o interlocked.exe
PS> ./interlocked.exe
counter = 2000000, expected 2000000
PS> ./interlocked.exe
counter = 2000000, expected 2000000
PS> ./interlocked.exe
counter = 2000000, expected 2000000
```

:::

绝大多数 Win32 互锁函数均默认隐式附带完整的全双向内存屏障（Full Memory Barrier），其不仅能确保竞争线程绝无可能观察到仅写入一半状态的撕裂中间值（torn write），而且能严格防止编译器指令重排与处理器乱序执行引擎跨越该边界重新调度内存访问。这种硬件层面的内存可见性保证，在强内存序（Strongly-ordered）的 x86/x64 架构下或许由于硬件天然禁止大部分读写重排而显得含蓄；但在具备弱内存序（Weakly-ordered）特性的 ARM64 设备（如 Windows on ARM 平台）上，缺失内存屏障将直接引发跨核心可见性延迟甚至状态颠倒。

在跨平台与原生 API 的工程选型上：若系统专门面向 Windows 生态深耕，Win32 的互锁函数族与 C11 标准库 `<stdatomic.h>` 中的原子类型及操作在指令生成上殊途同归，底层最终均映射至 `lock cmpxchg` 等硬件原子指令；若代码库存在跨类 UNIX 系统的移植诉求，采用 C11 的 `atomic_fetch_add` 则是更具兼容性的选择（例如在 MinGW-w64 工具链下同样能编译并取得一致的 2,000,000 正确结果）。需要极其警惕的是，切不可试图通过将整型对象简单修饰为 `volatile int` 配合手写 `while` 循环来臆测原子性：在标准 C 语义中，`volatile` 关键字的全部意义仅在于告知编译器禁止对该对象的访问做寄存器缓存优化，迫使其每次显式产生针对物理内存的读写操作，它既不具备把「读-改-写」三步原子化的硬件能力，也丝毫不提供任何跨核心的内存屏障保障。

<Quiz
  type="single"
  :options="[
    { label: '5', value: 'A' },
    { label: '6', value: 'B' },
    { label: '0', value: 'C' },
    { label: '1', value: 'D' }
  ]"
  answer="B"
>

<template #question>

`x` 是 `LONG` 类型的对象，值为 5。执行 `InterlockedIncrement(&x)` 之后，这个调用的返回值是？

</template>

正确答案是 **B**。`InterlockedIncrement` 返回加一之后的新值，也就是 6；`x` 自身同样变成 6。返回旧值的是 `InterlockedExchangeAdd` 与 `InterlockedCompareExchange`，记混了会让「取号」一类的代码从 0 开始。

</Quiz>

## 3. 临界区对象

互锁函数只解决了单个对象的加法。如果一段代码要同时修改好几个对象，并且这些对象之间有必须保持的关系，比如往链表里插入一个节点同时把长度加一，那么需要的是把整段代码圈起来，同一时刻只让一个线程进去。这段代码称为临界区（critical section），承担这个职责的对象叫临界区对象。第一部分 [23.3](/教程/正文/语法和标准库/23_原子操作与并发/23_3_互斥) 用标准 C 的互斥量讲过同一件事，这里的对象形状不同，要守的规矩一样。

```c
CRITICAL_SECTION cs;

InitializeCriticalSection(&cs);        /* 或者 InitializeCriticalSectionAndSpinCount(&cs, 4000) */

EnterCriticalSection(&cs);
counter++;                             /* 受保护的代码 */
LeaveCriticalSection(&cs);

DeleteCriticalSection(&cs);
```

`InitializeCriticalSectionAndSpinCount` 多一个自旋次数：锁被别人占着时，线程先原地空转这么多次再决定去睡，适合临界区很短、争用又多的场合。堆管理器自己给每个堆的临界区用的就是大约 4000。

`EnterCriticalSection` 在没有别的线程占用时，只做一条处理器提供的原子测试置位指令，不进内核；被占用时才自旋一阵，仍然拿不到才挂起等待。`TryEnterCriticalSection` 是不阻塞的版本，拿到返回非零，拿不到立即返回 0。

同一个线程重复进入是允许的，这一条与后面的 SRW 锁相反。用 `cs_recursive.c` 验证：

```c
/* cs_recursive.c 的主线程部分：连续进入两次，再分别观察别的线程能否进入。 */
EnterCriticalSection(&g_lock);
EnterCriticalSection(&g_lock);          /* 同一线程，不会阻塞 */
printf("entered twice without blocking\n");

LeaveCriticalSection(&g_lock);          /* 只离开一次，所有权还在本线程手里 */
/* 此时另一个线程的 TryEnterCriticalSection 返回 0 */

LeaveCriticalSection(&g_lock);          /* 再离开一次，锁才真正释放 */
/* 此时另一个线程的 TryEnterCriticalSection 返回 1 */
```

::: terminal title="PowerShell - cs_recursive"

```text
PS> gcc -std=c17 -Wall -Wextra cs_recursive.c -o cs_recursive.exe
PS> ./cs_recursive.exe
entered twice without blocking
other thread's TryEnter after one leave -> 0
other thread's TryEnter after two leaves -> 1
```

:::

进入几次就要离开几次，这是临界区对象最容易埋下的错误：某个提前返回的分支少写一句 `LeaveCriticalSection`，锁就永远留在自己手里，别的线程全部卡在 `EnterCriticalSection` 上，而程序本身还在跑。

::: concept win-sync-cs-user-mode title="临界区对象不是内核对象"

临界区对象由进程自己分配内存（一个 `CRITICAL_SECTION` 结构，本机 x64 上 40 字节），不需要 `CloseHandle`，没有句柄，也不能被 `WaitForSingleObject` 等待，只能被同一个进程内的线程使用。它可以用 `InitializeCriticalSection` 或 `InitializeCriticalSectionAndSpinCount` 初始化，用完后用 `DeleteCriticalSection` 释放；在初始化之前使用、把已经初始化的对象再初始化一次，都是错的。同一线程可以重复进入，释放的次数必须与进入的次数相同。跨进程互斥要用 A.10 的互斥量对象。

:::

用它修复 `race.c`，把自增夹在进入与离开之间即可（`cs_counter.c`）：

```c
/* cs_counter.c：worker 函数体。 */
for (int i = 0; i < ITERS; i++) {
    EnterCriticalSection(&lock);
    counter++;
    LeaveCriticalSection(&lock);
}
```

::: terminal title="PowerShell - cs_counter"

```text
PS> gcc -std=c17 -Wall -Wextra cs_counter.c -o cs_counter.exe
PS> ./cs_counter.exe
counter = 2000000, expected 2000000
```

:::

### 3.1 三种锁的开销

没有争用时，临界区对象与 SRW 锁都只在用户态转一圈，互斥量对象却每次都要进内核。`lock_cost.c` 让三者各做一百万次「加锁、改一个数、解锁」，用 `QueryPerformanceCounter` 计时：

::: terminal title="PowerShell - lock_cost"

```text
PS> gcc -std=c17 -Wall -Wextra lock_cost.c -o lock_cost.exe
PS> ./lock_cost.exe
critical section:     17.8 ms
SRW lock:             16.6 ms
mutex object:        258.7 ms
```

:::

本机十六个逻辑处理器上，临界区对象与 SRW 锁都在 16～18 毫秒，互斥量对象要 258 毫秒，差了一个数量级以上。具体数字随机器和负载变化，看数量级就够了：一百万次无争用加锁，用户态原语花几十毫秒，内核对象花几百毫秒。跨进程的互斥仍然得用内核对象，代价就是这一档。

## 4. SRW 锁

SRW 锁把「独占」与「共享」两种模式放进同一个对象，大小只有一个指针（本机 x64 上是 8 字节），不需要删除：

```c
SRWLOCK lock = SRWLOCK_INIT;      /* 静态初始化；动态初始化用 InitializeSRWLock */

AcquireSRWLockExclusive(&lock);   /* 写者：独占 */
/* 修改共享对象 */
ReleaseSRWLockExclusive(&lock);

AcquireSRWLockShared(&lock);      /* 读者：共享，可以多个线程同时持有 */
/* 读取共享对象 */
ReleaseSRWLockShared(&lock);
```

`TryAcquireSRWLockExclusive` 与 `TryAcquireSRWLockShared` 是不阻塞的版本。被持有时它们的结果：

::: terminal title="PowerShell - srw_try"

```text
PS> gcc -std=c17 -Wall -Wextra srw_try.c -o srw_try.exe
PS> ./srw_try.exe
exclusive held: TryAcquireExclusive -> 0
exclusive held: TryAcquireShared    -> 0
shared held:    TryAcquireExclusive -> 0
free:           TryAcquireExclusive -> 1
```

:::

独占锁被拿着时，两种获取都失败；只持有共享锁时再取独占也会失败，这就是「有读者在读时写者进不来」。读者与读者之间不互斥，这是 SRW 锁相对临界区对象的全部意义：读操作远多于写操作时，多个读者可以真正并行。

::: concept win-sync-srw-no-recursion title="SRW 锁不可递归"

同一线程第二次调用 `AcquireSRWLockExclusive` 获取自己已经持有的独占锁，会一直等下去，程序停在这一行，不会报错也不会返回。共享模式下重复获取本身能成功，但如果这期间有别的线程在等独占锁，两边就会互相等死。SRW 锁也不支持把共享所有权升级成独占所有权。共享模式下的重复获取不会被检测出来，释放次数却要相应增加（实测两次共享获取后释放一次，独占获取仍然失败；释放两次才成功），所以不要这样写。SRW 锁不能跨进程使用，没有删除函数，未使用且无人等待时可以直接丢弃。

:::

演示它要小心，死锁的程序不会自己退出。`srw_deadlock.c` 故意在同一个线程里获取两次独占锁，用 PowerShell 的进程对象加超时来收场：

::: terminal title="PowerShell - srw_deadlock"

```text
PS> gcc -std=c17 -Wall -Wextra srw_deadlock.c -o srw_deadlock.exe
PS> $p = Start-Process -FilePath .\srw_deadlock.exe -PassThru -NoNewWindow
PS> if (-not $p.WaitForExit(2000)) { $p.Kill(); "still running after 2 s, killed (PID $($p.Id))" }
first acquire done
still running after 2 s, killed (PID 10276)
```

:::

程序在控制台打印完第一行诊断信息后便陷入了无尽的静默，最终在两秒超时后被外围监控脚本强制终止。在真实的工程排错中，这类无任何错误码吐出的挂起现象，绝大多数源自锁获取调用链上的深层嵌套：外层业务函数在夺取锁后调用了下层通用工具函数，而内层函数内部由于抽象封装又独立请求了同一把锁。由于 SRW 锁坚决不记录拥有者线程标识，它无法甄别第二次请求究竟来自他人还是自身，因而不可避免地进入自我等待的死局。在后文关于条件量的章节中还会进一步看到，如果将等待逻辑嵌套在 `while` 状态轮询循环中，这类不可重入问题会变得更加难以觉察。

### 4.1 读多写少：配置表

`config_cache.c` 演示了一个典型的全局动态配置结构体，内部包含 `version` 与 `timeout_ms` 两个强相关的整型字段，并严格维持 `timeout_ms == 100 * (version + 1)` 这一业务不变式约束。四个工作线程以高频节奏并发读取该配置，而一个独立的配置重载线程则在后台周期性对其进行版本递增与覆写。在此场景中，读线程仅需申请共享读锁，而写线程则独占排他锁：

```c
/* config_cache.c：读线程与写线程。 */
LONG active;

AcquireSRWLockShared(&g_cfg.lock);
active = InterlockedIncrement(&g_active_readers);
/* 在锁内把要用的字段复制出来，锁外慢慢用 */
int version = g_cfg.version;
int timeout = g_cfg.timeout_ms;
if (timeout != 100 * (version + 1)) {
    InterlockedIncrement(&g_inconsistent);
}
Sleep(2);
InterlockedDecrement(&g_active_readers);
ReleaseSRWLockShared(&g_cfg.lock);

/* 写线程 */
AcquireSRWLockExclusive(&g_cfg.lock);
Sleep(5);                       /* 模拟重新装载配置 */
g_cfg.version = v;
g_cfg.timeout_ms = 100 * (v + 1);
ReleaseSRWLockExclusive(&g_cfg.lock);
```

::: terminal title="PowerShell - config_cache"

```text
PS> gcc -std=c17 -Wall -Wextra config_cache.c -o config_cache.exe
PS> ./config_cache.exe
writer:  version=1 timeout=200
writer:  version=2 timeout=300
writer:  version=3 timeout=400
reads by version: v0=16 v1=16 v2=16 v3=432
max concurrent readers = 4, inconsistent reads = 0
```

:::

实测数据显示，并发持有共享锁的读者峰值达到了 4 个，意味着全部四个工作线程在大多数时刻均处于并行读取状态；而在全量四百八十次高密度的状态检视中，未发生过哪怕一次新旧版本交错的不一致读取。无论写线程在何种时刻触发更新，读者所捕获的始终要么是前一版本的全套参数，要么是完成重载后的最新参数，绝无可能窥探到更新了一半的残缺撕裂状态。

读线程在持锁期间仅做最小化局部拷贝、随后立即脱离临界区并在锁外开展重度计算，这是并发系统设计中的黄金准则：将昂贵的业务处理逻辑严格隔绝在临界区之外，能让排他写锁的等待延迟从毫秒级直接骤降至纳秒级。在 `config_cache.c` 中故意置于共享锁内部的 `Sleep(2)`，仅仅是为了在终端演示中放大多读者重叠并发的可观测窗口；在生产代码的临界区内应严防任何主动休眠操作。

在进程内同步原语的设计选型中，现代 Win32 系统编程通常将 SRW 锁的独占模式作为互斥保护的首选方案：它的内存开销仅为一个指针大小，没有销毁清理负担，且在无争用路径上的吞吐表现优异。唯有当既有接口抽象深厚、因回调机制而必须依赖同线程重复递归锁定时，才退守选用临界区对象；但在更为干净的系统架构中，理应优先重构分层调用关系，消除重入需求。

## 5. 条件量（CONDITION_VARIABLE）

互斥机制固然能够无懈可击地保护共享数据结构的读写完整性，但它本身无法解决「当期望的前置业务状态不满足时该如何调度」的困境。以经典的有界环形队列为例：当消费者线程获取锁后发现队列为空，它唯一的退路只能是释放锁、调用休眠函数等待片刻、随后再次加锁探测。然而休眠时长的设定永远处于两难权衡之中：时延过短会导致大量 CPU 时间在空转加锁中被白白浪费，时延过长又会导致系统事件的响应延迟剧烈退化。

条件量（`CONDITION_VARIABLE`）正是为此类协作状态机量身定制的原语。它允许调用线程在原子性释放关联互斥锁的同时步入休眠挂起，并在另一个工作线程变更了共享状态并触发唤醒时被重新激活，整个过程由系统底层保障不会发生唤醒信号的漏失。

```c
CONDITION_VARIABLE cv = CONDITION_VARIABLE_INIT;   /* 静态初始化 */

/* 等：锁必须已经由本线程持有，标志告诉 API 它是以哪种模式持有的 */
SleepConditionVariableSRW(&cv, &lock, INFINITE, 0);

/* 叫醒一个 / 全部 */
WakeConditionVariable(&cv);
WakeAllConditionVariable(&cv);
```

`SleepConditionVariableSRW` 的第三个参数是超时（毫秒），第四个参数写 `CONDITION_VARIABLE_LOCKMODE_SHARED` 表示这把 SRW 锁是以共享模式持有的，写 0 表示独占模式。函数返回时，锁已经按原来的模式重新拿到手。临界区对象也有配套的 `SleepConditionVariableCS`。

### 5.1 有界队列

`bounded_queue.c` 采用 SRW 锁配合两个条件量实现了一个容量为 4 的环形队列：两个生产者线程各自向队列投递 8 个整型元素，两个消费者线程并发读取消费。当队列达到容量上限时生产者主动挂起等待空位，当队列彻底排空时消费者主动挂起等待新数据，最终依靠主线程注入的「关闭」标志完成优雅退出：

```c
/* bounded_queue.c：放入与取出。 */
static void put(struct queue *q, int value) {
    AcquireSRWLockExclusive(&q->lock);
    while (q->count == CAPACITY) {
        SleepConditionVariableSRW(&q->not_full, &q->lock, INFINITE, 0);
    }
    q->slot[q->tail] = value;
    q->tail = (q->tail + 1) % CAPACITY;
    q->count++;
    q->produced++;
    ReleaseSRWLockExclusive(&q->lock);
    WakeConditionVariable(&q->not_empty);
}

static int get(struct queue *q, int id) {
    int value;

    AcquireSRWLockExclusive(&q->lock);
    while (q->count == 0 && !q->closed) {
        SleepConditionVariableSRW(&q->not_empty, &q->lock, INFINITE, 0);
    }
    if (q->count == 0) {                     /* 关闭了，且确实没有剩余元素 */
        ReleaseSRWLockExclusive(&q->lock);
        return -1;
    }
    value = q->slot[q->head];
    q->head = (q->head + 1) % CAPACITY;
    q->count--;
    q->consumed++;
    q->sum += value;
    q->per_consumer[id]++;
    ReleaseSRWLockExclusive(&q->lock);
    WakeConditionVariable(&q->not_full);
    return value;
}
```

生产者和消费者之间的一轮交互：

```mermaid
sequenceDiagram
    participant P as 生产者
    participant Q as 队列与锁
    participant C as 消费者
    C->>Q: 加锁，检查 count 为 0
    C->>Q: SleepConditionVariableSRW 释放锁并睡下
    P->>Q: 加锁，写入元素，count 变为 1
    P->>Q: 解锁
    P->>C: WakeConditionVariable
    C->>Q: 被唤醒后重新加锁
    Note over C,Q: 再次检查 count，条件成立，取出元素
```

::: terminal title="PowerShell - bounded_queue"

```text
PS> gcc -std=c17 -Wall -Wextra bounded_queue.c -o bounded_queue.exe
PS> ./bounded_queue.exe
produced 16, consumed 16, sum 856, left in queue 0
consumer 0 took 8, consumer 1 took 8
PS> ./bounded_queue.exe
produced 16, consumed 16, sum 856, left in queue 0
consumer 0 took 11, consumer 1 took 5
```

:::

十六个元素全部取走，`sum` 是 0 到 7 与 100 到 107 的总和 856，队列最后为空。两个消费者各取多少每次都不同，取决于谁先抢到锁，这是正常的。

细看 `put` 与 `get` 的实现可以发现一个重要的并发性能细节：二者均遵循「先变更队列状态、再释放排他锁、最后触发条件量唤醒」的顺序。将唤醒调用置于临界区之外，可以有效规避所谓的“抢锁-重睡”惩罚（lock bounce）：如果在尚未释放锁的前提下唤醒对端线程，被唤醒的线程在脱离等待的瞬间会立刻尝试夺取当前锁，而此时锁依然被唤醒方牢牢掌控，导致刚醒来的线程只能再度被系统挂起并让出 CPU，白白耗费了两次昂贵的线程上下文切换。微软技术文档同样推荐将信号发射推迟至锁释放之后。

::: concept win-sync-cv-loop title="等待条件量必须写在循环里"

`SleepConditionVariableSRW` 成功返回，绝不能等同于目标业务条件已经自动就绪。这一返回可能源于底层的虚假唤醒（spurious wakeup，即没有任何其他线程调用唤醒接口，硬件或内核调度逻辑自行解除等待），也可能源于被其他并发工作线程率先捷足先登夺走了刚刚放入的元素（stolen wakeup），抑或是设定的等待超时窗口已到期。因此，前置状态的断言必须无一例外地封装于 `while (条件不成立) { 等待 }` 循环之中，在每次唤醒后强制重新检视最新状态。此外，该条件对象本身必须严格受关联互斥锁的保护，所有状态探测与修改均须在持锁状态下进行，确保在「状态判据检视」与「安全步入休眠」之间不给任何第三方线程留下插足的竞态窗口。

:::

写错了会怎样，看 `if_vs_while.c`。两个消费者都在等同一个空队列，生产者只放一个元素然后 `WakeAllConditionVariable` 叫醒两个：

::: compare vertical left-title="等待写成 if" right-title="等待写成 while"

```c
/* consumer_if_body：醒来后不再检查 */
struct ctx *c = arg;
struct queue *q = c->q;

AcquireSRWLockExclusive(&q->lock);
q->arrived++;
if (q->count == 0) {
    SleepConditionVariableSRW(&q->cv, &q->lock, INFINITE, 0);
}
int value = q->item;
q->count--;
printf("if   consumer %d: value=%d, count after take = %d\n", c->id, value, q->count);
ReleaseSRWLockExclusive(&q->lock);
```

---

```c
/* consumer_while_body：醒来后重新检查 */
struct ctx *c = arg;
struct queue *q = c->q;

AcquireSRWLockExclusive(&q->lock);
q->arrived++;
while (q->count == 0 && !q->closed) {
    SleepConditionVariableSRW(&q->cv, &q->lock, INFINITE, 0);
}
if (q->count == 0) {            /* 关闭了，确实没有元素 */
    printf("while consumer %d: closed, no item\n", c->id);
    ReleaseSRWLockExclusive(&q->lock);
    return 0;
}
int value = q->item;
q->count--;
printf("while consumer %d: value=%d, count after take = %d\n", c->id, value, q->count);
ReleaseSRWLockExclusive(&q->lock);
```

:::

::: terminal title="PowerShell - if_vs_while"

```text
PS> gcc -std=c17 -Wall -Wextra if_vs_while.c -o if_vs_while.exe
PS> ./if_vs_while.exe
if   consumer 0: value=42, count after take = 0
if   consumer 1: value=42, count after take = -1
---
while consumer 1: value=42, count after take = 0
while consumer 0: closed, no item
```

:::

`if` 版本里两个消费者取到了同一个元素，队列计数变成 -1，同一个元素被处理了两遍（哪一行先打印每次可能不同）。真实业务里这对应着两张订单被同一个订单号处理、一笔金额被扣两次。`while` 版本里后醒来的消费者发现队列还是空的，重新睡下，等到关闭标志生效才退出。

在调度策略上，`WakeConditionVariable` 与 `WakeAllConditionVariable` 的选用需审慎权衡：当仅有一个独立资源单元入队时，只需调用 `WakeConditionVariable` 单播激活一个等待者；唯有当队列宣布关闭、全局配置全量更新等影响到全体工作线程的宏观状态突变发生时，才应当调用 `WakeAll` 广播唤醒。若在单一消费场景下误用广播唤醒，将引发经典的“惊群效应”（thundering herd）：大量等待线程同时被内核调度唤醒，在疯狂争夺同一把排他锁的过程中逐一遭遇失败，并最终被迫重新步入休眠，徒增无意义的核间总线通信与调度开销。

相同的条件协同机制在 C11 标准与 POSIX 线程体系中亦有着完全平行的映射：C11 的 `cnd_wait`/`cnd_signal`/`cnd_broadcast`（详见第一部分 [23.4](/教程/正文/语法和标准库/23_原子操作与并发/23_4_条件等待)），以及 POSIX 的 `pthread_cond_wait`/`pthread_cond_signal`/`pthread_cond_broadcast`。尽管 API 命名因平台规范而略有演化，但其底层的同步规律一脉相承：等待逻辑必须嵌于循环判定内部、条件状态必须由伴生互斥锁守护、且通常建议先变更数据再发射唤醒通知。

<Quiz
  type="single"
  :options="[
    { label: '提高唤醒速度', value: 'A' },
    { label: '被唤醒时条件不一定成立', value: 'B' },
    { label: 'API 要求成对调用', value: 'C' },
    { label: '防止等待超时', value: 'D' }
  ]"
  answer="B"
>

<template #question>

为什么 `SleepConditionVariableSRW` 要放在 `while (条件不成立)` 循环里，而不是一个 `if` 里？

</template>

正确答案是 **B**。虚假唤醒、被其他线程抢先改变状态、超时返回，都会让函数返回时条件仍不成立。循环让它重新检查，条件不满足就继续睡。

</Quiz>

## 6. 一次性初始化与其他

### 6.1 一次性初始化

配置、单例、函数表这类东西往往希望「第一次用到时才初始化，而且只初始化一次」。用锁加一个标志位当然能做到，但每次取用都要加锁。`InitOnceExecuteOnce` 把这件事做进了 API：

```c
INIT_ONCE g_once = INIT_ONCE_STATIC_INIT;
PVOID context = NULL;

BOOL ok = InitOnceExecuteOnce(&g_once, init_config, NULL, &context);
```

`init_config` 的类型是 `BOOL (CALLBACK *)(PINIT_ONCE, PVOID, PVOID *)`，返回值表示初始化成功与否，第三个参数用来把初始化结果交出去；交出去的是一个指针或 `DWORD` 对齐的数据结构，它的低几位被系统占用，不能拿它传任意整数。四个线程同时调用的实测：

::: terminal title="PowerShell - once"

```text
PS> gcc -std=c17 -Wall -Wextra once.c -o once.exe
PS> ./once.exe
init_config runs on thread 1672
worker 2: port=23456 timeout=500
worker 0: port=23456 timeout=500
worker 1: port=23456 timeout=500
worker 3: port=23456 timeout=500
callback ran 1 time(s)
```

:::

回调只执行了一次，其余线程在它执行期间被挡住，等它返回之后直接拿到同一个结果。初始化成功后，函数内部只是读一个已经置位的指针，比任何锁都快。回调返回 `FALSE` 表示这次初始化失败，下一个调用者会重新尝试：单独写一个第一次返回 `FALSE` 的回调放进同样的四线程程序，回调被执行了两次，第一个线程拿到 `FALSE`、context 为空，后面三个线程拿到 `TRUE` 与结果。失败路径上已经分配的资源要自己清理。

它对应 C11 的 `call_once` 与 POSIX 的 `pthread_once`，`once_flag`/`pthread_once_t` 就是同一角色的对象。

### 6.2 WaitOnAddress

`WaitOnAddress` 更接近 Linux 的 futex：盯住一块 1、2、4 或 8 字节的内存，值没变就睡，值变了就醒。它不需要锁，也不需要事件对象：

```c
LONG observed = InterlockedCompareExchange(&g_state, 0, 0);

while (observed == 0) {
    WaitOnAddress((volatile VOID *)&g_state, &observed, sizeof observed, INFINITE);
    observed = g_state;
}
```

第二个参数是「我上次看到的值」，函数在内部比较：地址上的值已经变了就立即返回，没变才睡。另一个线程修改之后调用 `WakeByAddressSingle`（叫醒一个）或 `WakeByAddressAll`：

::: terminal title="PowerShell - wait_on_address"

```text
PS> gcc -std=c17 -Wall -Wextra wait_on_address.c -o wait_on_address.exe -lsynchronization
PS> ./wait_on_address.exe
waiter: state = 0, going to sleep
setter: state = 1, waking one
waiter: woke up, state = 1
```

:::

链接时别忘了 `-lsynchronization`，MinGW 默认链接的库里没有这两个函数。把 `WaitOnAddress` 的最后一个参数从 `INFINITE` 换成 250，等一个始终不变的值，函数在超时后返回 `FALSE`、`GetLastError()` 为 1460（`ERROR_TIMEOUT`），两次实测耗时 251.2 毫秒和 258.3 毫秒。

`WaitOnAddress` 同样允许提前返回（例如系统内存吃紧、上一次唤醒事件未能成功撤销等），因此线程从等待脱离后必须重新核对目标内存的值是否确实发生变动，这与条件量的 `while` 校验循环遵循相同的安全法则。它尤其适合实现诸如「引用计数降至零时唤醒垃圾清理线程」这类单一数值的原子等待；而若需要等待的是多个字段交织的复合状态机，采用条件量配合 SRW 锁则具备更高的代码表达力与鲁棒性。

### 6.3 同步屏障

`InitializeSynchronizationBarrier(&barrier, 线程数, 自旋次数)` 搭配 `EnterSynchronizationBarrier(&barrier, 0)` 能够协调一组并行计算的工作线程在各阶段末尾互相对齐，直到所有参与者全部到达集合点后才统一步调跨入下一阶段。在每一轮同步中，恰好仅有最后一个到达屏障的线程所调用的 `EnterSynchronizationBarrier` 会返回 `TRUE`，其余到达者均返回 `FALSE`（在由四个线程连续执行两轮的实测验证中，各轮次均恒定复现该规律），开发者可以借由这一特殊的布尔返回值委托该线程专门执行阶段性数据汇总或全局清理工作。屏障使用完毕后应调用 `DeleteSynchronizationBarrier` 销毁。该原语通常适用于将大规模矩阵运算或模拟管线划分为严密阶段的密集并行场景。

## 7. 同步原语选型全景

| 需求 | 选择 | 理由 |
| --- | --- | --- |
| 进程内的互斥 | SRW 锁，独占模式 | 仅占用一个指针大小，无争用快速路径下仅需一条硬件互锁指令 |
| 同一线程需要重复进入同一把锁 | 临界区对象 | SRW 锁会陷入不可恢复的自死锁；但更推荐重构设计消除递归 |
| 读多写少的高并发缓存 | SRW 锁，共享模式与独占模式混合 | 允许多个读者完全并行穿透，写者独占保护一致性 |
| 等待某一特定业务条件就绪 | 条件量加 SRW 锁 | 释放互斥锁与线程步入休眠由系统原子保障，绝不遗漏唤醒信号 |
| 跨越进程边界的互斥访问 | 命名互斥量对象 | 临界区对象与 SRW 锁均为纯进程内用户空间结构，详见 A.10 |
| 需要与文件/事件等句柄组合等待 | 内核对象与等待函数族 | 条件量与 SRW 锁无法直接加入 `WaitForMultipleObjects` 的句柄数组 |
| 单一数值计数器或布尔状态切换 | 互锁函数或 C11 原子操作 | 无需构建重型锁结构，单条硬件指令即可闭环 |
| 全局资源延迟安全初始化 | `InitOnceExecuteOnce` | 初始化完成后的后续访问仅蜕化为单次指针读取，零锁开销 |

在现代 Windows 平台构筑进程内并发模块时，推荐默认以 SRW 锁配合条件量作为主力骨架；将带有自旋和递归包袱的临界区对象限制在需要向下兼容重入的旧式接口层；当同步诉求超出单一虚拟地址空间、或者必须在多路等待中与异构句柄深度聚合时，再果断引入 A.10 的内核对象。在架构推演中，优先厘清「同一时刻排他还是共享」、「是阻断竞态还是等待特定业务状态就绪」，通常就能清晰锁定最佳的原语组合。

<Quiz
  type="single"
  :options="[
    { label: '临界区对象', value: 'A' },
    { label: 'SRW 锁', value: 'B' },
    { label: '命名互斥量对象', value: 'C' },
    { label: '条件量（CONDITION_VARIABLE）', value: 'D' }
  ]"
  answer="C"
>

<template #question>

两个进程要互斥地访问同一块共享内存，应该用下面哪一个？

</template>

正确答案是 **C**。临界区对象、SRW 锁与条件量均只在创建它们的目标进程虚拟内存中有效，其他外部进程无法获取亦无法解析其内存地址。由操作系统内核统合管理、并可通过全局或会话名称解析打开的命名互斥量对象，是实现跨进程安全互斥的标准途径（参见 A.10）。

</Quiz>

::: generate win-sync-primitive-map kind=comparison concepts=win-sync-cs-user-mode,win-sync-srw-no-recursion,win-sync-cv-loop

面向熟悉 POSIX 线程的读者，把本页的 Win32 原语与 pthread、C11 的对应物排成一张对照表：互斥量对象对 `pthread_mutex_t`，临界区对象对非递归的 `pthread_mutex_t` 但只在本进程内有效，SRW 锁对 `pthread_rwlock_t`，条件量对 `pthread_cond_t`，可等待的内核对象对 `eventfd`、`sem_t` 与 `poll` 的组合。逐项说明语义差别：Win32 的互斥量对象可跨进程且可递归、SRW 锁不可递归且不能升级、条件量必须配 SRW 锁或临界区对象而不是单独使用。最后用一句话点出 MinGW 的 winpthreads 把 pthread 互斥量建在临界区对象之上，这一点从 `objdump -p libwinpthread-1.dll` 的导入表里能看到 `InitializeCriticalSection`、`EnterCriticalSection` 与 `LeaveCriticalSection`。

:::

## 8. 习题

### 8.1 巩固

<Exercise id="44101" :d="2" :w="2" multi>

(1) _[1.1]_ `race.c` 里把 `counter++` 换成 `InterlockedIncrement(&counter)` 之后结果正确；如果改写成 `InterlockedExchangeAdd(&counter, 2)` 也正确，而写成 `counter = counter + 2` 仍然会出错，为什么？

(2) _[1.1]_ `lock_cost.c` 输出的三行里，互斥量对象那一行大约是另外两行的十五倍。这个差距来自哪一步操作？

(3) _[1.1]_ `srw_try.c` 里，共享锁被本线程持有时 `TryAcquireSRWLockExclusive` 返回 0。如果把前半段的 `AcquireSRWLockExclusive` 换成 `AcquireSRWLockShared`，那么紧随其后的 `TryAcquireSRWLockShared` 会返回什么？

(4) _[2.2]_ 下面四个需求分别该用本页或 A.10 的哪一种原语，各写一句理由：一个只有本进程访问的任务队列要互斥；两个进程共用一块共享内存要互斥；一个函数被多处调用、只允许初始化一次；一个工作线程要等到队列非空。如果最后一个还要求 500 毫秒超时，选择会不会变？

</Exercise>

<Exercise id="44102" :d="2" :w="3">

写一个 `lock_probe` 程序：主线程创建一把临界区对象和一把 SRW 锁，先各拿在手里，再创建两个线程分别对两把锁调用 `TryEnterCriticalSection` 与 `TryAcquireSRWLockExclusive`，打印返回值；主线程释放之后重复一次。

要求：

- 两个线程各自把结果写进自己的结构体，不要共享写入。
- 输出必须能看出「持有中 → 0、释放后 → 1」的变化，两次结果顺序固定。
- 再补一个对照：把 SRW 锁那一半改成同一线程连续两次 `AcquireSRWLockExclusive`，用 PowerShell 的 `Start-Process` 加 2 秒超时运行，记录它是被结束的还是自己退出的。

</Exercise>

### 8.2 编程

<Exercise id="44103" :d="3" :w="3">

把 `race.c` 扩展成三种修复方式的对比程序 `race3.c`：同一份工作（两个线程各加 1,000,000 次）分别用互锁函数、临界区对象、SRW 独占锁各做一遍，每次都把结果与耗时打印出来。

要求：

- 三种方式各自清零后重跑，结果都必须是 2000000。
- 用 `QueryPerformanceCounter` 计时，按互锁、临界区、SRW 的顺序打印毫秒数。
- 把临界区那一版再加一个变体：`InitializeCriticalSectionAndSpinCount(&cs, 4000)`，比较自旋次数对结果的影响（结果应当不变，耗时可能变化）。

验收：连续运行五次，五次结果全部正确；把耗时整理成一行，说明哪一种最快、差距有多大。

</Exercise>

<Exercise id="44104" :d="3" :w="3">

写一个读多写少的缓存表 `table.c`：一个 `SRWLOCK` 保护的 `struct entry { int key; int value; }` 数组，容量 64，写线程每秒插入或更新一条，四个读线程按 key 查找并统计命中次数。

要求：

- 读线程只用 `AcquireSRWLockShared`，写线程只用 `AcquireSRWLockExclusive`。
- 记录并打印「同时持有共享锁的读线程数」的最大值，以及读线程看到的「key 存在但 value 与 key 不匹配」的次数（不变式：`value == key * 10`）。
- 运行 2 秒后用一个 `volatile LONG` 停止标志让所有线程退出，最后打印命中总数与不变式被破坏的次数。

验收：最大值至少为 2，不变式破坏次数为 0。再用 `QueryPerformanceCounter` 测量写线程从调用 `AcquireSRWLockExclusive` 到真正拿到锁的等待时间，说明它与读线程在共享锁内停留的时长有什么关系，然后把读线程里的 `Sleep(2)` 去掉重测一次，比较两组数字。

</Exercise>

<Exercise id="44105" :d="4" :w="3" multi>

改写 `bounded_queue.c`，做三件事：

(1) _[3.2]_ 把容量改成命令行参数，例如 `bounded_queue.exe 1`、`bounded_queue.exe 16`，容量为 1 时也必须正确工作。

(2) _[3.2]_ 每个消费者记录自己取到的元素个数与总和，最后校验「生产总数 = 消费总数」且「总和相等」。

(3) _[4.3]_ 加一个 `put_batch(q, const int *values, int n)` 接口，一次放入多个元素，内部仍然逐个等待空位；再加一个 `get_batch(q, int *out, int max)`，一次取走至多 `max` 个。

验收：容量取 1、4、16 各跑三次，每次 32 个元素，校验全部通过；把 `get_batch` 实现成「不满足 `max` 个就继续等」，说明它在什么情况下会永远等下去。

</Exercise>

<Exercise id="44106" :d="4" :w="4">

`bounded_queue.c` 的关闭方式有两种写法，请都实现并比较：

- 写法 A（本页用的）：生产者全部结束后，主线程持锁把 `closed` 置 1，再 `WakeAllConditionVariable`，消费者取完剩余元素后退出。
- 写法 B：主线程直接 `WakeAllConditionVariable` 而不置 `closed`，让消费者自行判断「队列空就退出」。

要求：

- 两个版本都写，各自运行三次，每次 2 个生产者、2 个消费者、每个生产者 8 个元素。
- 打印每次实际消费的总数与预期 16 的差值。
- 解释写法 B 为什么不可靠，并给出一个能让它稳定丢元素的运行结果（可以调整 `Sleep` 的位置来放大窗口）。

</Exercise>

### 8.3 综合与挑战

<Exercise id="44107" :d="5" :w="4" multi>

下面这个程序读多写少，不变式是 `sum == count * 100`，但它有一个缺陷：

```c
struct cfg {
    SRWLOCK lock;
    long long sum;
    long long count;
    volatile LONG violations;
};

static DWORD WINAPI writer(LPVOID arg) {
    while (InterlockedCompareExchange(&g_stop, 0, 0) == 0) {
        AcquireSRWLockShared(&cfg.lock);   /* 这一行有问题 */
        cfg.sum += 100;
        cfg.count += 1;
        ReleaseSRWLockShared(&cfg.lock);
    }
    return 0;
}
```

(1) _[4.2]_ 不改代码，先运行一遍，记录 `violations` 与 `count` 的数量级。

(2) _[4.2]_ 找出缺陷并说明它为什么不会被 `-Wall -Wextra` 或任何运行时检查发现。

(3) _[3.2]_ 修复它，再运行同样的时长，确认 `violations` 为 0。

(4) _[5.4*]_ 修好之后，读线程要读到的是「一对」值：`sum` 与 `count` 必须来自同一次写。请说明修复第 (3) 问之后这一点为什么已经成立，再试一个替代方案：不用锁，把 `sum` 和 `count` 都改成 `volatile LONG64`，各自用 `InterlockedExchangeAdd64` 更新，写一个能观察到两个值不成对的程序。

验收：修复前后各贴一次输出，并写出缺陷属于「锁的模式用错」还是「锁的范围用错」。

</Exercise>

<Exercise id="44108" :d="6" :w="4">

实现一个线程安全的对象池 `pool.c`：容量 8，`acquire` 取一个对象，`release` 还回去，取不到时用条件量等待。

要求：

- 池本身用 `InitOnceExecuteOnce` 延迟初始化：第一次调用 `acquire` 时才建立对象数组与锁，多个线程同时首次调用也只能初始化一次，回调把池的指针作为 context 交出去。
- 池满时 `release` 不得阻塞（归还总是立即成功），取空时 `acquire` 等待。
- 支持关闭：关闭后 `acquire` 立即返回 `NULL`，已经借出的对象仍然可以归还。
- 用 6 个工作者线程各借还 200 次，统计最大同时借出数、总借出次数、等待唤醒次数。
- 用 `TryAcquireSRWLockExclusive` 做一次「不等待的自检」，在池的空闲块上加一个快速路径：空闲块大于 0 时先用 `TryAcquire` 拿一次锁，拿不到再走普通路径，比较加与不加这条快速路径的耗时。

验收：总借出次数为 1200，最大同时借出数不超过 8，初始化回调只执行一次，关闭后所有工作者在 1 秒内退出。

</Exercise>

<Exercise id="44109" :d="7" :w="5" scope="*">

同一个等待队列，用三种方式各实现一遍：

(1) SRW 锁加条件量，`get(q, timeout_ms)` 支持超时，超时时必须区分「真的超时」与「被唤醒但条件不成立」，并把 `SleepConditionVariableSRW` 的返回值与 `GetLastError()` 都检查一遍。

(2) `WaitOnAddress`/`WakeByAddressSingle` 版：把队列长度放进一个 `LONG`，消费者等在它上面，生产者修改后调用 `WakeByAddressSingle`（注意 `-lsynchronization`）。这个版本要自己处理「等待前重新检查」的循环。

(3) 纯 `Sleep(1)` 轮询版，作为性能下限的参照。

要求：

- 三个版本提供相同的 `put`/`get` 接口，用同一份测试驱动：2 个生产者、2 个消费者、每组 10 万个元素，记录总耗时与超时次数。
- 专门测试超时语义：一个空队列上调用 `get(q, 50)`，三个版本分别记录实际等待时间与返回值。第 (3) 版允许误差较大，但要说明误差从哪来。
- 用数据说明三者在中低争用下的差距，并给出你的选择。

本题需要先读完 A.10 的等待函数与内核对象，选做。

</Exercise>

## 9. 参考资料

- Microsoft Learn，Interlocked Variable Access：<https://learn.microsoft.com/en-us/windows/win32/sync/interlocked-variable-access>
- Microsoft Learn，Critical Section Objects：<https://learn.microsoft.com/en-us/windows/win32/sync/critical-section-objects>
- Microsoft Learn，Slim Reader/Writer (SRW) Locks：<https://learn.microsoft.com/en-us/windows/win32/sync/slim-reader-writer--srw--locks>
- Microsoft Learn，Condition Variables 与 Using Condition Variables：<https://learn.microsoft.com/en-us/windows/win32/sync/condition-variables>、<https://learn.microsoft.com/en-us/windows/win32/sync/using-condition-variables>
- Microsoft Learn，SleepConditionVariableSRW：<https://learn.microsoft.com/en-us/windows/win32/api/synchapi/nf-synchapi-sleepconditionvariablesrw>
- Microsoft Learn，InitializeCriticalSectionAndSpinCount：<https://learn.microsoft.com/en-us/windows/win32/api/synchapi/nf-synchapi-initializecriticalsectionandspincount>
- Microsoft Learn，One-Time Initialization：<https://learn.microsoft.com/en-us/windows/win32/sync/one-time-initialization>
- Microsoft Learn，WaitOnAddress：<https://learn.microsoft.com/en-us/windows/win32/api/synchapi/nf-synchapi-waitonaddress>
- Microsoft Learn，Synchronization Barriers：<https://learn.microsoft.com/en-us/windows/win32/sync/synchronization-barriers>
- Kenny Kerr，*Concurrency: Synchronization Primitives New to Windows Vista*，MSDN Magazine，2007 年 6 月：<https://learn.microsoft.com/en-us/archive/msdn-magazine/2007/june/concurrency-synchronization-primitives-new-to-windows-vista>
- Johnson M. Hart，*Windows System Programming*，第 4 版，第 8–10 章。
- Pavel Yosifovich，*Windows 10 System Programming, Part 1*，第 7 章。
- Jeffrey Richter、Christophe Nasarre，*Windows via C/C++*，第 5 版，第 8 章。
