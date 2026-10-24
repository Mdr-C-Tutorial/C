# 4.8 RT-Thread

4.7 拆开的是 FreeRTOS 的工程骨架：几个源文件、一份 `FreeRTOSConfig.h` 以及极简的移植层汇编，所有内容紧紧围绕调度器本身展开。而展开 RT-Thread 的源码仓库时，呈现在眼前的则是一套更具平台感的生态全貌：`src/` 负责承载微内核，同级的 `components/` 囊括了设备驱动框架、虚拟文件系统、网络协议栈与命令行 Shell，`bsp/` 目录下则陈列着大量主流硬件的板级支持包。连接串口即可键入交互式命令的开箱体验，让它更像一个完备的嵌入式运行时系统，任务调度仅仅是其底层运转的基础构件。

在众多量产项目与软硬件生态中，RT-Thread 占据着可观的市场份额。要平滑过渡到这套体系，关键在于将 4.2 至 4.5 节建立起的通用并发认知——任务流转、同步互斥、缓冲队列与内存分配——对齐到它所特有的机制中：线程生命周期、邮箱与消息队列的职能划分、以结构体嵌入实现的内核对象模型，以及借助链接器段属性落地的 `INIT_APP_EXPORT` 自动初始化体系。本节将沿用经典的生产者-消费者模型作为对照主线，展现同一套并发逻辑在两套风格迥异的 API 体系下如何舒展表达。

::: concept emb-rtt-opposite-conventions title="RT-Thread 与 FreeRTOS 相反的三条约定"

- **优先级数值越小越高**。RT-Thread 里 0 是最高优先级，空闲线程排在最后（默认 `RT_THREAD_PRIORITY_MAX - 1`）；FreeRTOS 里数值越大越高。
- **栈大小以字节计**。`rt_thread_create` 的 `stack_size` 是字节数，内核按 `RT_ALIGN_SIZE` 对齐后分配；FreeRTOS 的 `usStackDepth` 是以字为单位的个数。
- **时间片只对同优先级的就绪线程有效**。`rt_thread_create` 的最后一个参数 `tick` 是时间片长度，不同优先级之间始终是抢占式的。

:::

## 1. RT-Thread 是什么

RT-Thread 以 Apache-2.0 许可发布，允许商用与闭源修改，比 GPL 宽松，也没有 MIT 那种「必须保留版权声明」之外的额外要求。仓库分三个版本：

- **标准版**（`rt-thread`）：完整内核 + 组件 + BSP，本节用的就是它。
- **Nano**（`rt-thread-nano`）：只有内核与最基本的驱动框架，几个文件，适合资源极紧的芯片。在 GCC 下要让启动文件跳到 `entry()`：Nano 不用 `main` 作为入口约定。
- **Smart**（`rt-thread-smart`）：带 MMU 的版本，可以跑在 Cortex-A 上，用户态与内核态分开。本教程不涉及。

本教程固定 v5.3.0（截至 2026 年 10 月的最新版，内核 API 与 v5.2 相同）。官方仓库带 `bsp/stm32/stm32f103-blue-pill`，正好是本教程的参考硬件 STM32F103C8 最小系统板。

两者的定位差异主要体现在架构边界的划定上：**FreeRTOS 严格收敛于轻量级内核的职责，仅提供基础的任务调度与同步原语；RT-Thread 则进一步向外延伸，将设备驱动抽象、交互式 Shell、文件系统、网络协议栈以及配套工程构建工具整合为一体**。这种平台化设计极大降低了搭建设施脚手架的门槛，但也使得 RT-Thread 标准工程的代码体积与内存占用基准更高（以 Cortex-M3 编译最小内核加 klibc 约需 27 KB Flash，详见第 9 节实测）。当面对可用资源以几十 KB 计的严苛硬件，或者仅需纯粹调度器的微控制器场景时，过多的附带组件往往需要花费精力进行裁剪。

## 2. 线程

RT-Thread 的「任务」叫线程（thread），创建方式有两种：

```c
/* 动态线程：控制块与栈都从堆上分配 */
rt_thread_t rt_thread_create(const char *name,
                             void (*entry)(void *parameter),
                             void *parameter,
                             rt_uint32_t stack_size,
                             rt_uint8_t priority,
                             rt_uint32_t tick);

/* 静态线程：控制块与栈由调用者提供 */
rt_err_t rt_thread_init(struct rt_thread *thread,
                        const char *name,
                        void (*entry)(void *parameter),
                        void *parameter,
                        void *stack_start,
                        rt_uint32_t stack_size,
                        rt_uint8_t priority,
                        rt_uint32_t tick);
```

创建之后还要调用 `rt_thread_startup()`，线程才进入就绪队列。这一步容易漏：`rt_thread_create` 返回非空只说明分配成功，不调用 `startup` 线程永远不会运行。下面的生产者-消费者程序（`code/嵌入式/4_RTOS与固件架构/4_8_RT-Thread/producer_consumer_rtt.c`）就是这样起两个线程的：

```c
tid = rt_thread_create("sampler", sampler_thread, RT_NULL, 1024, 10, 10);
if (tid != RT_NULL) {
    rt_thread_startup(tid);
}
tid = rt_thread_create("proc", processor_thread, RT_NULL, 1024, 12, 10);
if (tid != RT_NULL) {
    rt_thread_startup(tid);
}
```

`sampler` 的优先级是 10，`proc` 是 12；由于数值越小优先级越高，10 的执行权重高于 12，采样线程就绪时便会立即抢占数据处理线程。倘若习惯了 FreeRTOS「数值越大越优先」的逻辑，这里的调度顺序就会完全颠倒。分配的栈大小 1024 指的是 1024 字节，与 FreeRTOS 中传入 256（即 256 个 32 位字，同样为 1024 字节）在量纲上存在差异，跨平台迁移时不能机械地复用数值。

