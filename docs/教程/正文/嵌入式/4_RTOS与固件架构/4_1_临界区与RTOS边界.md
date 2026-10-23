# 4.1 临界区与 RTOS 边界

USART1 的接收中断把收到的每个字节塞进日志缓冲区，TIM2 的定时中断把一句状态说明也塞进同一个缓冲区。两个中断优先级不同，高优先级那个可以在低优先级那个写到一半时插进来。日志偶尔丢几行、偶尔多出半行乱码，单步调试怎么都复现不了。

[2.5 中断与 NVIC](/教程/正文/嵌入式/2_MCU裸机开发/2_5_中断与NVIC) 用一个“关中断”包住了主循环与单个中断共享的计数器，[3.4 数据帧、环形缓冲区与状态机](/教程/正文/嵌入式/3_驱动_通信与中间件/3_4_数据帧_环形缓冲区与状态机) 用单生产者单消费者的原子下标避免加锁。那个方案的前提是“写的人只有一个”。现在写的人有两个，而且关中断会连带推迟电机保护中断——它比日志重要得多。这一节处理这种更棘手的情况，并顺势引出 RTOS：有了任务以后，打断共享数据的又多了一类来源。

读完这一节，读者能判断一段共享数据会被谁打断，并在 Cortex-M3 上为它选对手段：PRIMASK 关中断、BASEPRI 按优先级屏蔽、LDREX/STREX 与 C11 原子操作；能说清 RTOS 内核管什么、不管什么，据此判断手里的项目该继续用前后台结构还是改用 RTOS。

## 1. 谁会打断谁

Cortex-M3 上，一段代码可能被三类事件打断，按紧迫程度排成一条线：

```mermaid
flowchart LR
    M["主循环<br/>（线程模式）"] --> L["低优先级中断<br/>（如 USART1，优先级 7）"]
    L --> H["高优先级中断<br/>（如 TIM1 刹车，优先级 1）"]
    H --> T["更高优先级中断<br/>（优先级 0，最紧急）"]
    M -.->|引入 RTOS 后| S["任务切换<br/>（PendSV，最低中断优先级）"]
```

左边的代码任何时候都能被右边的打断，右边的代码不会反过来被左边打断。所以判断有没有竞争，先看“谁可能打断谁”。

一个 `++` 在 C 里是一句话，编译出来不是：

::: terminal title="bash - 反汇编 count_inc_unprotected"

```text
$ arm-none-eabi-objdump -d race_disasm.o
00000000 <count_inc_unprotected>:
   0:	4a02      	ldr	r2, [pc, #8]
   2:	6813      	ldr	r3, [r2, #0]
   4:	3301      	adds	r3, #1
   6:	6013      	str	r3, [r2, #0]
   8:	4770      	bx	lr
```

:::

读、加、写三步。如果在这三步之间来了中断，而中断也改同一个对象，两边各自读到旧值、各自加一、各自写回，其中一次自增就丢了。这就是丢失更新（lost update）。

[2.5 中断与 NVIC](/教程/正文/嵌入式/2_MCU裸机开发/2_5_中断与NVIC) 的情形只在“主循环 + 一个中断”之间发生，关中断就解决了。[3.4 数据帧、环形缓冲区与状态机](/教程/正文/嵌入式/3_驱动_通信与中间件/3_4_数据帧_环形缓冲区与状态机) 的环形缓冲区进一步：写的人只有一个（中断），读的人只有一个（主循环），写者只改写下标、读者只改读下标，两个下标各自只有一个写者，所以即使读改写被打断也不出错。现在换成两个中断都写：

```mermaid
sequenceDiagram
    participant U as USART1 中断（优先级 7）
    participant T as TIM2 中断（优先级 3）
    participant M as 主循环
    U->>U: 读 write_idx = 10
    T->>T: 抢占，读 write_idx = 10
    T->>T: 写 buf[10]，write_idx = 11
    U->>U: 恢复，写 buf[10]（覆盖），write_idx = 11
    Note over U,M: 两个字节写到了同一个槽位，一个丢失
```

`write_idx` 有了两个写者，SPSC 的前提不成立，3.4 的免锁方案失效。要么让写者互斥，要么把“读下标、比较、加一”做成一条不会被打断的指令序列——后面两条路都要用到。

::: concept emb-rtos-volatile-not-atomic title="volatile 只保证访存，不保证原子"

`volatile` 告诉编译器“这个对象随时可能被别处改动，每次使用都去内存里读、写了立刻写回”，它挡的是编译器优化。处理器执行 `ldr`/`adds`/`str` 这三条指令时照样可以被中断打断，`volatile` 对此无能为力。共享数据的正确性靠的是让读写序列本身不可分割（临界区、原子指令），不是靠加 `volatile`。`volatile` 在这里只解决“中断里写的值主循环死活看不见”那一类问题。

:::

<Quiz
  type="single"
  :options="[
    { label: '中断与主循环共享一个 uint8_t 标志，中断只写 0 或 1，主循环只读', value: 'A' },
    { label: '两个中断都对同一个 uint32_t 计数做 +=，谁来都不关中断', value: 'B' },
    { label: '主循环读中断里更新的 uint64_t 毫秒计数，Cortex-M3 上一次读写 32 位', value: 'C' },
    { label: '中断只往单一写者的环形缓冲区写，主循环只读', value: 'D' }
  ]"
  answer="B"
>

<template #question>

下面哪种情况在 Cortex-M3 上存在数据竞争？

</template>

正确答案是 **B**。两个写者同时做读改写，`+=` 展开成三条指令，中间一定有机会被抢占，会丢更新。A 只是一个字节的读或写，单条指令完成，不存在“改到一半”；C 需要两次 32 位读，但只有一个写者（中断）和一个读者（主循环），只要读者有办法识别“读到一半被改了”并重读，就不需要关中断（本节习题里有一道就写这个）；D 是 3.4 的 SPSC 情形，本来就安全。

