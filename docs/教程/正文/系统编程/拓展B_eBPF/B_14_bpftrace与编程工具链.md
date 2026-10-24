# B.14 bpftrace 与编程工具链

生产环境的线上服务往往会在没有征兆的情况下突发性能衰减。在紧急排障的最初十分钟内，运维与开发人员往往急于厘清两项事实：究竟是哪个进程在疯狂打开特定配置文件，以及各进程每秒分别触发了多少次系统调用。倘若完全沿用前文所介绍的 libbpf 工程化流程来回答这两个问题，开发者必须经历初始化源码目录、编写内核态探针、设计用户态载入器、配置 Makefile 构建规则、编译生成骨架头文件等一整套完整周期，耗时往往半小时起步。bpftrace 的核心价值，恰恰在于将这类探索性的观测需求压缩为终端中的一行即时命令，敲下回车即可实时捕捉内核指标。

本篇系统梳理 bpftrace 的底层工作机理与工具链全景：从极简的单行命令模式、脚本化开发范式（`.bt` 脚本）到发行版内置的现成运维工具，并深入剖析其将高级 DSL 编译为 BPF 字节码的编译流水线。随后，视野将进一步拓展至现代 eBPF 的多元开发生态：厘清从早期 BCC 向预编译 libbpf-tools 的架构演进逻辑、掌握 bpftool 在 ELF 目标文件维度的离线调试与静态链接能力、评估 Go（`cilium/ebpf`）与 Rust（`Aya`、`libbpf-rs`）工具链的工程取舍，最后阐明如何在脱离内核运行特权的前提下，将 BPF 代码质量检查无缝嵌入现代持续集成流水线中。

文中的指令与诊断输出均以标准 Linux 发行版环境（基于 Linux 6.12 与 bpftrace 0.23）为基准展开；关于 bpftrace 运行所需的特权要求与权限配置，可参阅[环境准备与工具链](/教程/正文/系统编程/拓展B_eBPF/B_2_环境准备与工具链)。

## 1. bpftrace 是什么

bpftrace 是一门面向临时排查的高级语言（high-level tracing language），语法借鉴了 awk、C 和 DTrace。一个程序由若干块组成，每块写成「探针 /过滤条件/ { 动作 }」，前面还可以放预处理指令和类型定义。它不负责编写可以长期部署的工具，目标是让从想法到结果的距离尽量短。

bpftrace 启动时把整段脚本编译成 BPF 字节码，再交给内核加载和附加。脚本里写下的探针名在这一步就全部解析成具体的钩子，运行期间没有解释器，也没有第二次编译。之后某个探针被事件触发，对应的动作直接在内核里执行：`printf` 产生的数据经缓冲区送到用户态打印，写 `@` 开头的 map 则在退出时统一输出。改一行脚本必须重新运行，这是它与「写一次、编译成二进制、分发出去」的 libbpf 程序最根本的区别。

```mermaid
flowchart LR
    A["foo.bt 或 -e 字符串"] --> B["词法分析与语法分析"]
    B --> C["语义分析与类型检查<br/>BTF 提供结构体布局"]
    C --> D["LLVM IR"]
    D --> E["优化后的 BPF 字节码"]
    E --> F["libbpf 装载<br/>内核验证器检查"]
    F --> G["附加到探针"]
    G --> H["事件触发，执行动作"]
    H --> I["printf 输出或更新 map"]
    I --> J["退出时打印 map"]
```

图中的每一段都可以用 `-d` 选项单独观察。手册里列出的阶段是 `ast`、`codegen`、`codegen-opt`、`dis`、`libbpf`、`verifier`，其中 `dis` 只在调试构建里可用。`--emit-llvm FILE` 会把原始和优化后的 LLVM IR 分别写到 `FILE.original.ll` 与 `FILE.optimized.ll`。这些选项平时用不到，排查「脚本能解析但加载失败」时很有用：先看 `ast`，再看 `verifier` 的日志，就能分清问题出在语法、类型还是验证器。

::: concept bpf14-script-compile-time title="bpftrace 脚本在启动时整段编译"

bpftrace 脚本在执行前整段编译成 BPF 字节码，探针在编译期解析并附加，运行期间不解释、不重新编译。因此脚本的运行开销来自事件处理本身，启动开销则来自编译、加载与验证；改变脚本必须重新运行程序。

:::

::: generate bpf14-script-pipeline kind=explanation concepts=bpf14-script-compile-time

面向第一次使用 bpftrace 的读者，以「一行命令从输入到输出」为主线走读上面的流程图：说明脚本怎样变成 LLVM IR 和 BPF 字节码、BTF 在类型检查里起什么作用、为什么脚本启动要花一两秒、为什么改一个字符就要重跑。不要展开 LLVM 的优化细节，也不要罗列全部调试选项。

:::

## 2. 一行命令

`bpftrace -e` 直接接收一段程序，`-l` 列出探针。官方教程按十二课组织，顺序是先看有哪些探针，再做最简单的输出，然后逐步加入过滤、map、直方图和栈。下面按这个顺序挑出最常用的几条；凡是需要加载进内核才能得到的结果，都照官方教程与自带工具的示例输出转引，写成「输出类似」。探针类型本身怎么选、跟踪点与 kprobe 各有什么代价，见[跟踪与观测：kprobe、uprobe 与 tracepoint](/教程/正文/系统编程/拓展B_eBPF/B_11_跟踪与观测_kprobe_uprobe与tracepoint)；如果连 `clang`、`bpftool` 和第一个 libbpf 程序都还没跑通，先回到[第一个 eBPF 程序](/教程/正文/系统编程/拓展B_eBPF/B_3_第一个eBPF程序)。

### 2.1 先看有哪些探针

列出所有系统调用的进入跟踪点：

::: terminal title="bash - bpftrace -l"

```text
$ sudo bpftrace -l 'tracepoint:syscalls:sys_enter_*'
```

:::

`-l` 接受与探针同名的通配符，`*` 匹配任意字符，`?` 匹配单个字符，输出是每行一个探针名。想不起某个系统调用的跟踪点长什么样，用这个方法查比翻内核源码快。把 `-v` 与 `-l` 一起用，还会列出探针的参数；`openat` 那一条会给出 `int __syscall_nr`、`int dfd`、`const char * filename`、`int flags`、`umode_t mode` 五个字段，正好对应 `openat` 的五个参数。

### 2.2 第一个程序与文件打开

输出类似：

::: terminal title="bash - hello world"

```text
$ sudo bpftrace -e 'BEGIN { printf("hello world\n"); }'
Attaching 1 probe...
hello world
^C
```

:::

`BEGIN` 在脚本启动时执行一次，作用相当于 awk 的 `BEGIN`，常用来打印表头。`printf` 的格式串与 C 相同。

把 `BEGIN` 换成具体的探针，就能看到文件打开，输出类似：

::: terminal title="bash - 跟踪 openat"

```text
$ sudo bpftrace -e 'tracepoint:syscalls:sys_enter_openat { printf("%s %s\n", comm, str(args.filename)); }'
Attaching 1 probe...
snmp-pass /proc/cpuinfo
snmp-pass /proc/stat
snmpd /proc/net/dev
snmpd /proc/net/if_inet6
^C
```

:::

`tracepoint:syscalls:sys_enter_openat` 是静态跟踪点，在 `openat()` 进入时触发。`comm` 是内置对象，保存当前线程的进程名；`args` 是探针参数的集合，由 bpftrace 按跟踪点的格式自动生成；`args.filename` 取其中的 `filename` 字段，它是一个指针，`str()` 把它指向的内存读成字符串。