线程在生命周期中经历初始、就绪、运行、挂起和关闭五种状态。`rt_thread_startup` 促使新创建的线程从初始状态加入就绪队列；调用 `rt_thread_mdelay` 则使当前线程转入挂起状态并由定时器在延时到期后唤醒；此外还可以通过 `rt_thread_suspend` 与 `rt_thread_resume` 显式操控挂起与恢复；若线程调用 `rt_thread_delete`，内核仅先将其标记为关闭状态并从调度链表中剔除，其控制块与栈内存的真正回收动作被推迟到空闲线程中异步完成。

在 RT-Thread 中，`main` 函数本身就运行在一个被调度器管理的常规线程环境中。在开启 `RT_USING_USER_MAIN`（系统默认开启）的情况下，内核在完成底层初始化后会自动派生 main 线程来调用开发者的 `main()` 入口，其栈深度与优先级由 `rtconfig.h` 中的 `RT_MAIN_THREAD_STACK_SIZE` 与 `RT_MAIN_THREAD_PRIORITY` 统一指定。因此在 `main()` 内部可以直接调用 `rt_thread_mdelay` 等阻塞式 API，而无需顾虑调度器尚未运转。

## 3. 线程间同步与通信

RT-Thread 提供了五种基本的同步与通信机制，按用途分：

| 机制 | 类型 | 特点 | FreeRTOS 对应 |
| --- | --- | --- | --- |
| 信号量 `rt_sem` | 同步 | 计数值，可跨线程/中断 | `SemaphoreHandle_t` |
| 互斥量 `rt_mutex` | 同步 | 带优先级继承，只能同一线程取放 | `SemaphoreHandle_t`（互斥类型） |
| 事件集 `rt_event` | 同步 | 32 位标志，支持「与」「或」，读后清除 | `EventGroupHandle_t` |
| 邮箱 `rt_mailbox` | 通信 | 每封 4 字节，通常传指针 | 无直接对应（用队列长度 4 字节模拟） |
| 消息队列 `rt_mq` | 通信 | 按值拷贝变长消息 | `QueueHandle_t` |

信号量与互斥量的区别在 4.3 讲过（互斥量有持有者，能做优先级继承）。RT-Thread 的互斥量同样带优先级继承，官方文档明确说它「能解决优先级反转问题」。事件集与 FreeRTOS 的事件组思路相同，但 RT-Thread 的 `rt_event_recv` 可以在收到后自动清除标志位：

```c
rt_uint32_t recv;
rt_event_recv(&g_event, EVT_A | EVT_B,
              RT_EVENT_FLAG_AND | RT_EVENT_FLAG_CLEAR,
              RT_WAITING_FOREVER, &recv);
```

邮箱与消息队列的分工是 RT-Thread 独有的：邮箱只放 4 字节的值（在 32 位系统上刚好放一个指针），开销小，适合传「有一个新数据，指针在这里」这类通知；消息队列按值拷贝整条消息，适合传结构体。FreeRTOS 没有邮箱这一层，统一用队列，靠「队列项长度为 4 字节」模拟。

把 4.3 的 `producer_consumer.c` 改写成 RT-Thread 版，核心差异如下：

::: compare left-title="FreeRTOS：队列 + 互斥量" right-title="RT-Thread：消息队列 + 互斥量"

```c
static QueueHandle_t g_sample_queue;
static SemaphoreHandle_t g_console_mutex;

static void sampler_task(void *param) {
    for (size_t i = 0; i < SAMPLE_COUNT; ++i) {
        sample_t s;

        fake_sensor_read(i, &s);
        if (xQueueSend(g_sample_queue, &s, portMAX_DELAY) == pdPASS) {
            xSemaphoreTake(g_console_mutex, portMAX_DELAY);
            console_printf("[sampler] sent #%u\n", (unsigned)(i + 1));
            xSemaphoreGive(g_console_mutex);
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

g_sample_queue = xQueueCreate(4, sizeof(sample_t));
g_console_mutex = xSemaphoreCreateMutex();
```

---

```c
static rt_mq_t g_sample_mq;
static rt_mutex_t g_console_lock;

static void sampler_thread(void *param) {
    for (int i = 0; i < SAMPLE_COUNT; ++i) {
        struct sample s;

        fake_sensor_read(i, &s);
        if (rt_mq_send(g_sample_mq, &s, sizeof(s)) == RT_EOK) {
            log_line("[sampler] sent #%d\n", i + 1);
        }
        rt_thread_mdelay(10);
    }
}

g_sample_mq = rt_mq_create("sample", sizeof(struct sample), 4, RT_IPC_FLAG_PRIO);
g_console_lock = rt_mutex_create("console", RT_IPC_FLAG_PRIO);
```

:::

几处参数含义不同，改写时最容易出错：

- `xQueueCreate(4, sizeof(sample_t))` 对应 `rt_mq_create("sample", sizeof(struct sample), 4, RT_IPC_FLAG_PRIO)`，长度与项大小的顺序正好相反。
- `RT_IPC_FLAG_PRIO` 表示多个线程等待时按优先级唤醒（默认），`RT_IPC_FLAG_FIFO` 按先来后到。
- `rt_mq_recv` 的返回值是**收到的字节数**（`rt_ssize_t`），取到消息时为正数；FreeRTOS 的 `xQueueReceive` 返回 `pdPASS`/`pdFAIL`。v5 起这个返回值类型才改成 `rt_ssize_t`，v4 及更早是 `rt_err_t`，看旧代码时要留意，以所用版本的 `rtthread.h` 为准。
- `rt_thread_mdelay(10)` 的参数是**毫秒**，`vTaskDelay(pdMS_TO_TICKS(10))` 的参数是节拍。`rtconfig.h` 里 `RT_TICK_PER_SECOND` 为 1000 时两者数值相同，改成 100 就差十倍。