</Quiz>

## 2. 关中断的代价

最直接的办法是关中断。2.5 节写过保存和恢复 PRIMASK 的临界区，这里不重复写法，只算代价。

PRIMASK 置 1 后，除 NMI 与 HardFault 外的所有中断都被推迟。设临界区最长持续 $t_c$，一个本应在临界区期间响应的中断就要多等最多 $t_c$。中断延迟的上限因此变成

$$
t_{\text{latency}} = t_{\text{当前指令完成}} + t_{\text{更高优先级中断嵌套}} + t_c
$$

第三项完全由软件决定。72 MHz 的 F103 上，一条 32 位加载大约 2 个周期，复制 64 字节的循环用 `ldr`/`str` 成对搬 32 位，每趟 4 个周期左右：

::: terminal title="bash - 临界区里复制 64 字节的指令数"

```text
$ arm-none-eabi-gcc -std=c11 -mcpu=cortex-m3 -mthumb -O2 -DSTM32F103xB \
    -I<cmsis 目录> -c cp64.c
$ arm-none-eabi-objdump -d cp64.o
00000000 <log_copy>:
   0:	f3ef 8c10 	mrs	ip, PRIMASK
   4:	b672      	cpsid	i
   6:	1f02      	subs	r2, r0, #4
   8:	1f0b      	subs	r3, r1, #4
   a:	f101 003c 	add.w	r0, r1, #60	@ 0x3c
   e:	f853 1f04 	ldr.w	r1, [r3, #4]!
  12:	4283      	cmp	r3, r0
  14:	f842 1f04 	str.w	r1, [r2, #4]!
  18:	d1f9      	bne.n	e <log_copy+0xe>
  1a:	f38c 8810 	msr	PRIMASK, ip
  1e:	4770      	bx	lr
```

:::

循环体 4 条指令、16 趟，按每条 2 个周期（Flash 有 2 个等待周期，取指还要慢些）粗算 128 个周期，72 MHz 下约 1.8 μs。这个数听起来不多，但如果电机保护中断的响应预算是 10 μs，这里一口气花掉近五分之一；临界区里再写个 `printf`，动辄几百微秒，整个实时性就没了。

所以临界区的规矩很简单：里面只做几条指令能完成的事。不调用库函数，不打印，不复制大块内存，不碰 Flash。要搬 64 字节数据，就在临界区里只做“交换指针”这个动作，把复制放到外面（第 4 节的多生产者缓冲区和 4.5 节的内存池都是这个思路）。

::: tip 临界区里别用 printf

`printf` 内部有锁、有格式化、有缓冲区，执行时间与输出内容和库实现都有关，最坏情况无法估计。调试时在临界区里加一句打印，往往症状会变：时序被拖长，竞争反而“消失”了。

:::

## 3. 用 BASEPRI 按优先级屏蔽

关掉所有中断太粗暴。日志缓冲区只需要挡住“也写这个缓冲区的中断”，电机保护中断根本不碰它，没有理由一起推迟。Cortex-M3 为此提供了 BASEPRI。

BASEPRI 的规则一句话：写一个非零值时，所有优先级数值大于等于该值的中断被屏蔽，数值更小的（更紧急的）不受影响；写 0 表示不屏蔽任何中断。

| 字段 | 位 | 含义 |
| --- | --- | --- |
| — | 31:8 | 保留，读回 0 |
| PRI | 7:0 | 屏蔽阈值。非零时，优先级数值 ≥ PRI 的中断被屏蔽；0 表示全开 |

STM32F1 只实现了 8 位中的高 4 位，也就是 16 个优先级。写进去的值低位被忽略，所以阈值要左移 4 位构造，`NVIC_SetPriority()` 对单个中断也是这么做的。

写一个阈值 5 的临界区，编译出来是这样：

::: terminal title="bash - BASEPRI 临界区与 PRIMASK 临界区的指令对比"

```text
$ arm-none-eabi-objdump -d basepri_lock.o
00000000 <log_counter_add_basepri>:
   0:	f3ef 8111 	mrs	r1, BASEPRI
   4:	2350      	movs	r3, #80	@ 0x50
   6:	f383 8812 	msr	BASEPRI_MAX, r3
   a:	f3bf 8f4f 	dsb	sy
   e:	f3bf 8f6f 	isb	sy
  12:	4a03      	ldr	r2, [pc, #12]
  14:	6813      	ldr	r3, [r2, #0]
  16:	4418      	add	r0, r3
  18:	6010      	str	r0, [r2, #0]
  1a:	f381 8811 	msr	BASEPRI, r1
  1e:	4770      	bx	lr

00000024 <log_counter_add_primask>:
  24:	f3ef 8110 	mrs	r1, PRIMASK
  28:	b672      	cpsid	i
  ...
```

:::

`movs r3, #80` 的 80 是 `0x50`，即 `5 << 4`。优先级数值 0～4 的中断（比如刹车中断设为 1）不受影响，5～15 的被挡住。这就是 BASEPRI 相对 PRIMASK 的全部好处：屏蔽范围可以按紧迫程度裁剪。

`code/嵌入式/4_RTOS与固件架构/4_1_临界区与RTOS边界/basepri_lock.h` 把它封装成一对函数：

```c
static inline uint32_t basepri_lock(uint32_t priority_threshold) {
    uint32_t saved = __get_BASEPRI();
    __set_BASEPRI_MAX(priority_threshold);
    __DSB();
    __ISB();
    return saved;
}

static inline void basepri_unlock(uint32_t saved) {
    __set_BASEPRI(saved);
}
```

`__set_BASEPRI_MAX()` 对应 `MSR BASEPRI_MAX` 指令，它的语义是“只有新值更严格时才写入”：新阈值非零且比当前值小（屏蔽范围更宽）时更新，否则保持原值。这让临界区可以嵌套而不会互相拆台。

::: concept emb-rtos-basepri-mask-rule title="BASEPRI 的屏蔽规则"