此处存在一项关键的版本演进细节：在 0.19 之前的旧版本中，访问探针参数采用指针解引用语法 `args->filename`；自 0.19 版本起，`args` 在抽象语义上由指针重构为结构体对象，规范写法变更为 `args.filename`。本文统一遵循 0.23 及更高版本的现代语法。在查阅早期历史脚本时，若发现形如 `->` 的旧语法，按现代规范改写为 `.` 即可无缝运行。

### 2.3 按进程统计系统调用

输出类似：

::: terminal title="bash - 按进程统计系统调用"

```text
$ sudo bpftrace -e 'tracepoint:raw_syscalls:sys_enter { @[comm] = count(); }'
Attaching 1 probe...
^C

@[bpftrace]: 6
@[systemd]: 24
@[snmp-pass]: 96
@[sshd]: 125
```

:::

`@` 开头的是 map，`@[comm]` 以进程名为键，`count()` 每命中一次加一。程序退出时 map 自动打印，按值从小到大排列。这一行命令和 [B.6](/教程/正文/系统编程/拓展B_eBPF/B_6_Map与数据交换) 里那个要写内核态程序、用户态加载器、Makefile 的 syscount 是同一件事；区别只在于 bpftrace 把它压缩成了一行，代价是不能带命令行参数、不能分发给别人、每次运行都要重新编译。两种写法的并排对照在 4.3 节。

冒号后面的键可以换成 `pid`、`probe`，也可以只写 `@`（不带键），后者把所有事件汇总成一个数。

### 2.4 直方图：read 的大小与耗时

统计某个进程的 `read` 返回值分布，输出类似：

::: terminal title="bash - read 字节数的 2 的幂直方图"

```text
$ sudo bpftrace -e 'tracepoint:syscalls:sys_exit_read /pid == 18644/ { @bytes = hist(args.ret); }'
Attaching 1 probe...
^C

@bytes:
[0, 1]                12 |@@@@@@@@@@@@@@@@@@@@                                |
[2, 4)                18 |@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@                     |
[4, 8)                 0 |                                                    |
[8, 16)                0 |                                                    |
[16, 32)               0 |                                                    |
[32, 64)              30 |@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@|
[64, 128)             19 |@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@                    |
[128, 256)             1 |@
```

:::

`/pid == 18644/` 是过滤条件，只有条件成立时动作才执行。`hist()` 按 2 的幂分桶，方括号里的区间左闭右开：`[128, 256)` 表示取值不小于 128 且小于 256。随后的数字是落在这个桶里的事件数，再右边是用 `@` 画的条形图。多峰分布在这种图上很直观，比如小读、大读混在一起时会出现两个峰。

要按固定宽度分桶，用 `lhist()`，输出类似：

::: terminal title="bash - read 字节数的线性直方图"

```text
$ sudo bpftrace -e 'kretprobe:vfs_read { @bytes = lhist(retval, 0, 2000, 200); }'
Attaching 1 probe...
^C

@bytes:
(...,0]                0 |                                                    |
[0, 200)              66 |@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@|
[200, 400)             2 |@                                                   |
[400, 600)             3 |@@                                                  |
[600, 800)             0 |                                                    |
[800, 1000)            5 |@@@                                                 |
[1000, 1200)           0 |                                                    |
[1200, 1400)           0 |                                                    |
[1400, 1600)           0 |                                                    |
[1600, 1800)           0 |                                                    |
[1800, 2000)           0 |                                                    |
[2000,...)            39 |@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@                      |
```

:::

`lhist(值, 最小值, 最大值, 步长)` 在 `[0, 2000)` 内按 200 一档分桶，区间外的分别归入第一行和最后一行。`kretprobe:vfs_read` 是函数返回探针，`retval` 是返回值，也就是实际读到的字节数。kprobe 与 kretprobe 跟踪的是内核函数，函数名、参数和返回值都可能随内核版本变化，这是它不如跟踪点稳定的地方：换一个内核，`vfs_read` 可能被改名、被拆分，或者干脆消失，脚本就挂不上了。写法上先用 `bpftrace -l 'kretprobe:vfs_read'` 确认函数存在；要长期使用，换成 `tracepoint:syscalls:sys_exit_read` 更稳，代价是拿不到 VFS 层与系统调用层之间的差别。

### 2.5 采样内核栈

输出类似：

::: terminal title="bash - 采样内核栈"

```text
$ sudo bpftrace -e 'profile:hz:99 { @[kstack] = count(); }'
Attaching 1 probe...
^C

[...]
@[
filemap_map_pages+181
__handle_mm_fault+2905
handle_mm_fault+250
__do_page_fault+599
async_page_fault+69
]: 12
[...]
@[
cpuidle_enter_state+164
do_idle+390
cpu_startup_entry+111
start_secondary+423
secondary_startup_64+165
]: 22122
```

:::

`profile:hz:99` 在每颗 CPU 上以 99 Hz 采样。100 Hz 这样的整频率容易和系统里其他定时活动对齐，采样会一直落在同一个函数上，结果有偏；99 与常见的周期不整除。`kstack` 是当前的内核调用栈，把它当作 map 的键就得到按栈汇总的采样计数。这段输出最适合直接喂给火焰图。

`profile` 与 `interval` 的区别值得记一下：`profile` 在所有 CPU 上按频率触发，适合采样；`interval:s:5` 只在一颗 CPU 上按固定间隔触发，适合定时打印和 `exit()`。

## 3. 语言要点

### 3.1 程序结构

一个动作块写成「探针 /过滤条件/ { 动作 }」。探针可以写多个，用逗号分隔；过滤条件可选，也可以写 `&&`、`||` 组合；动作里是多条以分号分隔的语句。

```text
tracepoint:syscalls:sys_enter_open,
tracepoint:syscalls:sys_enter_openat
/comm == "nginx"/
{
    printf("%s %s\n", probe, str(args.filename));
}
```

脚本头部可以声明 `#include` 头文件、自定义数据类型定义以及顶部的 `config = { ... }` 配置块。config 块必须置于脚本最顶层，用于调优 `max_strlen`（字符串最大截取长度）、`stack_mode`（堆栈解析模式）等运行时参数；若宿主环境中存在同名的系统环境项（如 `BPFTRACE_MAX_STRLEN`），则其配置优先级高于脚本内部声明。

### 3.2 内置对象

| 名字 | 含义 |
| --- | --- |
| `pid`、`tid` | 当前线程所属的进程 ID、线程 ID，都按 init 命名空间显示 |
| `comm` | 当前线程的名字，最长 16 字节 |
| `nsecs` | 开机以来的纳秒数，用来计算时间差 |
| `kstack`、`ustack` | 内核栈、用户态栈，可以带参数控制层数 |
| `args` | 探针参数的结构体，只在跟踪点、fentry/fexit 和带 DWARF 的 uprobe 里有 |
| `retval` | 返回探针的返回值 |
| `func` | 当前被跟踪的函数名，kprobe 与 uprobe 可用 |
| `curtask` | 当前任务的 `struct task_struct` 指针，读内核字段时用 |
| `probe` | 当前探针的完整名字，多探针合并到一个动作块时用来区分 |

### 3.3 临时对象与 map

`$` 开头的名字是临时对象，只存在于当前动作块，存在 BPF 栈上，块结束就消失。`@` 开头的名字是 map，生命周期与整个 bpftrace 进程相同，退出时自动打印。两者都不需要声明，第一次赋值时定下类型，之后不能改。