消息队列按值拷贝，一次最多拷 `msg_size` 字节；传大结构体时拷贝开销明显。这种场合改用邮箱传指针，接收方拿到指针后直接读——但要注意指针指向的内存在接收方使用期间必须有效（4.9 活动对象会讨论这一点）。

<Quiz
  type="single"
  :options="[
    { label: '优先级 10 更高，栈 1024 表示 1024 个字（4096 字节）', value: 'A' },
    { label: '优先级 10 更高，栈 1024 表示 1024 字节', value: 'B' },
    { label: '优先级 12 更高，栈 1024 表示 1024 字节', value: 'C' },
    { label: '优先级 12 更高，栈 1024 表示 1024 个字（4096 字节）', value: 'D' }
  ]"
  answer="B"
>

<template #question>

`rt_thread_create("sampler", entry, RT_NULL, 1024, 10, 10)` 与 `rt_thread_create("proc", entry, RT_NULL, 1024, 12, 10)` 这两个线程，谁的优先级更高？栈大小是多少？

</template>

正确答案是 **B**。RT-Thread 里 0 最高、数值越大越低，所以 10 优先；栈大小以字节计，1024 就是 1024 字节。把它与 FreeRTOS 的 `xTaskCreate` 对照时，栈参数要额外乘 4 才等价。

</Quiz>

## 4. 内核对象模型

RT-Thread 的内核对象（线程、信号量、互斥量、定时器……）都从同一个结构开始：

```c
struct rt_object {
#if RT_NAME_MAX > 0
    char        name[RT_NAME_MAX];
#else
    const char *name;
#endif
    rt_uint8_t  type;
    rt_uint8_t  flag;
    …
    rt_list_t   list;
};

struct rt_thread {
    struct rt_object            parent;   /* 必须是第一个成员 */
    void                        *sp;
    void                        *entry;
    …
};
```

`rt_thread` 的第一个成员是 `rt_object`，`rt_semaphore` 的第一个成员是 `rt_ipc_object`（它自己也以 `rt_object` 开头）。所以 `rt_thread_t` 可以直接当 `rt_object_t` 用，取名字、取类型都不需要额外转换：

```c
/* object_model.c：把 rt_thread 指针当 rt_object 用 */
static void dump_object(rt_object_t obj) {
    char name[RT_NAME_MAX + 1] = { 0 };

    rt_object_get_name(obj, name, sizeof(name));
    rt_kprintf("object name=%-8s type=%d\n", name, (int)rt_object_get_type(obj));
}

dump_object((rt_object_t)tid);
```

这就是 C 里最朴素的「继承」：把基类结构放在派生结构的第一个成员，两种指针互相转换时地址不变。语法和标准库第 10 章讲结构体内存布局时给过同样的例子。

对象容器（object container）按类型把同类对象串成一条链表。`rt_object_for_each(RT_Object_Class_Thread, ...)` 遍历的正是线程那条链表——msh 的 `list thread` 命令就是这么实现的，所以它能把系统里所有线程列出来，包括你不知道名字的那些。对象的 `name` 长度由 `RT_NAME_MAX` 限制，默认 8 字节（含结尾的空字节），所以名字最多 7 个字符。名字超长时会被截断，编译不报错，`list thread` 里看到的就是截断后的名字。

`object_model.c` 把这两件事合在一起：

```c
static rt_err_t print_thread(rt_object_t object, void *data) {
    rt_uint32_t *count = (rt_uint32_t *)data;

    dump_object(object);
    (*count)++;
    return RT_EOK;
}

rt_uint32_t count = 0;
rt_object_for_each(RT_Object_Class_Thread, print_thread, &count);
```

这段代码在本机用 arm-none-eabi-gcc 13.3 编译通过：`-mcpu=cortex-m3 -mthumb -O2 -Wall -Wextra`，目标文件 232 字节文本段。它有硬件依赖（需要调度器与对象容器已初始化），只在板上跑才有意义，所以这里只做编译检查。

## 5. 自动初始化与命令导出

RT-Thread 的工程里常能看到这样的代码：

```c
static int blink_init(void) {
    rt_thread_t tid = rt_thread_create("blink", blink_thread, RT_NULL, 512, 20, 10);

    if (tid != RT_NULL) {
        rt_thread_startup(tid);
    }
    return RT_EOK;
}
INIT_APP_EXPORT(blink_init);
```

表面上看，工程中并没有任何业务代码去直接调用 `blink_init`，也没有在全局数组里显式登记该函数指针，但在系统引导过程中它却会被准时执行。这一机制的达成，依靠的是编译器属性、链接器脚本布局与内核启动序列之间的巧妙协作。

`INIT_APP_EXPORT(blink_init)` 展开成下面这一行（`gcc -E` 的真实输出）：

```c
__attribute__((used)) const init_fn_t __rt_init_blink_init
    __attribute__((section(".rti_fn." "6"))) = blink_init;
```

它把一个函数指针放进名为 `.rti_fn.6` 的段，`used` 属性防止编译器因为「没人引用」把它优化掉。RT-Thread 的链接脚本里有这样一段（`bsp/stm32/stm32f103-blue-pill/board/linker_scripts/link.lds`）：

```text
/* section information for initial. */
. = ALIGN(4);
__rt_init_start = .;
KEEP(*(SORT(.rti_fn*)))
__rt_init_end = .;
```

`KEEP` 防止链接器把没人引用的段丢掉，`SORT` 按段名排序。段名的数字部分决定了执行顺序：`1` 是板级初始化，`2` 是组件预初始化，`3` 是设备初始化，`4` 是组件初始化，`5` 是环境初始化，`6` 是应用初始化。启动时分两次遍历：`rt_components_board_init()` 在调度器启动前调用，跑 `rti_board_start` 到 `rti_board_end`（级别 1）之间的函数；`rt_components_init()` 在 main 线程里调用，跑 `rti_board_end` 到 `rti_end`（级别 1.end 到 6.end）之间的函数。