BASEPRI 写非零值 $p$ 时，优先级数值 $\ge p$ 的中断被屏蔽，数值 $< p$ 的中断照常响应；写 0 表示不屏蔽。`MSR BASEPRI_MAX` 只在“新值比当前值更严格”（新值非零且数值更小时）才更新 BASEPRI，用于嵌套进入临界区。Cortex-M 的中断优先级数值越小越紧急，与 FreeRTOS 任务的优先级方向相反。

:::

嵌套时如果直接写 BASEPRI，问题就来了：

::: compare left-title="直接写 BASEPRI，内层提前放开屏蔽" right-title="用 BASEPRI_MAX，只提高不降低"

```c
uint32_t outer = __get_BASEPRI();
__set_BASEPRI(5u << 4);       /* 挡住 5 及以上 */
{
    uint32_t inner = __get_BASEPRI();
    __set_BASEPRI(9u << 4);   /* 内层阈值更大，屏蔽范围反而更窄 */
    /* 临界区 */
    __set_BASEPRI(inner);     /* 恢复成 0x50 */
}
__set_BASEPRI(outer);
```

---

```c
uint32_t outer = __get_BASEPRI();
__set_BASEPRI_MAX(5u << 4);
{
    uint32_t inner = __get_BASEPRI();
    __set_BASEPRI_MAX(9u << 4);
    /* 临界区 */
    __set_BASEPRI(inner);
}
__set_BASEPRI(outer);
```

:::

左栏看着也“恢复了”，问题出在内层那一步：`__set_BASEPRI(9 << 4)` 把阈值从 5 改到 9，屏蔽范围随之收窄，优先级 5～8 的中断——本来被外层挡住的——在内层代码执行期间反而能进来。右栏的 `__set_BASEPRI_MAX(9 << 4)` 发现新值比当前值宽松（数值更大），干脆不写，保持 5 不变。反汇编里两种写法只有指令不同：

::: terminal title="bash - 两种嵌套写法的指令"

```text
00000000 <bad_nesting>:
   0:	f3ef 8311 	mrs	r3, BASEPRI
   4:	2250      	movs	r2, #80	@ 0x50
   6:	f382 8811 	msr	BASEPRI, r2
   a:	f3ef 8211 	mrs	r2, BASEPRI
   e:	2190      	movs	r1, #144	@ 0x90
  10:	f381 8811 	msr	BASEPRI, r1
  ...

00000020 <good_nesting>:
  20:	f3ef 8311 	mrs	r3, BASEPRI
  24:	2250      	movs	r2, #80	@ 0x50
  26:	f382 8812 	msr	BASEPRI_MAX, r2
  2a:	f3ef 8211 	mrs	r2, BASEPRI
  2e:	2190      	movs	r1, #144	@ 0x90
  30:	f381 8812 	msr	BASEPRI_MAX, r1
  ...
```

:::

::: warning 嵌套的临界区一律用 BASEPRI_MAX 进入

内层直接写 BASEPRI，一个更大的阈值就意味着比外层更窄的屏蔽范围，外层保护的数据在内层期间重新暴露出来。只用 `__set_BASEPRI_MAX()` 进入、用进入前保存的旧值退出，就不用管嵌套了几层。

:::

按优先级屏蔽也有管不到的地方：优先级数值比阈值更小的中断照样能进来。所以“谁能改这段数据”必须想清楚——如果刹车中断（优先级 1）也要写同一个日志缓冲区，BASEPRI 阈值 5 挡不住它，只能用 PRIMASK 或者下面要讲的原子操作。阈值选的是“保护范围”，也是在给系统定规矩。

## 4. LDREX/STREX 与 C11 原子操作

临界区把“读改写”整段包起来，代价是打断别人的响应。原子指令让硬件保证“读改写”这件事，全程不关中断。

Cortex-M3 有一对独占访问指令：`LDREX` 读一个 32 位对象并在总线上标记“我在盯着这块地址”，`STREX` 只有当标记还在时才写入并返回 0，否则返回 1 表示失败。异常进入与返回都会清除独占标记，所以中间来了中断，等中断返回再执行 `STREX` 会失败，重试即可，不会写坏数据。

C11 的原子操作在 Cortex-M3 上就是编成这对指令的：

```c
#include <stdatomic.h>

atomic_uint_least32_t g_tx_bytes;

void add_tx_bytes(uint32_t n) {
    atomic_fetch_add_explicit(&g_tx_bytes, n, memory_order_relaxed);
}
```

::: terminal title="bash - atomic_fetch_add 生成的指令"

```text
$ arm-none-eabi-objdump -d atomic_inc.o
00000000 <add_tx_bytes>:
   0:	4b04      	ldr	r3, [pc, #16]
   2:	e853 1f00 	ldrex	r1, [r3]
   6:	4401      	add	r1, r0
   8:	e843 1200 	strex	r2, r1, [r3]
   c:	2a00      	cmp	r2, #0
   e:	d1f8      	bne.n	2 <add_tx_bytes+0x2>
  10:	4770      	bx	lr
```

:::

`bne.n` 跳回 `ldrex` 就是重试循环。`memory_order_relaxed` 只要求操作本身原子，不额外约束别的访问顺序；多个写者只关心计数本身时它够了。单核 MCU 上，这条循环不会“忙等”太久：失败只可能因为中途来了异常或另一个更高优先级的东西插了一脚，重试通常一次就过。

::: concept emb-rtos-atomic-limit title="原子操作只保护单个对象"

C11 原子操作保护的是一个对象的一个操作。几个对象要一起变（比如下标和长度、状态和错误码），原子操作只能一个一个来，中间仍可能被观察者看到“改了一半”的状态。这种场合要么合并进一个 32 位字里，要么用临界区把整段包住。Cortex-M3 上一次只能原子地读写 32 位，64 位计数做不到单个指令完成：`_Static_assert(ATOMIC_LLONG_LOCK_FREE == 2, ...)` 在 `-mcpu=cortex-m3` 下编不过，说明它不保证无锁。