map 最常见的用法是入口出口配对。下面这条命令测 `vfs_read` 的耗时，输出类似：

::: terminal title="bash - 测量 vfs_read 的耗时"

```text
$ sudo bpftrace -e 'kprobe:vfs_read { @start[tid] = nsecs; } kretprobe:vfs_read /@start[tid]/ { @ns[comm] = hist(nsecs - @start[tid]); delete(@start, tid); }'
Attaching 2 probes...

[...]
@ns[snmp-pass]:
[0, 1]                 0 |                                                    |
[...]
[256, 512)            27 |@@@@@@@@@                                           |
[512, 1k)            125 |@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@       |
[1k, 2k)              22 |@@@@@@@                                             |
[...]
[32k, 64k)           144 |@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@|
[64k, 128k)            7 |@@                                                  |
[128k, 256k)          28 |@@@@@@@@@@                                          |
[...]
```

:::

入口处把当前时刻存进以线程 ID 为键的 map，出口处取出、相减、再删掉。用 `tid` 作键，是因为一个进程的多个线程可以同时卡在不同的 `read` 上，只有线程才能把入口和出口一一对应起来。出口处的过滤 `/@start[tid]/` 保证只在入口确实记录过时间时才计算，否则在程序启动前已经在进行的 `read` 会算出一段虚假的耗时。

`delete(@map, 键)` 删除一个元素。0.22 起键作为单独的参数传入，旧写法 `delete(@start[tid])` 仍可用但已废弃。`clear(@map)` 清空整个 map，`print(@map, N)` 只打印值最大的前 N 项。

### 3.4 聚合函数

| 函数 | 作用 |
| --- | --- |
| `count()` | 计数 |
| `sum(n)` | 求和 |
| `avg(n)` | 求平均 |
| `min(n)`、`max(n)` | 最小值、最大值 |
| `hist(n)` | 2 的幂直方图，适合看分布的形状 |
| `lhist(n, min, max, step)` | 固定宽度直方图，适合看落在范围内的比例 |
| `stats(n)` | 同时给出次数、平均值与总和 |

`stats()` 的输出类似（照手册示例）：

::: terminal title="bash - stats 的输出"

```text
@bytes[sleep]: count 5, average 832, total 4160
```

:::

需要「这个进程平均每次读多少字节」时用 `stats()` 比 `hist()` 直接；需要看「有多少次是 4 KiB 以下」时用 `hist()`。两者不冲突，可以开两个 map 同时收集。

### 3.5 探针与 libbpf 节名的对应

bpftrace 的探针类型和手写 libbpf 程序时 `SEC()` 里的节名是一一对应的，只是写法不同。把两者对照起来看，bpftrace 的脚本就不再是黑盒。

| bpftrace | libbpf 的 `SEC()` | 程序类型 |
| --- | --- | --- |
| `kprobe:fn` | `SEC("kprobe/fn")` | `BPF_PROG_TYPE_KPROBE` |
| `kretprobe:fn` | `SEC("kretprobe/fn")` | `BPF_PROG_TYPE_KPROBE` |
| `fentry:fn` | `SEC("fentry/fn")` | `BPF_PROG_TYPE_TRACING` |
| `fexit:fn` | `SEC("fexit/fn")` | `BPF_PROG_TYPE_TRACING` |
| `tracepoint:a:b` | `SEC("tracepoint/a/b")` | `BPF_PROG_TYPE_TRACEPOINT` |
| `rawtracepoint:x` | `SEC("raw_tracepoint/x")` | `BPF_PROG_TYPE_RAW_TRACEPOINT` |
| `uprobe:/path:fn` | `SEC("uprobe//path:fn")` | `BPF_PROG_TYPE_KPROBE` |
| `profile:hz:99`、`interval:s:1` | `SEC("perf_event")` | `BPF_PROG_TYPE_PERF_EVENT` |

`fentry`/`fexit` 这两个名字是 0.20 才有的，之前叫 `kfunc`/`kretfunc`。改名的原因写在手册里：避免与内核自己的 kfunc（内核函数）混淆。旧名字仍然可用，新脚本一律写 `fentry`/`fexit`。

`BEGIN` 与 `END` 是特殊探针，由 bpftrace 自己在启动和退出时各触发一次，不对应某个被跟踪的内核函数。`END` 执行完之后 map 仍会自动打印；要在退出前清掉某个 map，在 `END` 里写 `clear(@name)`。

## 4. 脚本与现成工具

### 4.1 写一个 .bt 脚本

一行命令放不下的逻辑写成文件，扩展名 `.bt`，第一行写 `#!/usr/bin/env bpftrace` 就可以直接执行。下面这个脚本来自 bpftrace 仓库的 `tools/vfsstat.bt`，任务是每秒打印一次几类 VFS 调用的次数：

```bash
#!/usr/bin/env bpftrace
BEGIN
{
    printf("Tracing key VFS calls... Hit Ctrl-C to end.\n");
}

kprobe:vfs_read*,
kprobe:vfs_write*,
kprobe:vfs_fsync,
kprobe:vfs_open,
kprobe:vfs_create
{
    @[func] = count();
}

interval:s:1
{
    time();
    print(@);
    clear(@);
}

END
{
    clear(@);
}
```

它用了三种块：`BEGIN` 打表头，探测块按函数名计数（`vfs_read*` 这样的通配符会展开成多个探针，`func` 内置对象区分具体是哪一个），`interval:s:1` 每秒把 map 打印一次再清空。输出类似：

::: terminal title="bash - sudo ./vfsstat.bt"

```text
$ sudo ./vfsstat.bt
Attaching 8 probes...
Tracing key VFS calls... Hit Ctrl-C to end.
21:30:38
@[vfs_write]: 1274
@[vfs_open]: 8675
@[vfs_read]: 11515

21:30:39
@[vfs_write]: 1155
@[vfs_open]: 8077
@[vfs_read]: 10398
^C
```

:::

注意 `interval` 里的 `print` 与 `clear` 是一对：不清空的话，下一个周期打印的是累计值，看不出每秒的变化。这类脚本比一行命令多出来的东西只有表头、定时输出和退出清理，语言本身没有新增概念。

### 4.2 自带的工具

bpftrace 源码树里有一个 `tools` 目录，维护了一批经过验证的脚本，每个都配有示例输出。Debian 13 的 `bpftrace` 包把它们装在 `/usr/sbin/` 下，可以直接运行。常用的几个：

| 工具 | 用途 |
| --- | --- |
| `opensnoop.bt` | 跟踪 `open`/`openat`，打印进程、文件描述符、错误码与路径 |
| `execsnoop.bt` | 跟踪 `exec`，打印新进程的时间、PID、父进程与参数 |
| `biolatency.bt` | 块 I/O 延迟的直方图 |
| `syscount.bt` | 按系统调用号和进程名统计次数 |
| `vfsstat.bt` | 每秒汇总几类 VFS 调用 |
| `tcpconnect.bt`、`tcpaccept.bt` | 主动、被动建立 TCP 连接 |

`opensnoop.bt` 的输出类似：

::: terminal title="bash - sudo opensnoop.bt（节选）"

```text
$ sudo opensnoop.bt
Attaching 3 probes...
Tracing open syscalls... Hit Ctrl-C to end.
PID    COMM               FD ERR PATH
2440   snmp-pass           4   0 /proc/cpuinfo
2440   snmp-pass           4   0 /proc/stat
25706  ls                  3   0 /etc/ld.so.cache
25706  ls                  3   0 /lib/x86_64-linux-gnu/libselinux.so.1
25706  ls                  3   0 /lib/x86_64-linux-gnu/libc.so.6
1744   snmpd               8   0 /proc/net/dev
1744   snmpd              -1   2 /sys/class/net/lo/device/vendor
```