这种通过自定义输入段收集函数指针的技巧，与 Linux 内核中广泛采用的 `initcall` 机制在设计思路上完全一致。其核心价值在于模块解耦：各个驱动或业务组件只需就地声明自己的初始化函数，无需在统一的系统初始化大函数中逐行维护庞杂的调用清单；但也必须承担由此带来的确定性代价——由于处于同一级别（例如同时声明为 `INIT_APP_EXPORT`）的函数在最终段内的相对次序依赖于链接器的输入顺序与文件名排序，模块间若存在隐式的依赖先后，就极易引发偶发性的启动时序缺陷。

同一个思路还用在命令导出上。`MSH_CMD_EXPORT(mdr_stat, print sampling statistics)` 展开成：

```c
const char __fsym_mdr_stat_name[] __attribute__((section(".rodata.name"))) = "mdr_stat";
const char __fsym_mdr_stat_desc[] __attribute__((section(".rodata.name"))) = "print sampling statistics";
__attribute__((used)) const struct finsh_syscall __fsym_mdr_stat
    __attribute__((section("FSymTab"))) = { __fsym_mdr_stat_name, (syscall_func)&mdr_stat };
```

一个 `struct finsh_syscall` 记录名字、描述与函数指针，放进 `FSymTab` 段。msh 启动时从这个段里读出所有命令，所以任何模块只要写一行 `MSH_CMD_EXPORT`，命令就出现在 shell 里，不需要改 shell 代码。

用 arm-none-eabi-gcc 13.3 编 `init_export_demo.c`（里面各有一次 `INIT_BOARD_EXPORT`、`INIT_APP_EXPORT`、`MSH_CMD_EXPORT`），`objdump -h` 能看到这几个段：

::: terminal title="bash - objdump -h init_export_demo.o"

```text
  3 .rodata.name  00000025  00000000  00000000  00000040  2**2
  4 .rti_fn.1     00000004  00000000  00000000  00000068  2**2
  5 .rti_fn.6     00000004  00000000  00000000  0000006c  2**2
  6 FSymTab       00000008  00000000  00000000  00000070  2**2
```

:::

`nm` 给出的符号与之对应：

::: terminal title="bash - nm init_export_demo.o"

```text
00000000 00000008 R __fsym_mdr_stat
00000000 0000001a R __fsym_mdr_stat_desc
0000001c 00000009 R __fsym_mdr_stat_name
00000000 00000004 R __rt_init_led_app_init
00000000 00000004 R __rt_init_led_init
00000008 00000004 t led_app_init
00000000 00000004 t led_init
00000004 00000004 t mdr_stat
```

:::

`.rti_fn.1` 与 `.rti_fn.6` 各 4 字节，正好是一个函数指针；`FSymTab` 8 字节，正好是 `struct finsh_syscall`（两个指针成员）。把它们链接进完整固件之后，初始化表按地址顺序排列：

::: terminal title="bash - nm pc2.elf（按地址排序）"

```text
0800aec8 T __rt_init_rti_start
0800aec8 T __rt_init_start
0800aecc T __rt_init_rti_board_start
0800aed0 T __rt_init_rti_board_end
0800aed4 T __rt_init_producer_consumer_start
0800aed8 T __rt_init_rti_end
0800aedc T __rt_init_end
```

:::

`rti_start` 与 `rti_board_start` 之间什么也没有，是因为这份最小工程没有 `INIT_BOARD_EXPORT` 的函数；`producer_consumer_start` 落在 `rti_board_end`（级别 1.end）与 `rti_end`（级别 6.end）之间，正好是应用级。启动时它会被调用，两个线程随之创建。

::: warning 初始化顺序不能靠「谁写在前面」

`.rti_fn` 段的排序依据是段名，与源文件里的先后无关。两个模块都标 `INIT_APP_EXPORT` 时，谁先执行可能随链接顺序变化。有先后依赖的初始化要放进不同级别（例如依赖设备的那一层用 `INIT_DEVICE_EXPORT`，用设备的那一层用 `INIT_APP_EXPORT`），或者在函数里显式检查依赖是否就绪。

:::

<Quiz
  type="multi"
  :options="[
    { label: '把函数指针放进 .rti_fn.<级别> 段，启动时按段名顺序调用', value: 'A' },
    { label: '链接脚本用 KEEP(*(SORT(.rti_fn*))) 收集并按名字排序', value: 'B' },
    { label: '内核启动时扫描 FSymTab 段，把命令登记进命令表', value: 'C' },
    { label: '在 main 函数里为每个模块手工调用一次初始化', value: 'D' }
  ]"
  :answer="['A', 'B']"
>

<template #question>

`INIT_APP_EXPORT` 让函数「写了就生效」，靠的是下面哪些机制？

</template>

正确答案是 **A、B**。C 描述的是 `MSH_CMD_EXPORT` 的命令表；D 描述的恰好是这个机制要免掉的手工调用。另外要注意 `used` 属性也必须写上，否则编译器会把没人引用的指针优化掉。

</Quiz>

## 6. FinSH 与 msh

FinSH 是 RT-Thread 的命令行组件，msh 是它的模块 shell 模式。打开 `RT_USING_FINSH` 之后，串口上会出现一个 `msh />` 提示符，可以敲命令查看系统状态、调用导出的函数、执行测试。常用命令：

| 命令 | 作用 |
| --- | --- |
| `list thread` 或 `ps` | 列出所有线程：优先级、状态、栈用量、剩余节拍 |
| `list device` | 列出已注册的设备 |
| `free` | 显示堆内存的总量、已用量、历史峰值 |
| `help` | 列出所有可用命令 |