:::

一个更实际的用法是把“写者抢占”变成“写者预约”。两个不同优先级的中断都要往一个环形缓冲区写，让它们先各自用 CAS 抢一个写入位置，再往自己的位置里写：

```c
bool mpsc_rb_put(struct mpsc_ringbuf *rb, uint8_t byte) {
    size_t w = atomic_load_explicit(&rb->write_idx, memory_order_relaxed);

    for (;;) {
        size_t r = atomic_load_explicit(&rb->read_idx, memory_order_acquire);
        if (w - r >= rb->cap) {
            return false; /* 缓冲区已满 */
        }
        if (atomic_compare_exchange_weak_explicit(
                &rb->write_idx, &w, w + 1u,
                memory_order_relaxed, memory_order_relaxed)) {
            break;
        }
    }

    size_t slot = w & rb->mask;
    rb->buf[slot] = byte;
    atomic_store_explicit(&rb->ready[slot], 1u, memory_order_release);
    return true;
}
```

`atomic_compare_exchange_weak_explicit` 用一对 LDREX/STREX 完成“比较并交换”：`write_idx` 还是 `w` 就改成 `w + 1` 并返回真，否则把 `w` 更新成当前值并返回假，循环重试。谁抢到 `w`，谁就独占 `slot = w & mask` 这个槽位。

有一点容易漏：只把位置预约出去不够，还得让读者知道“这个槽位的数据写完了没有”。低优先级中断可能刚预约完、还没把数据写进去，就被高优先级中断打断，后者预约了下一个位置并写完了。读者如果只看 `write_idx`，会读到低优先级那个还没写完的槽位。所以每个槽位配一个 `ready` 标志，写入完成后再用 release 语义置 1，读者先读 `ready` 再读数据。完整实现见 `code/嵌入式/4_RTOS与固件架构/4_1_临界区与RTOS边界/mpsc_reserve.c`，PC 上两个线程各写一万条记录的验证输出：

::: terminal title="PowerShell - MPSC 缓冲区的主机测试"

```text
$ gcc -std=c17 -O2 -Wall -Wextra -pthread test_mpsc.c mpsc_reserve.c -o test_mpsc
$ ./test_mpsc
MPSC host test passed: writer1=10000, writer2=10000
```

:::

两个写者各写一万条，读者按每个写者的顺序逐条校验，一条不丢、一条不重、次序不乱。

::: tip 单核 MCU 上不要用自旋锁

PC 上多线程抢锁，失败的一方自旋着反复试，反正另一个核迟早会释放。单核 MCU 上不行：如果中断服务程序去抢一把主循环持有的锁，主循环根本没机会运行，锁永远不会释放。中断与任务之间只能用“进不去就放弃”的原子操作（CAS 失败重试），或者用临界区，绝不能让中断自旋等。

:::

## 5. 有了任务以后

到这一节为止，打断都来自中断。引入 RTOS 以后多了一类：调度器在任意节拍或事件之后把 CPU 从一个任务切到另一个任务。两个任务各自执行 `g_count++`，被切换打断的那一份同样会丢更新，与中断的情形一模一样。区别只在于“打断者”是谁。

FreeRTOS 为此提供了三种手段。

`taskENTER_CRITICAL()` / `taskEXIT_CRITICAL()` 是最常用的。ARM_CM3 移植层的实现不是关 PRIMASK，而是用 BASEPRI，并且带一个嵌套计数：

```c
void vPortEnterCritical( void )
{
    portDISABLE_INTERRUPTS();
    uxCriticalNesting++;

    if( uxCriticalNesting == 1 )
    {
        configASSERT( ( portNVIC_INT_CTRL_REG & portVECTACTIVE_MASK ) == 0 );
    }
}

void vPortExitCritical( void )
{
    configASSERT( uxCriticalNesting );
    uxCriticalNesting--;

    if( uxCriticalNesting == 0 )
    {
        portENABLE_INTERRUPTS();
    }
}
```

（摘自 FreeRTOS-Kernel V11.3.0 `portable/GCC/ARM_CM3/port.c`，为篇幅略去注释，MIT 许可。）

其中 `portDISABLE_INTERRUPTS()` 展开为 `vPortRaiseBASEPRI()`，写的是 `configMAX_SYSCALL_INTERRUPT_PRIORITY` 这个阈值；`portENABLE_INTERRUPTS()` 写 0。嵌套计数让内层退出时不会提前放开屏蔽。这段代码反过来解释了第 3 节的选择：内核用 BASEPRI 而不用 PRIMASK，就是为了让优先级数值比 `configMAX_SYSCALL_INTERRUPT_PRIORITY` 更小的中断（比如刹车）在临界区期间照常响应。这个阈值的具体取值与规则在 4.4 节。

`vTaskSuspendAll()` / `xTaskResumeAll()` 只停调度，不关中断。任务切换被推迟，但中断照常进来；中断里调用的 `FromISR` API 会把需要切换的请求记下来，等 `xTaskResumeAll()` 时一并处理。它适合“不能关中断、但要保证别的任务看不到中间状态”的场合，比如在任务里改一个中断也会读的数据结构。

互斥量（mutex）保护的是资源而不是代码段：谁拿到谁用，用完归还，等的人阻塞而不是空转。它比临界区贵，但临界区里不能阻塞，一旦要在临界区里等外设就只好用互斥量。互斥量的细节与优先级继承在 4.3、4.6 节。

把这几种情况合起来看，选择取决于两件事：打断来源是谁，要保护的东西有多大。