:::

`FD` 为 -1、`ERR` 为 2 的那一行，是 `snmpd` 试图打开一个不存在的文件，错误码 2 就是 `ENOENT`。在应用启动时运行这个工具，能一次看清它读了哪些配置、找了哪些插件目录，比翻源码快得多。

`execsnoop.bt` 的输出类似：

::: terminal title="bash - sudo execsnoop.bt（节选）"

```text
$ sudo execsnoop.bt
Attaching 3 probes...
TIME            PID     PPID    ARGS
08:57:52.430193 3187374 1971701 ls --color --color=auto -lh
08:57:52.441868 3187378 3187375 man ls
08:57:52.473565 3187384 3187378 preconv -e UTF-8
08:57:52.476811 3187391 3187389 grotty
```

:::

看 `PPID` 这一列：`man` 的父进程是发起命令的 shell，`preconv`、`tbl`、`nroff` 的父进程都是 `man`。`man` 本身只负责调度，实际渲染手册页的是一串短命进程，`ps` 很难抓到它们，`execsnoop` 能。这类「谁启动了谁」的问题，用 `comm` 直接读进程名很省事，代价是同名的进程会合并到一行——把键换成 `pid` 就能分开。

工具目录中还收录了包括 `biolatency.bt`、`vfsstat.bt`、`tcpconnect.bt` 等在内的数十个开箱即用的实用脚本，其覆盖领域广泛，但所依赖的语法范式与前文介绍的规则完全一致。

### 4.3 什么时候离开 bpftrace

一行命令能做的事有明确的限度：它不接收命令行参数（只支持 `$1`、`$2` 这类位置参数），不能长期驻留，不能打包成不依赖 bpftrace 的二进制。要给同事一个 `opensnoop -p 1234` 这样带选项、能持续运行、能装进容器镜像的工具，就该换成 libbpf 程序。同一个需求的两种写法并排放着，差别一目了然：

::: compare left-title="临时看一眼：一行命令，不能分发" right-title="要给别人用：libbpf 程序的一角"

```bash
sudo bpftrace -e '
tracepoint:syscalls:sys_enter_openat
{
    @[comm] = count();
}'
```

---

```c
SEC("tracepoint/syscalls/sys_enter_openat")
int count_openat(void *ctx) {
    char comm[16];
    __u64 one = 1;
    __u64 *n;

    bpf_get_current_comm(comm, sizeof(comm));
    n = bpf_map_lookup_elem(&opens, comm);
    if (n != 0) {
        __sync_fetch_and_add(n, 1);
        return 0;
    }
    bpf_map_update_elem(&opens, comm, &one, BPF_ANY);
    return 0;
}
```

:::

右边只是内核态的那一半。完整程序还要一份 `openstat.c` 负责加载、附加、遍历 map，以及一个 Makefile 生成 `vmlinux.h` 和骨架，全部放在 `code/系统编程/拓展B_eBPF/B_14_bpftrace与编程工具链/`。左边一行命令做完的事，右边要做一天；多出来的工作是分发和长期运行的代价，值得付的时候才付。

::: tip 先用 `-c` 限定子进程

`bpftrace -c './myprog'` 会启动指定的程序，等它退出时 bpftrace 也退出。配合 `/pid == cpid/` 过滤，就只跟踪这一个子进程，看不到系统里其他进程的噪声。排查单个程序的启动过程时，这比按进程名过滤可靠。

:::

<Quiz
  type="single"
  :options="[
    { label: '把所有进程的系统调用总数加起来', value: 'A' },
    { label: '以进程名为键，统计每个进程命中探针的次数', value: 'B' },
    { label: '统计每个进程打开的文件数量', value: 'C' },
    { label: '按 PID 排序打印系统调用', value: 'D' }
  ]"
  answer="B"
>

<template #question>

`tracepoint:raw_syscalls:sys_enter { @[comm] = count(); }` 这条命令在做什么？

</template>

正确答案是 **B**。`@[comm]` 是 map，方括号里是键，这里用进程名作键；`count()` 是聚合函数，每命中一次加一。输出里的 `@[sshd]: 125` 表示跟踪期间 `sshd` 发出了 125 次系统调用。A 少了分组，C 统计的是系统调用，没有统计文件，D 与排序无关（map 按值大小输出）。

</Quiz>

## 5. BCC 与 libbpf-tools

### 5.1 BCC

BCC（BPF Compiler Collection）是最早的一套 BPF 工具集，工具用 Python 写前端，把一段嵌在字符串里的 C 代码在运行时交给 clang 编译，再加载进内核。它解决的问题是：让不熟悉 BPF 的人也能写出可用的观测工具。`opensnoop`、`execsnoop`、`biolatency` 这些名字都来自 BCC。

代价在运行时。每台要运行 BCC 工具的机器都得装上 clang、LLVM 和与当前内核匹配的头文件，Brendan Gregg 在 2020 年的文章里给出的数字是这套依赖可以占 100 MB 以上的存储；BCC 工具本身的 Python 文件很小，负担主要来自那套编译环境。他还写了另一句更关键的话：用 BCC Python 编写性能工具已经被视为过时做法，方向是带 BTF 和 CO-RE 的 libbpf C。

### 5.2 libbpf-tools

libbpf-tools 是把 BCC 里那批工具用 libbpf 重写一遍的产物。每个工具是一个独立的小二进制，BPF 字节码在发布前就编译好并嵌在里面，运行时只做 CO-RE 重定位，不需要 clang，也不需要内核头文件。同样在 Gregg 的文章里，去掉符号的 libbpf 工具平均约 229 KB，`opensnoop` 去掉符号后是 151 KB。

Debian 13 直接提供了 `libbpf-tools` 包（版本 0.31.0），装好之后 `opensnoop`、`execsnoop`、`biolatency` 这些命令就有 CO-RE 版本可用，输出格式与 BCC 版基本一致，通常还多几个选项。

对比之下，bpftrace 自己仍然依赖 LLVM：Debian 的 `bpftrace` 包依赖 `libclang1-19` 和 `libllvm19`，因为它的语法必须在运行时现场编译。三者是三种不同的取舍，各有各的适用场合：

| 方案 | 内核态写法 | 运行时依赖 | 适合 |
| --- | --- | --- | --- |
| BCC | C 字符串，运行时编译 | clang、LLVM、内核头文件 | 维护老工具；需要 Python 生态 |
| libbpf-tools | 预编译的 CO-RE 字节码 | 几乎没有 | 直接用现成工具观测 |
| bpftrace | 自己的脚本语言，运行时编译 | LLVM、libbpf | 临时排查、探索性分析 |
| 自写 libbpf 程序 | C，编译期生成字节码 | libbpf 一个库 | 要参数、要分发、要长期运行 |

### 5.3 怎么选

作者的经验是两条：手上要用的功能已经有 libbpf-tools 版本，就直接用它，不要为了「可定制」去重写；需要写新工具时，先在 bpftrace 里把逻辑跑通，确认答案对得上，再决定要不要换成 libbpf。换的判据是这东西要不要交给别人、要不要在无人值守的环境里跑上几个月。要，就换；不要，留在 bpftrace 里。