`list_thread` 的输出类似（照 RT-Thread 文档的示例，实际数值随工程而变）：

::: terminal title="msh - list_thread"

```text
msh />list_thread
thread   pri  status      sp     stack size max used left tick  error
-------- ---  ------- ---------- ----------  ------  ---------- ---
tshell    20  ready   0x00000118 0x00001000    29%   0x00000009 000
tidle     31  ready   0x0000005c 0x00000200    28%   0x00000005 000
timer      4  suspend 0x00000078 0x00000400    11%   0x00000009 000
```

:::

`status` 一列写的是 `ready`、`suspend`、`running`；`max used` 是栈使用峰值占栈大小的比例——这一列在 FreeRTOS 里对应栈高水位线（4.5），但 RT-Thread 的数值更容易读。注意 `pri` 一列：`tshell` 是 20，`tidle` 是 31，数值小的优先级更高，所以 `tshell` 比空闲线程优先。

导出自定义命令只需要一个函数加一行宏：

```c
static int mdr_stat(int argc, char **argv) {
    (void)argc;
    (void)argv;

    rt_kprintf("sampler: %u samples, %u drops\n", g_samples, g_drops);
    return 0;
}
MSH_CMD_EXPORT(mdr_stat, print sampling statistics);
```

函数签名固定为 `int (*)(int argc, char **argv)`，返回值没有约定，通常返回 0。参数解析可以调用 `rt_strcmp`、`atoi` 之类的函数；msh 默认按空格切分。

`help` 命令列出所有导出项，内容来自 `FSymTab` 段。开启 `FINSH_USING_DESCRIPTION` 后每行后面会带描述，也就是 `MSH_CMD_EXPORT` 第二个参数的内容。

## 7. I/O 设备框架

RT-Thread 的驱动层并不直接向上层暴露裸机寄存器或厂商 HAL 接口，而是统一抽象为「设备」（device）模型，提供一套规范化的访问接口：

```c
rt_device_t dev = rt_device_find("uart1");
rt_device_open(dev, RT_DEVICE_FLAG_INT_RX);
rt_device_set_rx_indicate(dev, rx_callback);
rt_device_read(dev, -1, buf, size);
rt_device_write(dev, -1, buf, size);
rt_device_control(dev, cmd, arg);
```

设备按用途划分为字符设备、块设备、网络接口、通用杂项设备等不同门类，每一类通过对应的操作表（`struct rt_device_ops`）绑定底层的读写实现。板级支持包（BSP）负责在系统启动时初始化硬件并注册具名的设备实例，上层应用则仅通过诸如 `"uart1"` 的字符串标识符进行检索与操作，无需关心底层芯片的寄存器差异。这正是 3.2 节所探讨的 BSP 抽象设计在操作系统环境下的完整落地。

串口是设备框架用得最多的一类。用中断接收、线程里处理的标准写法如下（`uart_device_sample.c`）：

```c
static struct rt_semaphore g_rx_sem;
static rt_device_t g_uart;

/* 接收回调：串口每收到一批字节就调一次，运行在中断上下文 */
static rt_err_t uart_rx_ind(rt_device_t dev, rt_size_t size) {
    (void)dev;
    if (size > 0) {
        rt_sem_release(&g_rx_sem);
    }
    return RT_EOK;
}

static void reader_thread(void *param) {
    char buf[READ_BUF_SIZE];

    for (;;) {
        rt_size_t n;

        if (rt_sem_take(&g_rx_sem, RT_WAITING_FOREVER) != RT_EOK) {
            continue;
        }
        n = rt_device_read(g_uart, -1, buf, sizeof(buf) - 1);
        if (n > 0) {
            buf[n] = '\0';
            rt_kprintf("rx: %s\n", buf);
        }
    }
}
```

回调里只释放信号量，读数据放在线程里，原因与 4.4 讲的一样：中断上下文不能阻塞，也不能做耗时的解析。`rt_device_read` 的第二个参数 `pos` 对字符设备没有意义，固定写 -1。

PIN 设备是另一类常用的设备。板载 LED 挂在 PC13，用 `rt_pin_*` 就能控制（`pin_blink.c`）：

```c
#define LED_PIN GET_PIN(C, 13)

rt_pin_mode(LED_PIN, PIN_MODE_OUTPUT);
for (;;) {
    rt_pin_write(LED_PIN, PIN_HIGH);
    rt_thread_mdelay(500);
    rt_pin_write(LED_PIN, PIN_LOW);
    rt_thread_mdelay(500);
}
```

`GET_PIN` 来自 BSP 的 `drv_gpio.h`，把「端口 + 引脚号」换算成设备框架里的引脚编号。blue-pill BSP 的板载 LED 接在 PB2（低电平点亮），本页按本教程前几章的约定写 PC13，接自己的板子时以实际原理图为准。

## 8. 构建与软件包

RT-Thread 的工程用 Env 工具（Windows 上的 `env.exe` 与命令行，Linux/macOS 上的脚本）管理，流程分三步：

1. **配置**：`menuconfig` 打开 `Kconfig` 里的选项，生成 `rtconfig.h`。选项覆盖内核（优先级数量、节拍频率、是否带堆）、组件（FinSH、DFS、网络）、设备框架（串口、SPI、PIN）与 BSP 专用项。
2. **取依赖**：`pkgs --update` 按 `pkgs/packages.json` 拉取软件包。blue-pill BSP 需要先取 `STM32 HAL` 与 `CMSIS` 两个包，否则编译会因为找不到 `stm32f1xx.h` 失败。
3. **构建**：`scons` 直接编译（用 `rtconfig.py` 里的编译器路径），或 `scons --target=mdk5`、`--target=iar`、`--target=cmake` 生成其他构建系统的工程文件。