| 打断来源 | 数据规模 | 手段 |
| --- | --- | --- |
| 单个中断，与主循环或单任务共享 | 32 位以内的单个对象 | C11 原子操作（`atomic_fetch_add` 等） |
| 多个不同优先级中断 | 32 位以内的单个对象 | 原子操作；需要多个对象一起变时用 PRIMASK 或 BASEPRI |
| 多个不同优先级中断 | 一段数据（缓冲区、结构体） | BASEPRI 阈值选在“所有写者之下、紧急中断之上”；挡不住的写者改用 PRIMASK |
| 任务与任务 | 任何规模 | `taskENTER_CRITICAL()`，或者用队列把数据交出去（4.3） |
| 任务与任务 | 需要等待外设、可能阻塞 | 互斥量，而不是临界区 |
| 任务与中断 | 一段数据 | 临界区（任务侧）+ `FromISR` API（中断侧），见 4.4 |

<Quiz
  type="single"
  :options="[
    { label: '高优先级中断只写、任务只读一个 32 位状态字，两边用 atomic_load/atomic_store', value: 'A' },
    { label: '任务与任务共享一个结构体，读写都用 taskENTER_CRITICAL 包住', value: 'B' },
    { label: '任务要发一次 I²C 事务，用互斥量保护总线而不是临界区', value: 'C' },
    { label: '刹车中断（优先级 1）也要写日志缓冲区，只把 BASEPRI 阈值设成 5', value: 'D' }
  ]"
  answer="D"
>

<template #question>

按第 5 节的表，下面哪种手段选错了？

</template>

正确答案是 **D**。BASEPRI 阈值 5 挡不住优先级数值更小的刹车中断（数值 1 更紧急），它照样能插进日志缓冲区的写操作。要挡住它只能用 PRIMASK（所有可屏蔽中断一起推迟），或者把写入改成无锁的方式。A、B、C 都与表里的对应行一致：单个 32 位对象用原子操作，任务之间的一段数据用临界区，需要等外设的场合用互斥量。

</Quiz>

还有一条在表外的规矩：临界区与原子操作保护的是“代码与数据的正确性”，不是“响应时间”。系统里最紧急的那个中断延迟是多少，必须在设计时就算出来，然后让每一处临界区都为这个预算让路。

## 6. RTOS 内核管什么、不管什么

把 RTOS 当“操作系统”容易期待过高。FreeRTOS 在 STM32F103 上做的事只有四件：

- 决定下一秒哪个任务运行（调度）；
- 让任务在等事件时让出 CPU，事件来了把它唤醒（阻塞与唤醒）；
- 提供时间，也就是节拍与延时；
- 提供任务之间传递数据、同步动作的工具（队列、信号量、互斥量、事件组）。

它不提供驱动、不提供协议栈、不提供文件系统——这些要么自己写，要么另外集成。它也不提供存储器保护：STM32F103 没有 MPU，所有任务都以特权级运行，一个任务写坏了另一个任务的栈，内核不会有任何反应（4.5 节展开）。文件系统、TCP/IP 这类组件在 FreeRTOS 之上单独选择，第 5 章的嵌入式 Linux 是另一条路线。

一个 FreeRTOS 工程的代码分成三类，各自能用什么 API 是硬规矩：

```mermaid
flowchart TB
    subgraph 调度器启动前
        I["初始化代码<br/>时钟、外设、创建任务与队列"]
    end
    subgraph 任务
        T["任务函数<br/>可以用全部任务 API"]
    end
    subgraph 中断
        A["优先级数值 ≥ configMAX_SYSCALL_INTERRUPT_PRIORITY<br/>可以调用 FromISR API"]
        B["优先级数值 < configMAX_SYSCALL_INTERRUPT_PRIORITY<br/>内核从不屏蔽，不许调用任何内核 API"]
    end
    I --> T
    A --> T
    B -.->|只能靠无锁的共享方式| T
```

- 调度器启动前的初始化代码：不能再调用会阻塞的 API（4.2 节第 8 节）。
- 任务：什么都能用。
- 中断分两种。优先级数值不小于 `configMAX_SYSCALL_INTERRUPT_PRIORITY` 的中断，内核会屏蔽它，它也可以调用以 `FromISR` 结尾的 API；优先级数值更小的中断，内核从不屏蔽（这正是不让刹车中断被推迟的原因），代价是它绝对不能调用任何内核 API。

最后一类中断如果必须与任务交换数据，就只能用无锁的方式：一个 `volatile` 标志、一对原子操作、或者 3.4 节的单生产者单消费者缓冲区。这也是为什么第 3 章那些免锁设计在 RTOS 里依然有用。

## 7. 什么时候该用 RTOS

回到开头那个需求：LED 每 500 ms 翻转，串口来命令就处理，传感器每 10 ms 采一次。用 [2.6 定时器、PWM 与系统节拍](/教程/正文/嵌入式/2_MCU裸机开发/2_6_定时器_PWM与系统节拍) 里那种非阻塞超级循环写，主循环里三件事轮流查：

```c
while (g_now_ms < RUN_MS) {
    uint32_t now = tick_now();

    if ((uint32_t)(now - last_led_ms) >= 500u) {
        last_led_ms = now;
        led_toggle();
    }

    if (state == SENSOR_IDLE) {
        if ((uint32_t)(now - last_sample_ms) >= 10u) {
            last_sample_ms += 10u;
            sensor_start_conv();
            state = SENSOR_CONVERTING;
        }
    } else if (sensor_conv_ready()) {
        filter_push(sensor_read_raw());
        state = SENSOR_IDLE;
    }

    uint8_t ch = 0u;
    if (uart_try_getc(&ch)) {
        cmd_feed_byte(ch);
    }

    g_now_ms++;
}
```

完整程序在 `code/嵌入式/4_RTOS与固件架构/4_1_临界区与RTOS边界/superloop_vs_tasks/superloop.c`，在 PC 上用一个每轮加一的假时钟跑 1.1 秒的虚拟时间，输出：

::: terminal title="PowerShell - superloop"