<Quiz
  type="single"
  :options="[
    { label: '写成 BCC 的 Python 工具，交给运维运行', value: 'A' },
    { label: '写一个 .bt 脚本，要求每次运行前装好 bpftrace', value: 'B' },
    { label: '用 libbpf 写内核态与用户态两半程序，编译成单个二进制', value: 'C' },
    { label: '做成一个只用 bpftrace -e 的 shell 函数', value: 'D' }
  ]"
  answer="C"
>

<template #question>

要给同事一个带 `-p PID`、`-x 只看失败` 选项，能持续运行几周并装进容器镜像的 `opensnoop` 类工具，选哪种做法？

</template>

正确答案是 **C**。命令行解析、长期驻留、分发包这三件事都超出 bpftrace 的范围：它只支持 `$1` 这类位置参数，脚本每次启动都要编译，运行依赖整个 bpftrace 与 LLVM。A 把依赖推给了运行端，正是 BCC Python 已被视为过时做法的那条路；B 和 D 都要求目标机器装好 bpftrace。

</Quiz>

## 6. bpftool 作为调试工具

bpftool 是内核 BPF 子系统的官方命令行前端，既能查看已经加载的程序和 map，也能在编译阶段处理目标文件。这一节的重点是后者：一台机器上如果没有 root、不能加载程序，bpftool 仍然能检查目标文件的结构，这正是持续集成里最有用的一类检查。

### 6.1 子命令总览

::: terminal title="bash - bpftool help"

```text
$ bpftool help
Usage: bpftool [OPTIONS] OBJECT { COMMAND | help }
       bpftool batch file FILE
       bpftool version

       OBJECT := { prog | map | link | cgroup | perf | net | feature | btf | gen | struct_ops | iter }
       OPTIONS := { {-j|--json} [{-p|--pretty}] | {-d|--debug} |
                    {-V|--version} }
```

:::

按用途分四组：

- `prog`：`show` 列出已加载的程序，`dump xlated` 打印验证器处理后的指令，`dump jited` 打印 JIT 之后的本机指令，`profile` 采集运行统计，`tracelog` 读跟踪输出，`pin` 把程序固定到 bpffs，`attach`、`detach` 手工附加，`run` 用 `BPF_PROG_TEST_RUN` 喂数据。
- `map`：`show`、`dump`、`lookup`、`update`、`delete`、`pin`、`freeze`。调试时最常用的是 `dump` 看内容、`update` 改一个值试效果。
- `gen`：`skeleton` 生成骨架头文件，`object` 把多个目标文件链接成一个，`min_core_btf` 裁剪 BTF，`subskeleton` 生成子骨架。
- `btf`、`feature`、`link`、`cgroup`、`net`：查类型信息、探测内核支持哪些特性、查看 link 与 cgroup 上的附加关系。

### 6.2 查看加载后的程序与 map

`bpftool prog show` 的输出每行一个程序，字段包括 ID、类型、名字、标签、是否 GPL 兼容、加载时间与加载者的 UID、`xlated` 与 `jited` 的字节数、`memlock` 占用、已固定的路径，以及使用它的进程 PID。拿到 ID 之后，`bpftool prog dump xlated id N` 打印验证器改写过的指令，`bpftool prog dump jited id N` 打印 JIT 之后在 CPU 上执行的机器码。前者在携带 BTF 信息时会附带展示源码行号（`linum`），后者则能清晰呈现 JIT 编译生成的机器指令细节。由于这两条指令必须对已经加载至内核的活跃程序发起审查，需要持有 root 特权方可执行。

`bpftool prog tracelog` 读的是 `bpf_trace_printk` 的缓冲区，作用和读 `/sys/kernel/tracing/trace_pipe` 一样。`bpftool prog profile` 需要 perf 事件支持，用来给一个程序采集周期数、指令数、缓存未命中这类指标，同样要有 root 和已加载的程序。

### 6.3 生成与链接目标文件

`bpftool gen` 这一组不需要 root，也不需要内核配合，只要有目标文件就能跑。先看 `gen` 的帮助：

::: terminal title="bash - bpftool gen help"

```text
$ bpftool gen help
Usage: bpftool gen object OUTPUT_FILE INPUT_FILE [INPUT_FILE...]
       bpftool gen skeleton FILE [name OBJECT_NAME]
       bpftool gen subskeleton FILE [name OBJECT_NAME]
       bpftool gen min_core_btf INPUT OUTPUT OBJECT [OBJECT...]
       bpftool gen help

       OPTIONS := { {-j|--json} [{-p|--pretty}] | {-d|--debug} |
                    {-L|--use-loader} }
```

:::

`gen skeleton` 读一个 `.bpf.o`，生成一个头文件，里面有 `struct openstat_bpf`、`openstat_bpf__open()`、`openstat_bpf__load()`、`openstat_bpf__attach()` 等函数，以及每个 map、每个程序、每条 link 的指针。生成的骨架开头是这样的：

```c
/* SPDX-License-Identifier: (LGPL-2.1 OR BSD-2-Clause) */

/* THIS FILE IS AUTOGENERATED BY BPFTOOL! */
#ifndef __OPENSTAT_BPF_SKEL_H__
#define __OPENSTAT_BPF_SKEL_H__

#include <errno.h>
#include <stdlib.h>
#include <bpf/libbpf.h>

#define BPF_SKEL_SUPPORTS_MAP_AUTO_ATTACH 1

struct openstat_bpf {
    struct bpf_object_skeleton *skeleton;
    struct bpf_object *obj;
    struct {
        struct bpf_map *opens;
    } maps;
    struct {
        struct bpf_program *count_openat;
    } progs;
    struct {
        struct bpf_link *count_openat;
    } links;
```

骨架把目标文件的内容以字节数组的形式嵌进头文件，用户态程序因此不需要在运行时找 `.bpf.o`，只拷一个二进制就能分发。`gen skeleton` 需要目标文件里带 BTF，没有 BTF 的目标文件直接被拒。

::: terminal title="bash - 缺 BTF 时生成骨架失败"

```text
$ bpftool gen skeleton openstat_nobtf.bpf.o > /dev/null
libbpf: BTF is required, but is missing or corrupted.
Error: failed to open BPF object file: No such file or directory
```

:::

这里要纠正一个常见的误解：`gen skeleton` 能失败的地方只有目标文件本身（缺 BTF、格式不对），它不检查节名是不是 libbpf 认识的合法类型。目标文件里写 `SEC("tracepointX/syscalls/sys_enter_openat")`，骨架照生成不误，要到加载时才由 libbpf 报出来：

::: terminal title="bash - 节名不认识时的调试输出"

```text
$ bpftool -d gen skeleton bad_section.bpf.o > /dev/null
libbpf: prog 'count_openat': unrecognized ELF section name 'tracepointX/syscalls/sys_enter_openat'
```

:::

这条只是 `-d` 的调试输出。加载时的警告写作 `prog 'count_openat': missing BPF prog type, check ELF section name '...'`，意思是一样的。检查节名的办法是编译之后用 `llvm-objdump -h` 看一眼节列表，或者干脆在 CI 里做一次加载测试。

`gen object` 是模块化构建中的核心工具：它能够在用户空间将多个独立的 `.bpf.o` 静态链接为单一目标文件。这种模式允许开发者依据业务功能将庞大的观测逻辑拆解到不同的 `.bpf.c` 源码单元中分别编译，再通过静态链接融为一体，用户态程序则依然只需面向单一骨架头文件进行交互。下述示例将分别采集 `openat` 与系统调用计数的两个模块进行合并构建：

::: terminal title="bash - 链接两个目标文件并生成骨架"