本机没有安装 scons，所以上述流程只给出步骤，不贴 scons 的构建输出。本页的示例改用 arm-none-eabi-gcc 直接编译内核源码与示例（v5.3.0 的 `src/`、`libcpu/arm/cortex-m3` 与本节目录下的 `rtconfig.h`、`board.c`），完整步骤在 `code/嵌入式/4_RTOS与固件架构/4_8_RT-Thread/README.md`：先编出 `librtthread.a`，再用本目录的 `link.ld` 与 1.8 的启动文件链接。验证的是「代码能不能编过、链接出来多大」，scons 流程本身没有跑。

RT-Thread Studio 是基于 Eclipse 的图形 IDE，把 Env、构建、下载、调试打包在一起，适合不熟悉命令行的用户。它的界面操作不在本页范围。

## 9. 怎么选

两套 RTOS 都能满足绝大多数中小型嵌入式项目的需求，选择依据主要是工程需求与团队情况：

| 维度 | FreeRTOS | RT-Thread |
| --- | --- | --- |
| 许可 | MIT | Apache-2.0 |
| 内核体积 | 约 19.9 KB 目标代码（Cortex-M3，-O2，全部模块链接进来） | 约 26.9 KB（同上口径，含 klibc 的格式化函数） |
| 组件 | 以内核为主，网络、文件系统靠第三方 | 设备框架、FinSH、DFS、lwIP、MQTT 一体化 |
| 芯片厂支持 | 几乎每家 SDK 都带 FreeRTOS 适配 | STM32、NXP、GD32 等主流厂商有 BSP |
| 调试手段 | 任务表、运行时统计，配合 OpenOCD/SystemView | msh 命令行、`list thread`、`free` |
| 文档语言 | 英文为主 | 中文文档完整 |
| 商业支持 | AWS 提供 | 国内多家公司提供 |
| 认证 | 有安全认证版本（FreeRTOS-MPU、SAFERTOS 等衍生） | 有工业与汽车相关认证案例 |

在面对实际项目选型时，团队通常需要权衡以下几种典型场景与需求导向：

- **聚焦纯粹内核与轻量集成**：当项目强依赖芯片厂商原厂 SDK、已有成熟的外设驱动封装，或需要无缝对接云端 SDK（如 AWS IoT、Azure RTOS 等）时，FreeRTOS 是阻力最小的选择。其内核模型高度克制，全模块编译约在 20 KB 以内，按需剔除流缓冲区与软件定时器后足迹更小，且主流开源库几乎均提供开箱即用的 FreeRTOS 适配层。
- **追求快速交付与平台化组件**：如果产品需要包含交互式命令行 Shell、传感器设备抽象层、虚拟文件系统与各类联网协议栈，且团队立足于国内软硬件生态，RT-Thread 能够显著减少前期搭建设施脚手架的人力投入。开箱即用的驱动模型与组件生态能让工程人员更快聚焦于业务逻辑本身。
- **特定标准与生态诉求**：当硬件具有特殊的安全合规或认证需求，抑或需要严苛的 POSIX 兼容性时，还可以将视野拓展到其他优秀内核——例如配置机制高度模块化的 Zephyr、POSIX 抽象完备的 NuttX，或是具备深厚工业安全认证背书的商业/开源方案（如 ThreadX、μC/OS）。

一个常见的错误是拿两边的「内核体积」直接比。上面两行的数字口径相同——都是「全部模块链接进来」的目标代码，但还是不成比例：两者打开的功能不一样。RT-Thread 的 26.9 KB 是本节用最小 `rtconfig.h` 编出来的内核加 klibc，还没算设备框架、命令行与 BSP 驱动；打开这些之后体积还要涨一截。选择时要按**整个工程要用的功能集合**来比。

## 10. 习题

### 10.1 巩固

<Exercise id="60463" :d="1" :w="2">

`rtconfig.h` 里 `RT_ALIGN_SIZE` 为 4、`RT_TICK_PER_SECOND` 为 1000。读下面两行创建代码，回答问题：

```c
rt_thread_t a = rt_thread_create("a", entry_a, RT_NULL, 512, 5, 5);
rt_thread_t b = rt_thread_create("b", entry_b, RT_NULL, 1024, 2, 10);
rt_thread_startup(a);
rt_thread_startup(b);
```

(1) 两个线程都进入就绪状态之后，谁占 CPU？栈各占多少字节？把 `b` 的栈参数改成与 FreeRTOS 里 `xTaskCreate` 的 256 等价，该写多少？

(2) 最后一个参数 `5` 与 `10` 在什么情况下才起作用？两个线程优先级不同时，`b` 会不会因为时间片更长而多占 CPU？

(3) 把 `b` 的优先级改成 9，会发生什么？本页 `rtconfig.h` 里 `RT_THREAD_PRIORITY_MAX` 是多少，合法范围到哪一位为止？

(4) 漏掉 `rt_thread_startup(a)`，`a` 处于什么状态？`rt_thread_create` 返回非空能说明什么、不能说明什么？

(5) `rt_thread_mdelay(50)` 在本配置下延时多少毫秒？把 `RT_TICK_PER_SECOND` 改成 100，同一个调用延时多少？

</Exercise>

<Exercise id="60464" :d="2" :w="3">

在 `code/嵌入式/4_RTOS与固件架构/4_8_RT-Thread/producer_consumer_rtt.c` 里加一段消息队列的边界实验：

(1) 打印 `sizeof(struct sample)` 与 `rt_mq_create` 传入的 `msg_size`，确认两者相等。不相等时 `rt_mq_send` 会返回什么？