```text
$ ./superloop
t= 200 ms  收到命令字节 'A'
t= 252 ms  采样 25 次，均值 999 mg
t= 450 ms  收到命令字节 'B'
t= 500 ms  LED 翻转（第 1 次）
t= 502 ms  采样 50 次，均值 1000 mg
t= 700 ms  收到命令字节 'C'
t= 752 ms  采样 75 次，均值 1000 mg
t= 950 ms  收到命令字节 'D'
t=1000 ms  LED 翻转（第 2 次）
t=1002 ms  采样 100 次，均值 1000 mg
t=1050 ms  收到命令字节 'E'
虚拟时间 1100 ms 结束
```

:::

能跑，也没有丢事件。代价全在写法上：转换要拆成“启动”与“读取”两个状态，串口只能用 `try_getc` 轮询，每加一件事就要在主循环里多一个 `if`，而任何一处不小心写了个阻塞的等待，整个循环的节拍就全乱了。

同一需求的 FreeRTOS 版本（`rtos_tasks.c`）只有三个互不相干的循环：

```c
static void sensor_task(void *param) {
    TickType_t last_wake = xTaskGetTickCount();

    for (;;) {
        sensor_start_conv();
        /* sensor_read_raw() 内部等转换完成，用信号量或任务通知实现（4.3、4.4） */
        filter_push(sensor_read_raw());
        xTaskDelayUntil(&last_wake, pdMS_TO_TICKS(10));
    }
}

static void cmd_task(void *param) {
    for (;;) {
        uint8_t ch = uart_blocking_getc();
        cmd_feed_byte(ch);
    }
}

static void led_task(void *param) {
    for (;;) {
        led_toggle();
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}
```

三处阻塞等待分别由调度器负责：采样任务等周期，命令任务等字节，闪灯任务等时间。每个任务里可以按顺序写代码，不用状态机拆。三个任务的创建、调度与时间在 4.2 节展开，这一节只看结构上的差别。

::: compare left-title="非阻塞超级循环" right-title="三个任务"

```c
for (;;) {
    if (led_due()) { led_toggle(); }
    sensor_poll_step();
    if (uart_try_getc(&ch)) { cmd_feed_byte(ch); }
}
/* 任何一步等太久，全部节拍一起乱 */
```

---

```c
for (;;) { led_toggle(); vTaskDelay(500); }
for (;;) { cmd_feed_byte(uart_blocking_getc()); }
for (;;) { sample(); xTaskDelayUntil(&w, 10); }
/* 各自阻塞，时序由调度器分开管 */
```

:::

那么什么时候该换 RTOS？几个明确信号：

- 多个活动周期不同、紧迫程度不同，而且会继续增加。主循环里的 `if` 堆到十来个，加一个功能要重新算一遍所有时序，说明结构撑不住了。
- 某一步会阻塞很久（等 Flash 擦除、等 I²C 应答、等网络回复），而且不能让它拖住别的活动。超级循环里只能拆状态机，任务里直接阻塞就行。
- 需要按紧迫程度抢占：一个紧急事件来了，就该立刻得到 CPU，而不是等主循环转到它那一格。

反过来，这几类产品不必上 RTOS：功能单一、时序固定、加一个任务的成本比省下的复杂度还高的小设备；RAM 紧张到 10 KB 的堆都拿不出来的场合（F103C8 只有 20 KB）；以及所有活动本来就共用一个固定节拍、用中断就够的系统。作者的经验是：一个主循环能在一屏内写完、加功能时不用改已有逻辑的固件，就别上 RTOS。要 MMU 隔离、要完整协议栈与文件系统的，看第 5 章。

## 8. 习题

题面用到的程序都在 `code/嵌入式/4_RTOS与固件架构/4_1_临界区与RTOS边界/` 下。Cortex-M3 片段按各文件开头给出的命令编译，编译前需要一个放有 CMSIS 头文件的目录（CMSIS_5 5.9.0 的 `CMSIS/Core/Include` 与 cmsis_device_f1 v4.3.5 的 `Include` 放在一起）。

### 8.1 巩固

<Exercise id="64401" :d="1" :w="2" multi>

下面五段共享数据，写者与读者各不相同。

| 编号 | 写者 | 读者 | 被共享的对象 |
| --- | --- | --- | --- |
| A | TIM2 中断与 USART1 中断 | 主循环 | `uint32_t g_drop_count`，两个中断都做 `++` |
| B | USART1 中断 | 主循环 | `uint8_t g_rx_byte`，中断整体赋值，主循环只读一次 |
| C | 两个不同优先级的中断 | 主循环 | 环形缓冲区的 `write_idx` 与数据槽位 |
| D | 中断 | 主循环 | `struct { uint32_t seq; int16_t value; } g_sample` |
| E | USART1 中断 | 主循环 | 3.4 节的单生产者单消费者环形缓冲区 |

(1) _[1.2]_ 逐行判断有没有数据竞争，写出依据（写者有几个、读改写是否拆成多条指令、是不是多字对象），并给出该用的手段：不用保护、原子操作、BASEPRI 临界区或 PRIMASK。

(2) _[2.2]_ 编译 `race_disasm.c` 并反汇编，贴出 `count_inc_unprotected` 的指令，指出读、加、写分别对应哪一条，说明中断落在哪两条之间会丢更新。再把 `g_count++` 改成 `g_count = 1u`，重新反汇编，说明这次为什么不需要保护。

(3) _[2.2]_ 把 `race_disasm.c` 里的 `volatile uint32_t g_count` 改成 `atomic_uint_least32_t`，自增改成 `atomic_fetch_add_explicit(&g_count, 1u, memory_order_relaxed)`，重新编译反汇编，数一数多出了哪几条指令，说明重试循环在什么情况下会执行第二次。

</Exercise>

<Exercise id="64402" :d="2" :w="2">

`cp64.c` 在 PRIMASK 临界区里复制 16 个 32 位字。

(1) 用文件开头的命令编译并反汇编，数出循环体的指令条数与循环次数。按每条指令 2 个周期、72 MHz 估算这段临界区的持续时间，写出微秒数。