```text
$ bpftool gen object openstat.combined.bpf.o openstat.bpf.o syscallstat.bpf.o
$ ls -l *.bpf.o
-rw-r--r-- 1 minsecrus minsecrus 5800 openstat.bpf.o
-rw-r--r-- 1 minsecrus minsecrus 5824 syscallstat.bpf.o
-rw-r--r-- 1 minsecrus minsecrus 3536 openstat.combined.bpf.o
$ bpftool gen skeleton openstat.combined.bpf.o > combined.skel.h
$ grep -A 2 'struct bpf_map \*' combined.skel.h
        struct bpf_map *opens;
        struct bpf_map *syscalls;
    } maps;
    struct {
```

:::

合并后的文件比两个输入加起来小得多，因为调试信息被去掉了，BTF 也合并成了一份。对它生成骨架，两个程序和两个 map 都在。

再用 `btf dump` 看合并后的类型信息，只保留 `FUNC`、`VAR`、`DATASEC` 三类：

::: terminal title="bash - 合并后的 BTF"

```text
$ bpftool btf dump file openstat.combined.bpf.o | grep -E 'FUNC|VAR|DATASEC'
[14] VAR 'opens' type_id=13, linkage=global
[16] FUNC_PROTO '(anon)' ret_type_id=2 vlen=1
[17] FUNC 'count_openat' type_id=16 linkage=global
[19] VAR 'LICENSE' type_id=18, linkage=global
[20] VAR 'syscalls' type_id=13, linkage=global
[21] FUNC 'count_syscall' type_id=16 linkage=global
[22] DATASEC 'license' size=13 vlen=1
        type_id=19 offset=0 size=13 (VAR 'LICENSE')
[23] DATASEC '.maps' size=64 vlen=2
        type_id=14 offset=0 size=32 (VAR 'opens')
        type_id=20 offset=32 size=32 (VAR 'syscalls')
```

:::

`DATASEC '.maps'` 里两个 map 的偏移是 0 和 32，说明链接器把它们排进了同一个节。`DATASEC 'license'` 说明合并后的目标文件只有一个许可证，两个输入必须给出相同的许可证，否则链接会失败：

::: terminal title="bash - 许可证不一致"

```text
$ bpftool gen object lic.bpf.o a.bpf.o b.bpf.o
libbpf: non-identical contents of section 'license' are not supported
Error: failed to link 'b.bpf.o': Invalid argument (22)
```

:::

链接失败最常见的原因是符号重名。两个目标文件里各有一个同名的全局函数或全局对象时，`gen object` 直接报冲突：

::: terminal title="bash - 链接同名符号失败"

```text
$ bpftool gen object dup.bpf.o dup_a.bpf.o dup_b.bpf.o
libbpf: conflicting non-weak symbol #10 (count_openat) definition in 'dup_b.bpf.o'
Error: failed to link 'dup_b.bpf.o': Invalid argument (22)
```

:::

`non-weak` 说的是这个符号不是弱符号，两个强定义不能共存。解决办法有两种：把其中一个函数改名，或者给其中一个加 `__attribute__((weak))`，让链接器挑一个。内核态程序不吃链接器的符号解析那一套，`weak` 在这里的实际效果只是让链接通过；函数名冲突要在源码层面解决。

## 7. 其他语言的工具链

内核态程序受验证器和指令集的限制，绝大多数情况下仍然是 C。变化发生在用户态：加载、附加、读 map、处理事件这一半，可以用目标语言自己的库来写。

### 7.1 Go

`cilium/ebpf` 是 Go 生态里事实上的标准库。它完全不依赖 C 和 libbpf，自己实现了 ELF 解析、BTF 处理和系统调用封装，编出来的程序是一个静态的 Go 二进制，适合放进容器镜像。内核态部分仍用 C 写，`bpf2go` 负责调用 clang 编译并生成 Go 绑定：

```go
//go:generate go tool bpf2go -tags linux counter counter.c
```

执行 `go generate` 之后会多出 `counter_bpfel.o`、`counter_bpfeb.o` 和对应的两个 `.go` 文件，小端和大端各一套，目标文件以字节数组的形式嵌进 Go 源码。生成的绑定提供 `counterPrograms.CountPackets` 这样的类型安全接口，读 map 不用手工算偏移。

### 7.2 Rust

Rust 有两条路线，区别在**内核态用什么写**。

`libbpf-rs` 是 libbpf 的 Rust 封装，思路与 Go 的 `cilium/ebpf` 类似：内核态仍是 `.bpf.c`，用 clang 编译，`libbpf-cargo` 把编译步骤接进 Cargo；用户态用 Rust 调用 libbpf，加载和附加的语义与 C 程序完全一致。

`Aya` 走得更远，整套库用 Rust 从零实现，不依赖 libbpf 和 BCC，连 C 工具链都不需要。内核态程序也用 Rust 写，编译成 BPF 目标文件。BTF 在支持的内核上自动启用，程序因此可以跨内核版本运行。这条路适合整个团队都在写 Rust、并且不想在构建里引入 C 的情况；代价是遇到问题时能参考的资料比 libbpf 少。

### 7.3 选择

- 项目主体是 Go，观测代码要嵌进服务里：`cilium/ebpf`，构建链最简单，产物是单个二进制。
- 项目主体是 Rust，愿意让内核态也用 Rust：`Aya`。
- 项目主体是 Rust，但内核态想沿用 C 和 libbpf 生态：`libbpf-rs`。
- 团队写 C，或者要和已有的 libbpf 工具共用代码：直接用 libbpf，不要为了语言统一绕一圈。

语言在选型里排在最后。三套库最终都调用同样的 `bpf()` 系统调用，程序能不能加载、验证器是否放行，与用哪种语言写用户态无关。

## 8. 在持续集成中检查 BPF 代码

BPF 代码的检查可以分成两层：不需要任何特权就能做的，和需要 root、虚拟机或特定内核才能做的。第一层应该放进每次提交，第二层放进夜间任务或发布前的流水线。

### 8.1 不需要 root 的检查

这一层的四件事，全部在普通用户下完成：

1. 编译每个内核态程序。`clang -g -O2 -target bpf -mcpu=v3 -D__TARGET_ARCH_x86 -I. -Wall -Wextra -c x.bpf.c`。Makefile 里已经开了这两个警告选项，CI 上还可以再加 `-Werror`，把警告变成失败。这一步能发现语法错误、类型错误、忘了 `(void)ctx` 之类的未使用参数。
2. 为每个目标文件生成骨架。`bpftool gen skeleton x.bpf.o > x.skel.h`，能发现目标文件里缺 BTF（例如忘了 `-g`）。
3. 用 `bpftool gen object` 把要合并的目标文件链接一遍。这一步能发现跨文件的重复符号，也会顺带验证 BTF 能被解析。
4. 编译用户态程序，或者至少做一次语法检查。`cc -std=c17 -Wall -Wextra -fsyntax-only x.c` 只要装了 libbpf 的头文件就能跑，不需要能加载程序的权限，也不需要链接 libbpf。

上述四项静态验证步骤完整体现在配套的 `check.sh` 脚本中：

```bash
#!/bin/sh
# 不需要 root 的检查：编译内核态程序、生成骨架、链接目标文件、用户态语法检查。
set -e

make clean
make openstat.bpf.o syscallstat.bpf.o openstat.skel.h openstat.combined.bpf.o
make check
echo "check passed"
```

`set -e` 让任何一步失败都立即退出，CI 只需要看退出码。`make check` 里是那条 `-fsyntax-only`。这套检查能发现的问题和发现不了的问题，值得分开记：