(2) 队列深度为 4。连续发送 5 条且不接收，第 5 次 `rt_mq_send` 的返回值是什么？改用 `rt_mq_send_wait` 并分别给超时 0 与超时 100 个节拍，记录返回值与耗时，说明 `rt_mq_send` 与 `rt_mq_send_wait` 的超时语义差别。

(3) `rt_mq_recv` 的返回值类型是 `rt_ssize_t`。写代码把「收到一条消息」「超时」「其他错误」三种情况分开，打印各自的返回值。构造一次超时，贴出实际数字。

(4) 把 (2) 里的第 5 条消息改为先 `rt_mq_recv` 一条再发送，说明为什么这次能成功，以及发送方被阻塞时接收方是否一定在运行。

按 README 的命令做编译检查，并说明哪些结论只能上板看。

</Exercise>

<Exercise id="60465" :d="2" :w="3">

扩展 `object_model.c`：在 `main` 里创建一个信号量、一个互斥量、一个消息队列，然后用 `rt_object_for_each` 遍历线程、信号量、互斥量、消息队列四类对象，各打印一条计数与对象名。

(1) 写出遍历函数与调用。四类分别用哪个 `RT_Object_Class_*` 常量？

(2) 线程名超过 `RT_NAME_MAX - 1` 个字符时会发生什么？创建一个名为 `"very_long_worker"` 的线程，从打印结果确认截断位置。

(3) 在退出前删除创建的对象，再遍历一次。哪些计数回到了初始值，哪些没有？`rt_thread_delete` 之后线程对象是否立刻从链表上消失（结合空闲线程回收资源的机制回答）。

(4) 遍历过程中创建对象安全吗？说明理由，并给出一个会破坏遍历的做法。

</Exercise>

### 10.2 编程

<Exercise id="60466" :d="3" :w="3">

用事件集实现三路就绪才启动：三个采集线程分别在启动后 20 ms、40 ms、60 ms 置位自己的一位，一个处理线程等待三位全部到齐，记录从启动到齐的节拍差。

(1) 写出完整程序：三个线程共用 `rt_event_send`，处理线程用 `RT_EVENT_FLAG_AND | RT_EVENT_FLAG_CLEAR` 等待。

(2) 处理线程的等待超时设为 200 个节拍。故意让第三个线程不置位，观察 `rt_event_recv` 的返回值，说明超时与成功的返回值怎么区分。

(3) 把 `RT_EVENT_FLAG_CLEAR` 去掉，处理线程循环等待第二轮，会发生什么？用实际输出说明。

(4) 如果两个线程几乎同时置位，`recv` 拿到的 `recv` 值里可能包含哪些位？用它判断「谁先到」可靠吗？

用 README 的命令做编译检查，并在代码里加注释说明每一处参数的含义。

</Exercise>

<Exercise id="60467" :d="3" :w="3">

用邮箱加内存池代替消息队列传大结构体。

(1) 用 `rt_mp_create("blk", 4, sizeof(struct sample))` 建内存池，生产者每次 `rt_mp_alloc` 取一块、填好数据后 `rt_mb_send` 把指针发出去，消费者 `rt_mb_recv` 取到指针、处理完 `rt_mp_free`。写出两个线程。

(2) 先跑通，再用 `rt_mp_alloc` 反复申请不释放，直到返回 `RT_NULL`。记录第几次失败，与池的块数是什么关系？

(3) 另写一个错误版本：生产者把栈上局部结构体的地址用邮箱发出，函数返回后消费者才读。说明读到的是什么，以及为什么编译器不报警。

(4) 比较 (1) 与消息队列的两次拷贝：邮箱方案省掉了哪两次拷贝，代价是什么（提示：所有权与释放责任）。列出三条使用邮箱时必须遵守的约定。

编译检查用 README 里的内核命令，不需要额外的组件开关（`RT_USING_MAILBOX` 与 `RT_USING_MEMPOOL` 已在 `rtconfig.h` 里打开）。

</Exercise>

<Exercise id="60468" :d="3" :w="4">

动手验证自动初始化的执行顺序。

(1) 写一个 `init_order.c`，三个函数分别用 `INIT_BOARD_EXPORT`、`INIT_DEVICE_EXPORT`、`INIT_APP_EXPORT` 导出，每个函数 `rt_kprintf` 打印自己的名字。

(2) 用 `arm-none-eabi-objdump -h` 确认三个函数落在哪几个 `.rti_fn.*` 段，段名里的数字是多少，并对照正文的级别表。

(3) 用 `arm-none-eabi-nm -n` 按地址排序列出 `__rt_init_*` 符号，说明上电后 `rt_components_board_init()` 与 `rt_components_init()` 各会调用哪几个，调用顺序如何。

(4) 两个函数都标 `INIT_APP_EXPORT`，其中一个依赖另一个先执行。给出两种修正办法并各写一行代码；说明为什么「把定义写在前面」不能保证顺序。

</Exercise>

<Exercise id="60469" :d="4" :w="4">

给 msh 增加一个带参数的命令 `mdr_stat`：

- `mdr_stat`：打印采样数与丢弃数；
- `mdr_stat reset`：把两个计数清零；
- `mdr_stat set N`：把采样周期设为 N 毫秒，只接受 1～1000；
- 其他参数：打印用法并返回负值。

(1) 写出函数与 `MSH_CMD_EXPORT`。参数怎么从 `argv` 里取？`argc` 在每种情况下分别是多少？

(2) 字符串比较与整数转换用什么函数？为什么不用标准库的 `strcmp`、`atoi`？

(3) 命令跑在哪个线程里（结合第 6 节的 `tshell`）？在命令里调用 `rt_thread_mdelay(5000)` 会有什么后果？要延时应该怎么办？

(4) 按 README 的 FinSH 编译选项做编译检查，并把 `objdump -h` 里 `FSymTab` 段的大小贴出来，说明它与 `struct finsh_syscall` 的成员数对得上。