(2) 电机保护中断的响应预算是 10 μs。算出这段临界区占掉预算的百分比。再估算“临界区里只交换指针”的版本：两次 32 位加载、两次 32 位存储，共多少周期、多少微秒。

(3) 把 `log_copy()` 改成双缓冲：写者填满一块 `struct log_page { uint32_t data[16]; }` 之后，在临界区里只交换一个指向当前页的指针，复制由读者在临界区外完成。写出完整的类型、交换函数与调用方，并用反汇编证明临界区里只剩几条指令。

(4) 说明 (3) 的写法要求写者遵守什么约定，否则读者会读到写了一半的页。

</Exercise>

<Exercise id="64403" :d="3" :w="3">

下面的三层嵌套全部用 `__set_BASEPRI()` 进入，阈值依次是 5、9、3（已按高 4 位左移对齐）：

```c
uint32_t a = __get_BASEPRI();
__set_BASEPRI(5u << 4);
{
    uint32_t b = __get_BASEPRI();
    __set_BASEPRI(9u << 4);
    {
        uint32_t c = __get_BASEPRI();
        __set_BASEPRI(3u << 4);
        /* 最内层 */
        __set_BASEPRI(c);
    }
    /* 中间层 */
    __set_BASEPRI(b);
}
__set_BASEPRI(a);
```

(1) 分别写出最外层、中间层、最内层执行期间，哪些优先级数值的中断能进来。设刹车中断的优先级数值是 1，日志中断是 6，通信中断是 11。

(2) 指出这段代码的缺陷：哪一层把外层的屏蔽范围放开了，放开期间哪个中断能插进来。

(3) 用 `__set_BASEPRI_MAX()` 重写进入动作，说明为什么退出时仍然用 `__set_BASEPRI()` 写回保存值，而不是也用 MAX。如果把退出也改成 MAX 会发生什么？

(4) 把修改前后的两版写成两个函数，编译反汇编，确认进入指令变成了 `msr BASEPRI_MAX`，贴出这两条指令。

</Exercise>

### 8.2 编程

<Exercise id="64404" :d="3" :w="3">

`exercises/clock64_starter.c` 里，64 位时钟由中断维护的高 32 位与硬件计数器的低 32 位拼成。起始代码的 `clock64_read()` 直接拼一次就返回。

(1) 先构造一个能暴露缺陷的用例：读完高 32 位之后、读低 32 位之前，让计数器回绕（高 32 位加一、低 32 位归零）。为了在单线程的 PC 程序里插入这一刻，给 `clock64_read()` 加一个只在 `CLOCK64_TEST` 下编译的钩子：

```c
#ifdef CLOCK64_TEST
extern void (*g_between_reads_hook)(void);
#endif
```

在两次读之间调用它，测试里把钩子设成“模拟中断”的函数。写出这个用例，确认起始代码返回的值比回绕前小。

(2) 补全 `clock64_read()`：读高、读低、再读高，两次高 32 位不同就重试。要求全程不关中断、不调用任何阻塞函数。

(3) 把 (1) 的用例改成随机注入 1000 次回绕，验证返回值单调不减。验收：起始代码的版本出现至少一次倒退，补全后的版本一次都不出现。

(4) 说明这个办法为什么在“高 32 位由中断更新、低 32 位自由计数”的模型下成立，而在“两个 32 位都由软件分别更新”的模型下不成立。

</Exercise>

<Exercise id="64405" :d="4" :w="4">

`exercises/mpsc_ringbuf_starter.c` 是两个不同优先级中断写、主循环读的环形缓冲区骨架，`mpsc_rb_put()` 与 `mpsc_rb_get()` 待补全。

(1) 补全 `mpsc_rb_put()`：用 `atomic_compare_exchange_weak_explicit()` 预约 `write_idx`，写完数据后用 release 语义置 `ready[slot] = 1`。补全 `mpsc_rb_get()`：用 acquire 读 `ready[slot]`，取走数据后清零并推进 `read_idx`。

(2) 编译运行，验收输出为 `verified 20000 records from 2 writers without loss or reorder`。把 `ready` 的 acquire/release 换成 `memory_order_relaxed`，重跑 10 次，记录是否出现断言失败或乱序，并解释为什么在这个程序里它“看起来也能跑”。

(3) 加一个白盒用例：把 `read_idx` 与 `write_idx` 都置成 5，`ready[5]` 保持 0，确认 `mpsc_rb_get()` 返回假；再只把 `ready[5]` 置 1，确认取到的正是 `buf[5]` 里的字节。用这个用例说明 `ready` 数组解决的是“槽位已预约、数据未写完”的哪一段时间，以及为什么消费者不能只凭 `write_idx > read_idx` 就认为有数据可取。

(4) 读 `mpsc_reserve.c` 里 `mpsc_rb_put()` 的满判断，说明 `w - r >= rb->cap` 为什么用无符号减法而不是 `w >= r + cap`。

</Exercise>

<Exercise id="64406" :d="4" :w="4">

`superloop_vs_tasks/superloop.c` 用假时钟跑一个非阻塞超级循环。

(1) 在循环里加一个 `flash_erase()`，用 `g_now_ms += 30;` 模拟一次耗时 30 ms、期间 CPU 被占住的 Flash 擦除，由收到命令字节 `'C'` 时触发。运行程序，记录这次擦除前后两次 LED 翻转的时间间隔与采样次数，说明哪一项时序被拖坏了。

(2) 把擦除改成“启动 + 轮询完成”的状态机，与传感器的转换状态一样拆开。重跑，要求所有 LED 间隔回到 500 ms，采样一个不少。

(3) 在这个需求上再加两件事：一次 20 ms 的 I²C 事务、一次 50 ms 的网络等待。说明全部拆成状态机之后主循环会有多少个分支、每加一件事要改哪几处；据此写出你在第 7 节那几个信号里判断“该换 RTOS”的依据。