| 能发现 | 发现不了 |
| --- | --- |
| 语法错误、类型错误、警告 | 节名拼错（要加载才知道） |
| 缺 BTF、缺调试信息 | 验证器拒绝（辅助函数用错、越界访问） |
| 跨文件符号冲突 | 运行期语义错误 |
| 用户态编译错误 | 附加点不存在、权限不足 |

这里有一个会让人卡半天的坑：许可证的符号名不能与节名相同。很多旧资料写

```c
char license[] SEC("license") = "Dual BSD/GPL";
```

clang 会直接拒绝：

::: terminal title="bash - 符号名与节名相同时的报错"

```text
error: symbol 'license' is already defined
error: license changed binding to STB_GLOBAL
error: symbol 'license' is already defined
3 errors generated.
```

:::

原因是 ELF 汇编器不允许一个符号与它所在的节同名，这与 BPF 无关：用 GCC 编一个同样写法的普通 ELF 目标文件，汇编器同样会拒绝，提示 `license` 符号重复定义。把符号改成大写就能避开：

```c
char LICENSE[] SEC("license") = "Dual BSD/GPL";
```

libbpf 在底层仅检索节名称来识别许可证，符号名称的选取完全不影响内核加载，本文的示例统一采用 `LICENSE` 命名。

::: warning 节名写错在编译期查不出来

`SEC()` 里的节名是普通字符串，编译器不检查。写成 `tracepointX/...` 或 `tp_x/...` 都能编译通过，`gen skeleton` 也能生成骨架，直到加载时才报 `missing BPF prog type`。在 CI 里至少做一次加载测试，或者用 `llvm-objdump -h` 核对节名。

:::

### 8.2 需要 root 或虚拟机的检查

第二层里有四件事，按代价从低到高：

**veristat** 加载目标文件并记录验证器的统计，不附加程序。它最常用的功能是比较：用 `veristat -o csv x.bpf.o > baseline.csv` 把改动前的结果存一份，改完再存一份，然后用 `veristat -C baseline.csv current.csv` 对照。输出里的 `Insns` 和 `States` 两列最能说明问题，若验证器分析的指令步数或缓存状态数发生陡增，往往表明某处代码诱发了更为庞大的状态分支组合，预示着程序可能逼近复杂度上限。

**BPF_PROG_TEST_RUN** 是内核提供的一个接口，可以给已经加载的程序喂一段输入、取回返回值，不用真的把程序挂到钩子上。它适合 `XDP`、`tc`、套接字过滤器这类有明确输入输出格式的程序，可以把「包长多少、应不应该丢」写成单元测试。`bpftool prog run` 是它的命令行入口。

**加载测试**是在真实内核上跑一次 `open → load → attach`，能发现前一层全部漏掉的问题：节名、辅助函数是否可用、附加点是否存在、验证器是否放行。它需要 root，因此通常放在容器或虚拟机里。

**多内核版本的加载测试**用 QEMU 跑。`virtme-ng` 和 `vmtest` 都能用当前源码树起一个虚拟机，把目标文件拷进去加载一遍，用来验证 CO-RE 重定位在旧内核上是否仍然正确。这一步的成本最高，只在发布前做。

本文介绍的流水线检查重点聚焦于第一层：静态编译、骨架生成与用户态语法检查。这套分层校验思想与[持续集成](/教程/正文/开发与工程基础/18_持续集成)章节中阐述的工程理念完全契合：在每次代码提交触发的高频流水线中运行轻量、无需特权的快速自测，而将依赖真实虚拟化内核的长周期集成测试沉淀至夜间或发布前阶段。

<Quiz
  type="multi"
  :options="[
    { label: '用 clang 编译内核态程序', value: 'A' },
    { label: '用 bpftool gen skeleton 生成骨架', value: 'B' },
    { label: '用 bpftool prog dump xlated 查看指令', value: 'C' },
    { label: '编译用户态加载程序或做语法检查', value: 'D' },
    { label: '用 veristat 比较验证器统计', value: 'E' }
  ]"
  :answer="['A', 'B', 'D']"
>

<template #question>

下面哪些检查在普通用户下就能完成，不需要 root？

</template>

正确答案是 **A、B、D**。编译、生成骨架、编译用户态程序都只读文件、写文件，不碰内核。C 要读取已经加载的程序，E 要把程序交给验证器，两者都需要相应的权限。

</Quiz>

## 9. 习题

### 9.1 巩固

<Exercise id="47401" :d="1" :w="1">

在某台机器上运行 `sudo bpftrace -e 'tracepoint:raw_syscalls:sys_enter { @[comm] = count(); }'`，十秒后按 Ctrl-C，得到下面几行：

```text
@[make]: 1404065
@[cc1]: 482888
@[sh]: 270515
```

回答三个问题：

1. 方括号里的名字是什么，冒号后面的数字是什么？
2. 如果把键从 `comm` 换成 `pid`，输出会多出什么信息、失去什么信息？
3. 为什么结果是在按下 Ctrl-C 之后才出现，而不是每发生一次系统调用就打印一行？

</Exercise>

<Exercise id="47402" :d="2" :w="2">

写一条命令或一个短脚本：只统计 PID 为 1234 的进程发出的 `openat`，并且只统计路径里含 `/etc` 的调用；每 5 秒打印一次结果并清空，按 Ctrl-C 退出。

要求：

- 路径判断用 `strcontains(str(args.filename), "/etc")`，注意 `args.filename` 是指针，要先 `str()`。
- 先用 `bpftrace -l 'tracepoint:syscalls:sys_enter_openat'` 确认探针存在。
- 用 `bpftrace -d ast` 或实际运行验证语法；写出你对 `clear(@)` 与 `delete(@, key)` 各自作用的一句话说明。

</Exercise>

### 9.2 编程

<Exercise id="47403" :d="3" :w="3">

写一个 `vfslat.bt`，测量 `vfs_read` 的耗时，按进程名分别给出直方图，单位纳秒。要求：

- 入口用 `kprobe:vfs_read` 记录 `nsecs`，出口用 `kretprobe:vfs_read` 计算差值；
- 入口与出口的配对以 `tid` 为键，出口处必须过滤 `@start[tid]` 存在的情况；
- 出口处用 `delete(@start, tid)` 释放条目，`END` 里用 `clear(@start)` 兜底；
- 脚本开头用 `BEGIN` 打印一行说明。

验收：`sudo bpftrace vfslat.bt` 运行 30 秒后按 Ctrl-C，每个被跟踪到的进程各有一组直方图；在另一个终端跑 `cat /etc/passwd > /dev/null` 若干次，应当能看到对应的桶有计数。答题时给出脚本全文，并解释为什么用 `tid` 而不是 `pid`。

</Exercise>

<Exercise id="47404" :d="4" :w="3">

用 `stats()` 写一个脚本，每 2 秒打印一次各进程 `read` 的字节数统计（次数、平均值、总和），打印后清空，退出时再打印一次总计。

起点是这条命令：

```text
tracepoint:syscalls:sys_exit_read { @bytes[comm] = stats(args.ret); }
```

要求：

- 加 `BEGIN` 打印表头，说明三列的含义；
- 加 `interval:s:2` 打印并 `clear(@)`；
- 加 `END` 打印最后一次结果，并说明如果不加 `clear` 会看到什么。

再回答：同样统计 `read` 的返回值，`stats()` 与 `hist()` 分别适合回答什么问题？各自给出一个具体的问题作为例子。

</Exercise>

<Exercise id="47405" :d="5" :w="4">