</Exercise>

<Exercise id="60470" :d="5" :w="4">

串口按行接收。要求把行组装逻辑写成不依赖 RTOS 的纯函数，能在主机上用 gcc 测试：

```c
/* 输入一段字节，凑满一行（以 '\n' 结尾）时写入 out 并返回 1，否则返回 0 */
int line_feed(struct line_asm *la, const char *data, size_t n, char *out, size_t out_size);
```

(1) 写出 `struct line_asm` 与 `line_feed`。缓冲区溢出时怎么处理？给出一致的策略并写进注释。

(2) 用 `rt_device_set_rx_indicate` 加信号量驱动读取线程，收到通知后 `rt_device_read` 一次最多读 `sizeof(buf)` 字节。说明为什么一次 `read` 可能读不满，以及剩余字节怎么处理。

(3) 在主机上写测试：喂入三段被切开的 `"hello\nwor"`、`"ld\n"`、`"abc"`，期望得到两行 `hello`、`world`，第三段不产生输出。给出 `gcc -std=c17 -Wall -Wextra` 命令与运行结果。

(4) 把 `line_feed` 挪进线程之前，哪些东西必须由线程提供（提示：行缓冲区的归属）？这样拆分对测试和移植各有什么好处？

</Exercise>

<Exercise id="60471" :d="5" :w="4">

把一段 FreeRTOS 程序移植到 RT-Thread，并量化两边的差异：

```c
/* FreeRTOS 原版：计数信号量限流 + 互斥量保护串口 */
static SemaphoreHandle_t g_slots;
static SemaphoreHandle_t g_console;

static void worker(void *p) {
    for (;;) {
        if (xSemaphoreTake(g_slots, portMAX_DELAY) == pdPASS) {
            xSemaphoreTake(g_console, portMAX_DELAY);
            printf("work\n");
            xSemaphoreGive(g_console);
            xSemaphoreGive(g_slots);
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
```

(1) 用 `rt_sem_create`、`rt_mutex_create`、`rt_sem_take`、`rt_sem_release` 重写，包括计数信号量的初值怎么给（提示：`rt_sem_init` 与 `rt_sem_create` 的第二个参数是初值）。

(2) 列一张对照表，至少覆盖：优先级方向、栈单位、延时单位、信号量取用失败的返回值、互斥量能否在中断里释放。每行给出两边的写法。

(3) 两边内核都编译成 Cortex-M3 目标文件，用 `arm-none-eabi-size` 比较同一个示例的文本段大小。说明这个比较能说明什么、不能说明什么（对照 4.8 第 9 节的口径问题）。

(4) 找出一个「看起来一样、语义不同」的 API 对，写一小段代码在两边都跑，用输出证明差异。

(5) 移植过程中最容易漏掉的一步是什么？给出一个会因此静默失败的例子。

</Exercise>

### 10.3 综合与挑战

<Exercise id="60472" :d="6" :w="5" scope="*">

把 4.9 的活动对象框架搬到 RT-Thread 上：每个对象一条邮箱、一个线程、一个处理函数，事件仍用 `{ sig, param }`。

(1) 用 `rt_mb_create`、`rt_thread_create` 实现 `rtt_ao_start`、`rtt_ao_post`、`rtt_ao_post_isr`。邮箱只能传 `rt_ubase_t`，事件结构体怎么放进去？给出所有权规则。

(2) 用信号量或事件集实现 `rtt_ao_post_isr` 的「中断里投递」：RT-Thread 的邮箱有 `rt_mb_send_wait`，中断里该用哪个函数？

(3) 把 4.9 的 LED 状态机接进来，实现按键、心跳、LED 三个对象，四个模拟按下推进四档。写出预期的状态序列，并说明在 RT-Thread 上验证需要什么环境。

(4) 对比两套实现：FreeRTOS 版用队列，RT-Thread 版用邮箱。事件大小、拷贝次数、内存来源、失败返回值各有什么不同？

(5) 给出这套框架在 RT-Thread 上的一处局限：当事件参数必须携带大结构体时，你打算怎么改？至少给出两种方案并说明取舍。

按 README 的命令做编译检查，提交代码与一段说明。

</Exercise>

## 11. 参考资料

1. RT-Thread, [*rt-thread*](https://github.com/RT-Thread/rt-thread)：仓库与 v5.3.0 发布；`src/`、`include/rtdef.h`、`include/rtthread.h` 的 API 定义。
2. RT-Thread, [*内核文档*](https://github.com/RT-Thread/rt-thread/tree/master/documentation/3.kernel)：线程、线程间同步、线程间通信、内核对象四部分。
3. RT-Thread, [*文档中心*](https://www.rt-thread.org/document/site/)：FinSH、设备框架、Env 工具的使用说明。
4. RT-Thread, [*stm32f103-blue-pill BSP*](https://github.com/RT-Thread/rt-thread/tree/master/bsp/stm32/stm32f103-blue-pill)：板级支持包、链接脚本 `link.lds`、`rtconfig.h`。
5. RT-Thread, [*rtdef.h*](https://github.com/RT-Thread/rt-thread/blob/master/include/rtdef.h)：`INIT_EXPORT` 系列宏与 `struct rt_object` 的定义。
6. RT-Thread, [*FinSH 组件文档*](https://github.com/RT-Thread/rt-thread/tree/master/documentation/6.components/finsh)：`list_thread`、`list_device`、`free` 的输出示例。
7. Arm, [*PM0056 Programming manual*](https://www.st.com/resource/en/programming_manual/pm0056-stm32f10xxx20xxx21xxxl1xxxx-cortexm3-programming-manual-stmicroelectronics.pdf)：SysTick 与 Cortex-M3 的异常模型，理解 `board.c` 里的节拍中断。