(4) 第 6 节说内核不提供驱动、协议栈、文件系统与存储器保护。列出这个超级循环工程换成 FreeRTOS 之后，仍然要自己实现的四样东西，各举一例说明它由内核负责还是由应用负责。

</Exercise>

### 8.3 综合与挑战

<Exercise id="64407" :d="5" :w="4">

下面是一段多中断固件的关键部分，日志缓冲区由两个中断写入、主循环读出。代码里的同步缺陷至少有五处。

```c
static volatile uint32_t g_log_count;
static uint8_t g_log_buf[64];
static uint32_t g_log_widx;

void TIM2_IRQHandler(void) {
    uint32_t saved = __get_BASEPRI();
    __set_BASEPRI(5u << 4);

    g_log_buf[g_log_widx++ & 63u] = 0xA5u;
    g_log_count++;
    printf("log %lu\n", (unsigned long)g_log_count);

    __set_BASEPRI(saved);
    TIM2->SR = 0;
}

void USART1_IRQHandler(void) {
    uint32_t saved = __get_BASEPRI();
    __set_BASEPRI(9u << 4);

    g_log_buf[g_log_widx++ & 63u] = 0x5Au;
    g_log_count++;

    __set_BASEPRI(saved);
    USART1->SR = 0;
}

void app_task(void *param) {
    for (;;) {
        taskENTER_CRITICAL();
        if (g_log_count > 0u) {
            uart_write_blocking(g_log_buf, 64u);
            g_log_count = 0u;
        }
        taskEXIT_CRITICAL();
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
```

设 TIM2 的中断优先级数值是 6，USART1 是 10，电机刹车中断是 1。

(1) 找出全部五处缺陷。每处写一句话说明后果，并指出它违反了本页哪一节的哪条规则。

(2) 逐处修改，给出改后的完整代码。要求：两个中断都能安全写日志；刹车中断的延迟不受影响；任务侧不阻塞在临界区里。

(3) 说明修改后 `g_log_widx` 与 `g_log_count` 分别由谁修改、用什么手段保护，以及主循环怎样判断“有新日志”。

(4) 这套改法在加入第三个写者（另一个中断）时还成立吗？如果不成立，写出你会换成什么结构，并给出它在 4.1 节里的名字。

</Exercise>

<Exercise id="64408" :d="5" :w="4">

第 5 节给了任务之间保护共享数据的三种手段。用模拟器把它们各自的效果量出来。

(1) 写一个会出错的版本：两个优先级相同的任务（时间片轮转）各自对一个全局 `uint32_t g_total` 做 100000 次 `g_total += 1`，第三个任务最后打印 `g_total`。要求第二个任务每做 1000 次让出一次 CPU（`vTaskDelay(1)` 或不加延时的 `taskYIELD()`），保证切换真的落在读改写中间。运行 10 次，记录 `g_total` 与 200000 的差。

(2) 分别用 `taskENTER_CRITICAL()` / `taskEXIT_CRITICAL()`、`vTaskSuspendAll()` / `xTaskResumeAll()`、`xSemaphoreCreateMutex()` 三种手段包住自增，各跑 10 次，记录结果是否都等于 200000。

(3) 在三种手段里各加一行 `console_printf()`（模拟“临界区里做了耗时的事”），并加一个优先级最高的心跳任务，每 5 ms 醒来打印一次节拍。记录心跳在三种写法下的最大间隔。说明互斥量那一版心跳为什么几乎不受影响：拿着锁的任务照样会被更高优先级的心跳抢占，锁只挡住另外那个计数任务。临界区与 `vTaskSuspendAll()` 两版为什么都被拖长：前者靠 BASEPRI 把 SysTick 一起挡住，后者让调度器停摆，两者都让心跳任务醒不过来。再指出这两版对**中断**的影响不同：哪一版期间串口中断照常响应，哪一版被推迟。

(4) 按第 5 节的表，为下面四种情形各选一种手段并说明理由：两个任务共享一个 32 位计数器；两个任务共享一个 64 字节的结构体；任务要发一次 I²C 事务；任务改一个中断也会读的结构，且不能关中断。

(5) 把 `taskENTER_CRITICAL()` 版本里的自增换成一个会调用 `vTaskDelay()` 的函数。解释运行结果（提示：临界区里不能阻塞），并说明这种情况应该改用哪种手段。

验收：给出 (1)(2) 各 10 次的结果表与 (3) 的三种心跳最大间隔；目标构建可选，模拟器上能跑即可。

</Exercise>

## 9. 参考资料

- ARM, *Cortex-M3 Devices Generic User Guide* (DUI0552A)，异常模型、PRIMASK/BASEPRI、同步原语：https://developer.arm.com/documentation/dui0552/a/
- ST, *PM0056 Cortex-M3 programming manual*：https://www.st.com/resource/en/programming_manual/pm0056-stm32f10xxx20xxx21xxxl1xxxx-cortexm3-programming-manual-stmicroelectronics.pdf
- FreeRTOS, *Running the RTOS on a ARM Cortex-M Core*：https://www.freertos.org/RTOS-Cortex-M3-M4.html
- FreeRTOS, *Mastering the FreeRTOS Real Time Kernel*, ch08 Resource Management：https://github.com/FreeRTOS/FreeRTOS-Kernel-Book/blob/main/ch08.md
- FreeRTOS-Kernel V11.3.0，`portable/GCC/ARM_CM3/port.c`、`portmacro.h`：https://github.com/FreeRTOS/FreeRTOS-Kernel/tree/V11.3.0/portable/GCC/ARM_CM3
- cppreference, *Atomic operations library*：https://en.cppreference.com/w/c/atomic
- Arm, *CMSIS-Core (Cortex-M)：内核寄存器访问函数*：https://arm-software.github.io/CMSIS_5/Core/html/group__Core__Register__gr.html
- Joseph Yiu, *The Definitive Guide to ARM Cortex-M3 and Cortex-M4 Processors*, 3rd ed., Newnes, 2014（ISBN 978-0-12-408082-9）。