把 47402 的过滤条件搬进 libbpf 程序。以 `code/系统编程/拓展B_eBPF/B_14_bpftrace与编程工具链/` 下的 `openstat.bpf.c` 与 `openstat.c` 为基础，增加两个选项：

- `-p PID`：只统计该进程；
- `-n PREFIX`：只统计路径以 PREFIX 开头的调用，判断放在内核态。

接口与验收：

::: terminal title="bash - 带两个选项的 openstat"

```text
$ make
$ sudo ./openstat -p 12345
Tracing openat for pid 12345...
/bin/bash           12
```

:::

- `make` 无警告通过，`make check` 通过；
- 用 `echo $$` 拿到当前 shell 的 PID，再用 `-p` 把它传给 `openstat`，只看到这个 shell 的计数，其他进程不出现；
- 路径过滤在内核态完成，用户态不再过滤。

提示：把 PID 和前缀通过 `const volatile` 全局对象传给内核态程序，写法见[实战：编写一个系统观测工具](/教程/正文/系统编程/拓展B_eBPF/B_15_实战_系统观测工具)；如果不想引入 `const volatile`，也可以把条件写成宏，编译时确定。

</Exercise>

### 9.3 综合与挑战

<Exercise id="47406" :d="5" :w="3">

`code/系统编程/拓展B_eBPF/B_14_bpftrace与编程工具链/exercises/` 下有两个故意写错的内核态程序：`bad_name.bpf.c` 与 `bad_section.bpf.c`。

对每一个：

1. 说明它在哪个阶段失败：clang 编译、`bpftool gen skeleton`，还是加载；
2. 用命令验证你的判断（编译、生成骨架、`bpftool -d gen skeleton` 各试一次）；
3. 给出修复后的代码，并说明修复为什么有效。

最后回答：

- `bad_section.bpf.c` 能通过前两步，却在加载时失败，这个现象揭示了持续集成流水线设计中的哪项关键原则？
- `bad_name.bpf.c` 的报错来自编译器后端，信息里没有指向 `SEC("license")` 那一行。如果换一个符号名与节名相同、但字段名更常见的例子（比如一个叫 `text` 的全局数组放进 `text` 节），会得到什么提示？这类报错该怎么更快定位？

</Exercise>

<Exercise id="47407" :d="5" :w="3">

写一个 `check.sh`，在普通用户下完成前文 8.1 节列出的四项检查，任何一步失败都以非零退出。

要求：

- 编译至少两个内核态程序，开 `-Wall -Wextra`，把警告当作错误（`-Werror`）；
- 为每个目标文件生成骨架，输出重定向到 `/dev/null`；
- 用 `bpftool gen object` 把它们链接成一个；
- 对用户态程序做 `cc -std=c17 -Wall -Wextra -fsyntax-only`；
- 打印每一步在做什么，最后打印一行成功信息。

验收：先在正确的代码上跑通，再故意把某个 `SEC()` 的节名改错、把某个函数的关键语句删掉、把两个程序改成同名，分别跑一次，记录各自在第几步被抓到。最后列出这套脚本覆盖不到的三类问题。

</Exercise>

<Exercise id="47408" :d="6" :w="4">

用 `bpftool gen object` 把 `openstat.bpf.o` 与 `syscallstat.bpf.o` 链接成一个目标文件，然后：

1. 用 `bpftool gen skeleton` 生成骨架，找出两个程序和两个 map 在 `struct xxx_bpf` 里的位置；
2. 用 `bpftool btf dump file` 查看合并后的 `FUNC`、`VAR`、`DATASEC`，说明 `.maps` 里两个 map 的相对偏移所反映的内存布局规则；
3. 复制一份 `openstat.bpf.o` 改名后与原件一起链接，读出 libbpf 的报错，解释 `non-weak symbol` 指的是什么，并给出两种解决办法；
4. 把 `syscallstat.bpf.c` 的许可证从 `Dual BSD/GPL` 改成 `GPL`，重新编译后与 `openstat.bpf.o` 链接，读出报错，说明链接器为什么要求许可证一致；
5. 说明合并后的目标文件在用户态怎么用：一个骨架能不能同时附加两个程序、两次附加的返回值怎么处理。

答题时贴出实际命令与输出。

</Exercise>

<Exercise id="47409" :d="7" :w="5" scope="*">

有人写了下面这段脚本，想统计「哪些进程打开文件失败最多」。

```bash
#!/usr/bin/env bpftrace
BEGIN
{
    printf("Tracing failed opens by comm: %s\n", comm);
}

tracepoint:syscalls:sys_enter_openat
/args->flags & 0x2/
{
    @failed[comm] = count();
}
```

回答：

1. `args->flags` 在 0.23 上会怎样？说明原因，并给出正确写法。这条错误能不能用 `bpftrace -d ast` 看出来？
2. 即使语法修好，这个脚本也统计不到「失败」。问题出在哪一步？要用哪个探针、哪个字段才能判断一次 `openat` 是否失败？
3. `0x2` 对应 `openat` 的哪个标志？把它当作「失败」的条件错在哪里？
4. `BEGIN` 里打印的 `comm` 是什么值？把它印在表头里会不会误导读者？
5. 给出修正后的完整脚本：用入口出口配对记录失败的 `openat`，按进程名分组统计，每 5 秒打印前 3 名并清空。

用 `bpftrace -d ast`、`bpftrace --dry-run` 和实际运行三种方式定位问题，并说明哪些错误只有实际运行才暴露。

</Exercise>

## 10. 参考资料

- bpftrace 官方一行命令教程（十二课，前文第 2 节的核心命令与示例输出均出自此处）：<https://github.com/bpftrace/bpftrace/blob/master/docs/tutorial_one_liners.md>
- bpftrace 0.23 语言手册（探针类型、内置对象、聚合函数、`-d` 阶段）：<https://bpftrace.org/docs/release_023/language>
- bpftrace 手册页（选项、`delete`/`clear`/`print` 的语义、`-lv` 示例）：<https://github.com/bpftrace/bpftrace/blob/master/man/adoc/bpftrace.adoc>
- bpftrace 自带的工具与示例输出（`vfsstat.bt`、`opensnoop.bt`、`execsnoop.bt`、`biolatency.bt`、`syscount.bt`）：<https://github.com/bpftrace/bpftrace/tree/master/tools>
- bpftrace 变更记录（`args` 写法、`fentry`/`fexit` 改名、0.22 的 `delete` 参数）：<https://github.com/bpftrace/bpftrace/blob/master/CHANGELOG.md>
- Brendan Gregg，*BPF CO-RE, BTF, and libbpf*（BCC 的运行时依赖、libbpf 工具的体积、BCC Python 的定位）：<https://www.brendangregg.com/blog/2020-11-04/bpf-co-re-btf-libbpf.html>
- libbpf-tools 源码：<https://github.com/iovisor/bcc/tree/master/libbpf-tools>
- bpftool 源码与文档：<https://github.com/libbpf/bpftool>
- veristat（用法与统计列）：<https://github.com/libbpf/veristat>
- cilium/ebpf 文档（`bpf2go` 的用法）：<https://ebpf-go.dev/>
- Aya（纯 Rust 的 eBPF 库）：<https://aya-rs.dev/>
- libbpf-rs（libbpf 的 Rust 封装）：<https://github.com/libbpf/libbpf-rs>
- eBPF 概览：<https://ebpf.io/what-is-ebpf/>
- Debian 13 的 bpftrace 包（版本 0.23.2、运行时依赖、工具安装位置）：<https://packages.debian.org/trixie/bpftrace>
