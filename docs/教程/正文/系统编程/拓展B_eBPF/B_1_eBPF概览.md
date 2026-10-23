# B.1 eBPF 概览：历史、架构与应用场景

某个服务每隔几秒读一次 `/etc/myapp/limits.conf`，配置一改就立刻生效，可日志里查不出是哪个进程在读、读的是哪个路径、每次读花了多久。要回答这个问题，得在 `openat` 的处理路径上加一段记录代码。改内核源码要重编译、重启，还要维护一份自己的分支；写内核模块不用重启，但那段代码与内核同权限，一次空指针解引用就可能让整台机器失去响应，而且内核一升级就要重新编译。

eBPF（extended Berkeley Packet Filter）走的是第三条路：把一段受限的 C 程序编译成字节码，加载时由内核里的验证器（verifier）证明它不会越界、一定会结束、只调用允许的辅助函数（helper function），通过后翻译成本机指令，附加到内核预先提供的钩子（hook）上，事件发生时执行，结果经 map 交回用户态。这一页交代 eBPF 的来历、从源码到运行的完整流程、它在观测、网络、安全三个方向的典型用途，以及本拓展为什么以 libbpf 加 CO-RE 作为主线。后面每一节展开其中一块，这里只给全景。

## 1. 给运行中的内核加一段逻辑

内核之外的程序想让内核多做一件事，有三条路。三条路都能走通，区别在代价和适用范围。

### 1.1 改内核源码

最直接的办法是修改内核源码，在 `fs/` 下处理系统调用的函数里加一行 `printk`，重新编译，安装，重启。这条路能做的事最多，代价也最大：发行版的内核要重新打包，启用 Secure Boot 的机器还要处理签名，而这段改动从此成为一份自己维护的分支，内核每升一次级，合并、解决冲突、重新验证都要重来一遍。它适合修改内核本身的实现，或者添加内核从未提供的新接口，不适合临时看一眼。

### 1.2 写内核模块

内核模块（loadable kernel module）不必重编译内核，`insmod` 就能把代码装进运行中的内核，这是它相对第一条路的优势。代价是模块代码与内核运行在同一特权级，没有验证，也没有隔离。模块还必须针对目标内核编译：内核版本、配置乃至结构体布局一变，就要重新编译；发行版内核往往不提供构造模块所需的完整环境。加载模块需要 root，许多发行版还要求模块带有可信签名。

### 1.3 用 eBPF

eBPF 把这段逻辑写成一个受限制的 C 程序，用 clang 编译成一套固定的字节码，再由用户态的加载程序（loader）通过 `bpf()` 系统调用交给内核。内核先验证：所有内存访问都在允许的范围内，程序一定会结束，调用的辅助函数属于当前程序类型允许的集合。通不过的程序根本进不了内核，用户态收到一个错误码和一段验证器日志；通过的程序由即时编译器（just-in-time compilation，JIT）翻译成本机指令，附加到某个钩子上，事件发生时执行。

整个过程不重编译内核，不重启，出错时最坏的结果是加载被拒。配合后面要讲的 CO-RE，同一份编译产物还能在多个内核版本上运行，目标机器不需要装编译器和内核头文件。

### 1.4 三条路的取舍

|  | 改内核源码 | 内核模块 | eBPF |
| --- | --- | --- | --- |
| 重编译内核 | 要 | 不要 | 不要 |
| 重启 | 要 | 不要 | 不要 |
| 出错后果 | 编译失败，或新内核起不来 | 内核 oops，可能整机不可用 | 加载被拒，打印验证器日志 |
| 与内核版本的关系 | 每个版本重新移植 | 按版本编译，结构体变了就要改 | 带 BTF 的内核上可以移植 |
| 能做的事 | 任意 | 任意 | 受程序类型与辅助函数限制 |
| 加载权限 | root | root，常常还要签名 | root，或相应的能力 |

判断标准可以归纳成一句话：要做的事能不能表达成「某个事件发生时读一点数据记下来，或者对这次操作放行、拒绝、改写」？能，就用 eBPF。不能，比如要新增一个系统调用、替换文件系统的实现、改调度算法本身，那仍然是内核源码或内核模块的领域。eBPF 不是通用的内核编程手段，它是一台被限制得很紧的虚拟机，能力来自内核预先开放的接口，新功能要等内核提供。

<Quiz
  type="multi"
  :options="[
    { label: '加载前由内核里的验证器检查，不通过就拒绝加载', value: 'A' },
    { label: '不需要重编译内核，也不需要重启', value: 'B' },
    { label: '可以调用任意一个内核函数', value: 'C' },
    { label: '能做的事受程序类型与辅助函数的限制', value: 'D' }
  ]"
  :answer="['A', 'B', 'D']"
>

<template #question>

与内核模块相比，eBPF 的特点有哪些？

</template>

正确答案是 **A、B、D**。C 错在「任意」：eBPF 程序只能调用当前程序类型允许的辅助函数与 kfunc，其余内核函数一律调不到；这也正是验证器能保证安全的前提之一。

</Quiz>

## 2. 从 BPF 到 eBPF

eBPF 的「e」是 extended，说明它是在一套更老的机制上扩出来的。那套机制叫经典 BPF（classic BPF，cBPF），最初的目的只有一个：让抓包工具在内核里就把不要的包丢掉。今天 RFC 9669 的说法是 eBPF 这个缩写「不再展开为任何东西」，但追根溯源它是 extended Berkeley Packet Filter 的简称，扩展的对象正是 cBPF。

### 2.1 包过滤器：1992 年的 BPF

1992 年，Lawrence Berkeley Laboratory 的 Steven McCanne 与 Van Jacobson 发表了《The BSD Packet Filter: A New Architecture for User-level Packet Capture》。论文要解决的问题很具体：抓包程序在用户态，每个包都要从内核复制到用户态才能判断要不要，判断完再丢掉，复制本身就吃掉了大部分时间。他们的做法是把过滤器编译成一段小程序，挂到套接字上，在内核里对每个包执行，只把通过过滤的包复制出去。论文给出的数字是：新的基于寄存器的过滤器求值器比原来的栈式设计快最多 20 倍，整体上比 Sun 的 NIT 快最多 100 倍。

这台过滤器机器就是 cBPF 虚拟机，它的全部家当是：

| 成分 | 说明 |
| --- | --- |
| `A` | 32 位累加器 |
| `X` | 32 位索引寄存器 |
| `M[]` | 16 个 32 位暂存槽，编号 0～15 |

一条 cBPF 指令编码成 `op:16, jt:8, jf:8, k:32`，共 8 字节。`op` 是 16 位操作码，`jt` 与 `jf` 分别是条件为真、为假时的跳转偏移，`k` 是随操作码解释的操作数。程序从网络包里读数用 `ld`、`ldh`、`ldb`（分别读字、半字、字节），用 `jeq`、`jgt`、`jset` 这类指令比较，用 `ret` 返回一个 32 位数。对套接字过滤器来说，返回值是「保留多少个字节」：0 表示丢弃，不小于包长的值表示整个包放行，介于两者之间表示截断到这个长度。

内核文档 `Documentation/networking/filter.rst` 给了一个完整的例子，过滤 ARP 包：

```text
ldh [12]
jne #0x806, drop
ret #-1
drop: ret #0
```

它读以太网帧偏移 12 处的 16 位类型字段，不等于 `0x806`（ARP）就跳到 `drop` 返回 0，否则返回 -1（按无符号解释是 4294967295），表示整个包放行。用内核源码 `tools/bpf/` 下的 `bpf_asm` 汇编这段文本，输出类似：

::: terminal title="bash - bpf_asm"

```text
$ ./bpf_asm foo
4,40 0 0 12,21 0 1 2054,6 0 0 4294967295,6 0 0 0,
```

:::

开头的 4 是指令条数，后面每条指令按 `op jt jf k` 的十进制顺序排列。`40 0 0 12` 就是 `ldh [12]`：0x28 是「半字加载」的操作码，`jt`、`jf` 不用，`k` 是偏移 12。`21 0 1 2054` 是那条 `jne`：0x15 是 `jeq` 的操作码，`jt=0`、`jf=1`，`k=2054`（即 0x806）。cBPF 没有单独的 `jne`，`bpf_asm` 把它汇编成 `jeq` 并把两个跳转目标对调：相等时走 `jt=0`，接着执行下一条 `ret #-1`；不等时走 `jf=1`，跳过一条到达 `drop: ret #0`。跳转偏移是相对下一条指令算的。`6 0 0 4294967295` 和 `6 0 0 0` 是两条 `ret`，0x06 是返回指令。

抓包工具里的过滤器至今还是这么来的。tcpdump 的 `-d` 选项把过滤表达式编译成可读的 cBPF 指令，`-dd` 输出成 C 数组初始化式，`-ddd` 输出成上面这种十进制形式，可以原样喂给 `bpf_asm` 或 `bpf_dbg`。写过滤表达式时感觉不到这台机器的存在，但每一句 `tcp port 22` 最后都会变成这样一串指令，在内核里对每个包执行。

### 2.2 eBPF：同一台机器，换了指令集与用途

cBPF 只有两个 32 位寄存器，面对 64 位机器和越来越复杂的逻辑就不够用了。2014 年，Alexei Starovoitov 提交的补丁把过滤器机器整体重做成一套新的指令集，即 eBPF，随 Linux 3.18（2014 年 12 月）进入内核，同时引入了 `bpf()` 系统调用。这次重做不只是加宽寄存器：

|  | cBPF | eBPF |
| --- | --- | --- |
| 寄存器 | `A`、`X` 两个 32 位寄存器，外加 16 个暂存槽 | 11 个 64 位寄存器 R0～R10，另有 32 位子寄存器 |
| 栈 | 无，只有 `M[]` 那 16 个暂存槽 | 每个程序 512 字节 |
| 指令编码 | `op:16, jt:8, jf:8, k:32` | `op:8, dst:4, src:4, off:16, imm:32` |
| 条件跳转 | 真、假两个 8 位偏移 | 一个 16 位偏移，加一条无条件跳转 |
| 调用 | 无 | `call` 调用辅助函数或同一目标文件里的子程序 |
| 数据交换 | 只能返回一个 32 位数 | map，内核态与用户态共同访问的键值存储 |
| 主要用途 | 套接字过滤、seccomp | 跟踪、网络、安全三大方向 |

寄存器多了，编码方式也跟着变。每条指令仍是 8 字节，但操作码让出 8 位给源、目的寄存器编号，`jt` 和 `jf` 合并成一个 16 位的有符号偏移。cBPF 的每条条件跳转都能指定两个目标，eBPF 的条件跳转只带一个 16 位偏移，意思是「条件成立时跳过几条」，条件不成立就执行下一条，反汇编里写成 `if w1 != w0 goto +0x5`。两个方向都要跳时，得再接一条无条件跳转。R0 放返回值，R1～R5 传参数，R6～R9 跨调用保存，R10 是只读的帧指针。这套约定的细节在 [B.4](/教程/正文/系统编程/拓展B_eBPF/B_4_程序的解剖_受限的C) 逐条讲。

指令集换了，程序能挂的地方也一路扩开。下面这些节点是按内核版本排的，每条都能在内核发布说明或内核文档里查到：

| 时间 | 内核 | 节点 |
| --- | --- | --- |
| 1992 年 12 月 | — | McCanne 与 Jacobson 的 BPF 论文（1993 年 1 月 USENIX Winter 会议宣读） |
| 2012 年 7 月 | 3.5 | seccomp 改用 cBPF 过滤系统调用，BPF 第一次离开网络 |
| 2014 年 12 月 | 3.18 | eBPF 与 `bpf()` 系统调用进入内核 |
| 2015 年 6 月 | 4.1 | eBPF 可以附加到 kprobe |
| 2016 年 10 月 | 4.8 | XDP 进入内核 |
| 2018 年 8 月 | 4.18 | BTF 与 AF_XDP |
| 2019 年 9 月 | 5.3 | 验证器支持有界循环 |
| 2020 年 1 月 | 5.5 | BPF 蹦床进入内核，fentry 与 fexit 由此可以挂到内核函数 |
| 2020 年 5 月 | 5.7 | BPF LSM |
| 2020 年 8 月 | 5.8 | 环形缓冲区 map |
| 2024 年 11 月 | 6.12 | sched_ext 的首批代码进入内核 |

::: details 更完整的里程碑

- 2011 年 4 月：cBPF 的 JIT 进入内核，过滤器不再逐条解释。
- 2015 年 8 月：LLVM 3.7 加入 BPF 后端，C 可以编译成 eBPF。
- 2015 年 9 月：bcc 项目发布。
- 2016 年 8 月：Cilium 发布。
- 2017 年 5 月：Meta 的 Katran 负载均衡上线。
- 2017 年 9 月：bpftool 进入内核源码树。
- 2018 年 10 月：bpftrace 发布。
- 2020 年 9 月：GCC 的 BPF 后端合并。
- 2021 年 5 月：微软宣布把 eBPF 带到 Windows。
- 2021 年：eBPF 基金会成立。
- 2024 年 10 月：RFC 9669 发布，BPF 指令集成为 IETF 标准。

:::

### 2.3 今天的 BPF

今天不加限定地说 BPF，指的都是 eBPF。cBPF 并没有消失：seccomp 从 3.5 起就用它做系统调用过滤，套接字过滤器也仍然接受 cBPF 程序，内核在内部把它们转换成 eBPF 再执行。所以读旧代码时会看到两套写法并存，判断依据是加载方式：走 `setsockopt(SO_ATTACH_FILTER)` 或 `seccomp()` 的是 cBPF，走 `bpf()` 的是 eBPF。seccomp 的写法在 [B.13](/教程/正文/系统编程/拓展B_eBPF/B_13_安全应用_seccomp_LSM与cgroup) 展开。

<Quiz
  :options="[
    { label: 'XDP', value: 'A' },
    { label: 'seccomp', value: 'B' },
    { label: 'kprobe', value: 'C' },
    { label: 'BPF LSM', value: 'D' }
  ]"
  answer="B"
>

<template #question>

今天仍在使用经典 BPF 的机制是哪一个？

</template>

正确答案是 **B**。seccomp 的过滤器由 `seccomp()` 系统调用安装，参数是一串 cBPF 指令和 `struct seccomp_data`；它只需要读固定偏移上的几个字段、做几十次比较，cBPF 的模型足够，没有必要引入 eBPF 的 map 与辅助函数。XDP、kprobe、BPF LSM 都是 eBPF 的程序类型。

</Quiz>

## 3. 架构总览

一个 eBPF 程序从源文件到在内核里跑起来，中间有一段固定的流水线。下面这张图画的是本拓展后面所有示例共用的路径。

```mermaid
flowchart LR
    A["x.bpf.c<br/>受限的 C"] -->|clang -target bpf| B["x.bpf.o<br/>ELF 目标文件"]
    B -->|bpftool gen skeleton| C["x.skel.h<br/>骨架"]
    C --> D["x.c<br/>用户态加载程序"]
    D -->|bpf() BPF_PROG_LOAD| E["内核验证器"]
    E -->|通过| F["JIT 编译"]
    F --> G["附加到钩子<br/>attach"]
    G -->|事件发生| H["程序执行"]
    H <--> I["map"]
    I <--> D
    E -->|拒绝| J["错误码加验证器日志"]
```

编译、生成骨架、编写加载程序这三步在用户态完成，产物是一个 ELF 文件和一个 C 头文件；验证、JIT 与附加由内核的 `bpf()` 完成；最右边是事件发生之后的运行与数据交换。每一块在后文的位置：

| 环节 | 里面发生什么 | 详见 |
| --- | --- | --- |
| 编译 | clang 把 C 编译成 BPF 字节码，`SEC` 宏把函数放进以节名命名的段 | [B.4](/教程/正文/系统编程/拓展B_eBPF/B_4_程序的解剖_受限的C) |
| 目标文件 | 一份 ELF，含程序段、`.BTF`、`.BTF.ext` 和重定位 | [B.4](/教程/正文/系统编程/拓展B_eBPF/B_4_程序的解剖_受限的C) |
| 加载程序 | 读 ELF，建 map，做 CO-RE 重定位，调 `bpf()` | [B.7](/教程/正文/系统编程/拓展B_eBPF/B_7_加载与附加_bpf系统调用与程序生命周期)、[B.8](/教程/正文/系统编程/拓展B_eBPF/B_8_libbpf_BTF与CO_RE) |
| 验证 | 逐条路径模拟寄存器状态，证明内存安全与终止 | [B.9](/教程/正文/系统编程/拓展B_eBPF/B_9_验证器) |
| 执行 | JIT 翻译成本机指令，钩子触发时运行 | 本节 |
| map | 内核态与用户态共同访问的键值存储 | [B.6](/教程/正文/系统编程/拓展B_eBPF/B_6_Map与数据交换) |
| 钩子与程序类型 | 决定 ctx 形态、可用辅助函数、返回值含义 | [B.10](/教程/正文/系统编程/拓展B_eBPF/B_10_程序类型与附加类型) |

### 3.1 最小程序：两条指令

程序的骨架可以先放一放，先看清最终要交给内核的是什么。下面这个文件不依赖任何头文件，手写 `SEC` 宏，只做一件事：定义在 `openat` 的入口处执行、立刻返回 0 的程序。完整文件在 `code/系统编程/拓展B_eBPF/B_1_eBPF概览/ret0.bpf.c`：

```c
/* ret0.bpf.c：本页用来观察 eBPF 指令编码的最小程序。 */
#define SEC(name) __attribute__((section(name), used))

char LICENSE[] SEC("license") = "Dual BSD/GPL";

SEC("tracepoint/syscalls/sys_enter_openat")
int ret0(void *ctx) {
    (void)ctx;
    return 0;
}
```

`SEC` 是个宏，展开成 `__attribute__((section(name), used))`：把函数放进名为 `tracepoint/syscalls/sys_enter_openat` 的段里，`used` 阻止编译器因为「没人调用」把它删掉。真正决定这个函数被当成哪种程序、挂到哪里的，是那串节名，不是函数名。

编译与反汇编。本页的产物用 zig 0.13 自带的 clang 18 加 `-target bpfel-freestanding-none` 编译；用另一台机器上 LLVM 22 的 `clang -target bpf` 编译同一个文件，程序段的指令字节完全相同，差别只在调试节的多少与各节的编号：

::: terminal title="bash - ret0"

```text
$ clang -g -O2 -target bpf -mcpu=v3 -Wall -Wextra -c ret0.bpf.c -o ret0.bpf.o
$ llvm-objdump -d ret0.bpf.o

ret0.bpf.o:	file format elf64-bpf

Disassembly of section tracepoint/syscalls/sys_enter_openat:

0000000000000000 <ret0>:
       0:	b4 00 00 00 00 00 00 00	w0 = 0x0
       1:	95 00 00 00 00 00 00 00	exit
```

:::

两条指令，十六个字节。左边一列十六进制是这 8 字节的原始内容：`b4 00 00 00 00 00 00 00` 中，`b4` 是操作码（把 32 位立即数移进寄存器），第二个字节 `00` 的低 4 位是目的寄存器编号 0，即 R0，后面两个字节是偏移 0，最后四字节是立即数 0。`95` 是 `exit`。R0 是返回值寄存器，所以这段程序的语义就是「返回 0 然后退出」。

写成 `w0 = 0x0` 而不是 `r0 = 0x0`，是因为 `-mcpu=v3` 让编译器选用了 32 位的 `mov32` 指令（操作码 `0xb4`）。把它换成 `-mcpu=v1`，同一条语句会编成 64 位的 `mov64`（`0xb7`），反汇编显示 `r0 = 0x0`。两者的运行时效果对返回 0 这件事没有区别，但指令不同，说明 CPU 版本的选择会真实改变字节码。本章统一写 `-mcpu=v3`，理由见 [B.4](/教程/正文/系统编程/拓展B_eBPF/B_4_程序的解剖_受限的C)。

目标文件里除了这段程序，还有几样东西（下面只摘出与运行相关的节，调试节省略）：

::: terminal title="bash - llvm-objdump -h"

```text
$ llvm-objdump -h ret0.bpf.o
ret0.bpf.o:	file format elf64-bpf

Sections:
Idx Name                                 Size     VMA              Type
  2 .text                                00000000 0000000000000000 TEXT
  3 tracepoint/syscalls/sys_enter_openat 00000010 0000000000000000 TEXT
  4 license                              0000000d 0000000000000000 DATA
  9 .BTF                                 0000015a 0000000000000000 
 10 .rel.BTF                             00000010 0000000000000000 
 11 .BTF.ext                             00000050 0000000000000000 
```

:::

`tracepoint/syscalls/sys_enter_openat` 长 0x10 字节，正好是那两条指令。`license` 里是 13 字节的 `"Dual BSD/GPL"`，加载时交给内核，决定这个程序能调用哪些辅助函数。`.BTF` 是类型信息，`.BTF.ext` 存函数与行号信息以及 CO-RE 重定位，加载程序和验证器都要读它。这三类东西的用途分别在 [B.4](/教程/正文/系统编程/拓展B_eBPF/B_4_程序的解剖_受限的C)、[B.8](/教程/正文/系统编程/拓展B_eBPF/B_8_libbpf_BTF与CO_RE) 和 [B.9](/教程/正文/系统编程/拓展B_eBPF/B_9_验证器) 讲。

### 3.2 加载与附加

目标文件本身不会自己跑起来。由用户态程序打开它，创建其中的 map，做完重定位，用 `bpf()` 的 `BPF_PROG_LOAD` 命令把每段程序交给内核，验证通过后再用另一条命令把它附加到钩子上。libbpf 把这一串操作包成几个函数，`bpftool gen skeleton` 又把它们生成成针对某个目标文件的 C 头文件，这就是「骨架」的来历。

用 SignalX 上的 bpftool 7.5 为上面这个文件生成骨架，`struct ret0_bpf` 里可以看到每个程序和每个 link 各占一个成员：

::: terminal title="bash - bpftool gen skeleton"

```text
$ bpftool gen skeleton ret0.bpf.o
/* SPDX-License-Identifier: (LGPL-2.1 OR BSD-2-Clause) */

/* THIS FILE IS AUTOGENERATED BY BPFTOOL! */
#ifndef __RET0_BPF_SKEL_H__
#define __RET0_BPF_SKEL_H__
...
struct ret0_bpf {
	struct bpf_object_skeleton *skeleton;
	struct bpf_object *obj;
	struct {
		struct bpf_program *ret0;
	} progs;
	struct {
		struct bpf_link *ret0;
	} links;
...
};
```

:::

骨架的四个入口依次是 `ret0_bpf__open()`、`ret0_bpf__load()`、`ret0_bpf__attach()` 和 `ret0_bpf__destroy()`，分别对应读 ELF、加载进内核、附加到钩子和释放。多一个程序就多一个 `progs` 成员（[B.3](/教程/正文/系统编程/拓展B_eBPF/B_3_第一个eBPF程序) 会写完整的用户态加载程序）。

::: concept ebpf-event-driven title="eBPF 程序的运行方式"

eBPF 程序由事件驱动：它被附加到某个钩子上，只有当该钩子对应的内核事件发生时才会运行，事件不发生，程序一行也不执行。加载时验证器已经证明它安全，运行时不再逐条检查，JIT 之后它执行的是本机指令，开销接近直接写在钩子处的代码。

:::

这两句话包含两个常在初学时被搞混的点。

第一，eBPF 程序不是常驻的守护进程，也没有自己的线程。它没有「主循环」，不能主动发起工作，不能等待、睡眠（少数标记为可睡眠的类型除外），也不能在两次调用之间保留寄存器或栈上的状态。需要跨事件保存数据时用 map：map 是独立于程序存在的内核对象，程序结束它还在，用户态通过文件描述符访问同一份数据。这个设计在 [B.6](/教程/正文/系统编程/拓展B_eBPF/B_6_Map与数据交换) 展开。

第二，「验证过所以安全」指的是运行时不再检查。验证器在加载时模拟程序可能走的每一条路径，跟踪每个寄存器的类型和取值范围，确认所有访存都在界内、程序一定会退出。这个证明过程很贵，所以只在加载时做一次；之后 JIT 出来的代码直接执行，速度接近手写的本机代码。如果每次调用都重新检查，eBPF 的性能优势就不存在了。

<Quiz
  :options="[
    { label: '加载程序启动后立即运行一次，之后每秒运行一次', value: 'A' },
    { label: '附加的钩子上有事件发生时运行', value: 'B' },
    { label: '用户态程序调用 bpf() 读取 map 时运行', value: 'C' },
    { label: '由内核的定时器周期性唤醒', value: 'D' }
  ]"
  answer="B"
>

<template #question>

一个附加到 `sys_enter_openat` 上的 eBPF 程序，什么时候运行？

</template>

正确答案是 **B**。程序没有主循环，也没有自己的线程，只有内核执行到 `openat` 的系统调用入口、触发这个 tracepoint 时才被调用；没有进程调用 `openat`，它就一次也不跑。A、D 说的都是「自己会跑起来」，那需要内核主动调度它，eBPF 程序没有这个能力。C 混淆了 map 与程序：读 map 是用户态发起的操作，不会触发程序。

</Quiz>

::: generate ebpf-vm-in-kernel kind=explanation concepts=ebpf-event-driven

面向第一次接触 eBPF、但熟悉 C 与进程概念的读者，解释「内核里的虚拟机」到底是什么意思：为什么说它是一台虚拟机、它和 VMware 那类系统虚拟机有什么区别、为什么程序不能自己跑起来而必须挂到钩子上、以及验证器为什么能保证安全而运行时不再检查。用实际的调用链条说明事件发生时内核怎样跳到这段程序，不要复述后面各节才讲的指令编码与 map API。

:::

### 3.3 各部分对应的节

把上面几节串起来，一个完整的工具包含四类文件：`x.bpf.c` 是内核态程序，`x.c` 是用户态加载程序，`x.skel.h` 由 bpftool 从目标文件生成，`x.h` 供两边共享结构体。本拓展每一节的示例都按这个布局放在 `code/系统编程/拓展B_eBPF/` 下对应的目录里。

::: tip 本拓展的验证方式

eBPF 程序加载进内核需要 root，本教程的验证环境没有这个条件。所以本拓展的 BPF 程序统一用 zig 自带的 clang 18 编译、用 `llvm-objdump` 反汇编，骨架由 SignalX 上的 bpftool 7.5 生成，用户态加载程序只做 `gcc -fsyntax-only` 语法检查。凡是需要真正加载才有的运行输出，都照 libbpf-bootstrap 等权威资料引用，写成「输出类似」。读者在自己的机器上运行时，看到的数字、PID、时间戳自然会不同。

:::

## 4. 三类典型应用

eBPF 的用途可以粗分成三块，这个分法在 ebpf.io 与各类综述里通用，也大致对应内核里的程序类型分组。

### 4.1 观测

这是 eBPF 用得最广的方向，也是本教程最看重的一块。内核里的每个关键事件，例如系统调用进入与返回、函数调用、网络包收发、调度切换、文件操作，都可以挂一个程序，把当时的时间、进程、参数记录下来，或者就地聚合成计数和直方图，只把汇总结果交回用户态。

传统上这件事靠三类工具：strace 看单个进程的系统调用，代价是每次调用都要停下目标进程；perf 采样调用栈，只能看到 CPU 上正在跑的代码；日志则要事先在源码里埋点。eBPF 补上了中间那一块：不需要停下进程，不需要重新编译，也不限于已经在跑的代码，对大多数内核函数与用户态函数都能下钩子。

Brendan Gregg 把 eBPF 的前端分成两类：bcc 用来写复杂的工具和常驻程序，bpftrace 用来写一行命令和短脚本。bcc 自带的 execsnoop、opensnoop、biolatency、tcpretrans 这些工具，以及 bpftrace 的单行命令，覆盖了日常排查的多数场景：哪个进程在频繁打开文件、块设备的 I/O 延迟分布、TCP 重传发生在哪条连接上、任务在运行队列里等了多久。它们的共同做法是把聚合放在内核里做，用户态只取结果，这样即使事件每秒发生几十万次，开销仍然可以接受。

自己写观测程序的写法在 [B.11](/教程/正文/系统编程/拓展B_eBPF/B_11_跟踪与观测_kprobe_uprobe与tracepoint) 与 [B.15](/教程/正文/系统编程/拓展B_eBPF/B_15_实战_系统观测工具)，用 bpftrace 一行命令解决问题在 [B.14](/教程/正文/系统编程/拓展B_eBPF/B_14_bpftrace与编程工具链)。

### 4.2 网络

网络是 eBPF 最早也最成熟的战场，原因是位置。XDP（快速数据路径，eXpress Data Path）把程序放在网卡驱动刚收到帧、还没有分配 `sk_buff` 的地方，此时一个包的全部处理就是读几十个字节、做几个比较，然后决定放行、丢弃还是从原网卡发回去。Cloudflare 2018 年的一篇文章给出的数字是：用 XDP 在单个 CPU 上每秒可以丢弃一千万个包。这正是 DDoS 防护里最基础的一步，把不该进来的包在最早的位置扔掉。

更复杂的用法是把转发决策也搬进 eBPF。Meta 的 Katran 是一台四层负载均衡器，转发面就是一个 XDP 程序：收到包之后查 map 选出后端，重新封装报文再发出去，整个转发不经过协议栈。Cilium 把 eBPF 用作 Kubernetes 的网络、安全与可观测性底座，服务之间的连接、网络策略、负载均衡都由 eBPF 程序完成，kube-proxy 那套 iptables 规则可以整个省掉。

C 语言写这类程序的难点不在逻辑，而在报文解析的边界检查：每次读报文头之前都要和 `data_end` 比较，否则验证器拒绝加载。这部分在 [B.12](/教程/正文/系统编程/拓展B_eBPF/B_12_网络应用_XDP与tc)。

### 4.3 安全

安全方向的用法分两类：审计和拦截。

审计是把安全相关的事件记下来。系统调用、文件打开、权限检查、网络连接，都能挂程序，采集到的数据比传统的审计框架更细，开销也更低。这类工具通常在 LSM 钩子或系统调用 tracepoint 上工作。

拦截是直接改变内核的决定。BPF LSM 允许程序挂在 Linux 安全模块（Linux Security Module，LSM）的钩子上，返回 0 放行，返回负的 errno 拒绝，效果等同于在内核里写一条访问控制规则，但规则可以随时加载、卸载、修改，不用重启。控制组（cgroup）上的程序则按进程组限制网络与设备访问，systemd 的 `SocketBindDeny=` 与 `DeviceAllow=` 就是用这类钩子实现的。

还有一类是限制进程自己：seccomp（安全计算模式，secure computing mode）让进程在启动时安装一个 cBPF 过滤器，此后它发起的每个系统调用都要先过一遍这个过滤器。容器运行时和浏览器用它把不需要的系统调用全部关掉。seccomp 只用 cBPF 而不是 eBPF，原因在 2.3 节说过：输入固定、逻辑简单，不需要更复杂的那套设施。三种机制的写法与对比在 [B.13](/教程/正文/系统编程/拓展B_eBPF/B_13_安全应用_seccomp_LSM与cgroup)。

### 4.4 还在长出来的方向

内核 6.12 引入了 sched_ext 的首批代码，允许用一组 BPF 程序定义调度器：什么时候选哪个任务、放到哪个 CPU、什么时候抢占，都由 BPF 决定。这已经超出了「观察」和「拦截」，是在替换内核的一个核心子系统。HID-BPF 则允许针对具体输入设备写修补程序，不必为每个 quirks 改一次内核驱动。

这两个方向说明一件事：eBPF 的能力边界不是固定的，它随内核开放的钩子扩张。判断一项需求能不能用 eBPF 做，最终要落到「内核有没有为它提供程序类型和辅助函数」上，而不是「eBPF 理论上能不能」。

<Quiz
  :options="[
    { label: '在网卡驱动刚收到帧、还没有分配 sk_buff 时运行，可以就地丢弃', value: 'A' },
    { label: '在应用调用 read() 时运行，可以改写读到的数据', value: 'B' },
    { label: '只能处理 TCP，不能处理 UDP', value: 'C' },
    { label: '必须配合 iptables 规则才能生效', value: 'D' }
  ]"
  answer="A"
>

<template #question>

XDP 程序在收包路径上的位置带来了什么特点？

</template>

正确答案是 **A**。XDP 钩子在驱动层，报文还没有变成 `sk_buff`，所以丢弃一个包的代价最小，这也是它在 DDoS 防护里被大量使用的原因。B 把位置说反了：应用调用 `read()` 时数据早已进入套接字的接收队列，XDP 看不到。C 错在协议：XDP 拿到的是原始以太网帧，协议由程序自己解析。D 错在依赖：XDP 由 `bpf()` 直接附加到网卡，与 iptables 无关。

</Quiz>

## 5. 生态与本章的取舍

### 5.1 工具链一览

写 eBPF 程序不必从零开始，可选的工具已经形成一个不短的清单：

| 工具 | 语言 | 适合什么 |
| --- | --- | --- |
| bcc | Python 前端，内核态 C | 用现成工具；写脚本快速验证想法 |
| bpftrace | 专用脚本语言 | 一行命令或十几行的临时排查 |
| libbpf | C 库 | 要分发、要长期维护的独立工具 |
| cilium/ebpf | Go 库 | 已经在用 Go 的项目 |
| Aya | Rust 库 | 前后两端都用 Rust 写 |
| libbpf-rs | Rust 库 | Rust 项目里复用 libbpf |

bcc 的工作方式是运行时编译：Python 脚本里内嵌一段 C，执行时调 clang 现场编译成字节码再加载。好处是写起来快，坏处是目标机器上必须装 clang、LLVM 和内核头文件。Brendan Gregg 2020 年那篇讲 CO-RE 的文章给出的数字是：这套依赖可以占去 100 MB 以上的存储。

### 5.2 为什么以 libbpf 加 CO-RE 为主线

libbpf 把程序的生命周期拆成明确的几步（打开、加载、附加、销毁），没有运行时编译，也没有 Python 解释器。同一篇文章里，libbpf 版的 opensnoop 未剥离时 645 KB，剥离后只有 151 KB，并且「不使用 libLLVM 或 libclang」，只依赖 libelf、libz 和 libc。这意味着一个工具编译好之后可以直接拷到目标机器上运行，目标机器只要有带 BTF 的内核。

CO-RE（一次编译、到处运行，Compile Once – Run Everywhere）补上最后一块：clang 在编译时把每个结构体成员访问记录成重定位信息，libbpf 在加载时读目标内核的 BTF，按名字和类型找到成员在新内核里的偏移，改写指令里的偏移量。这样同一个 `.bpf.o` 可以在多个内核版本上跑，不用为每个版本重新编译。原理在 [B.8](/教程/正文/系统编程/拓展B_eBPF/B_8_libbpf_BTF与CO_RE)。

本拓展因此以 libbpf 加 CO-RE 为主线：[B.3](/教程/正文/系统编程/拓展B_eBPF/B_3_第一个eBPF程序) 起的每个示例都是「内核态 C 加用户态 C」，用 `bpftool gen skeleton` 生成骨架，用 `pkg-config` 拿 libbpf 的编译参数。bpftrace 单独占一节（[B.14](/教程/正文/系统编程/拓展B_eBPF/B_14_bpftrace与编程工具链)），定位是临时排查：问题能用一行命令问清楚就不必写程序，需要参数、需要长期运行、需要分发给别人的时候再改成 libbpf 程序。

::: tip 先用 bpftrace 问清楚问题

排查时第一步往往是「哪个进程在干这件事」「这个函数被调用了多少次」，这类问题 bpftrace 一行就能回答。等确认了要盯的钩子、要记的字段、要过滤的条件，再把它写成 libbpf 程序。反过来，一上来就写完整的 libbpf 工程，常常在编译和部署上耗掉大部分时间，问题本身还没开始看。

:::

### 5.3 跨平台

eBPF 已经不止于 Linux。微软 2021 年开源了 eBPF for Windows，用 PREVAIL 验证器在用户态做验证，把程序编译成签名的内核驱动，提供与 libbpf 兼容的 API。2024 年 10 月，IETF 发布了 RFC 9669《BPF Instruction Set Architecture (ISA)》，把指令集本身标准化，状态是 Standards Track。标准化的只是「一份字节码怎么解释」，钩子、辅助函数、上下文都不在其中，所以源码可以跨平台复用，编译产物不能。这一块在 [B.16](/教程/正文/系统编程/拓展B_eBPF/B_16_eBPF_for_Windows与跨平台) 讲。

## 6. 习题

### 6.1 巩固

<Exercise id="47101" :d="1" :w="1">

本页的最小程序 `code/系统编程/拓展B_eBPF/B_1_eBPF概览/ret0.bpf.c` 不依赖 libbpf 的头文件也能编译。做三件事：

1. 分别用 `-mcpu=v1` 和 `-mcpu=v3` 编译它，把两份 `llvm-objdump -d` 的输出贴出来，指出唯一不同的那条指令（本题的实测结果：v1 是 `r0 = 0x0`，操作码 `0xb7`；v3 是 `w0 = 0x0`，操作码 `0xb4`）；
2. 按 3.1 节讲的方法，把 `b4 00 00 00 00 00 00 00` 这 8 个字节拆成 opcode、dst、src、off、imm 五个字段，写出每个字段的值；
3. 解释为什么返回 0 这条语句在 `-mcpu=v3` 下编成 `w0 = 0x0` 而不是 `r0 = 0x0`；再把返回值改成 `0x100000000LL` 各编译一次，说明为什么这时两个版本的输出完全一样。

</Exercise>

<Exercise id="47102" :d="2" :w="1">

2.1 节给出了 ARP 过滤器的 cBPF 源码和 `bpf_asm` 的汇编结果：

```text
4,40 0 0 12,21 0 1 2054,6 0 0 4294967295,6 0 0 0,
```

1. 逐个字段说明这四条指令各是什么，特别是 `21 0 1 2054` 里的 `21` 为什么对应 `jeq` 而不是 `jne`，`jt=0`、`jf=1` 又分别表示跳到第几条；
2. 手动执行这四条指令，分别写出以太网类型字段为 `0x0806`、`0x0800`、`0x86dd` 时程序的返回值；
3. 把源码改成只放行 IPv4 TCP（类型字段 `0x0800`，IP 协议号在偏移 23，值为 6），按 `op jt jf k` 的格式写出指令条数与每一条的四个数字，留到 47105 用解释器验证。

</Exercise>

<Exercise id="47103" :d="2" :w="3">

把第 3 节的流水线拆开：指出下面每一项处在哪个环节、由谁执行（用户态工具、内核，还是文件系统里的一份文本）：

- `ret0.bpf.c`
- `ret0.bpf.o`
- `ret0.skel.h`
- 用户态加载程序里的 `ret0_bpf__open()`
- `BPF_PROG_LOAD`
- JIT
- `tracepoint/syscalls/sys_enter_openat` 这个字符串

再做两件事：

1. 实验：把 `ret0.bpf.c` 里的节名改成 `SEC("tracepoint/syscalls/no_such_event")`，重新编译、重新生成骨架，观察这两步是否报错，并据此说明「节名的合法性由哪一步检查」；
2. 选型：下面三个需求分别该用本页第 5 节的哪种工具，为什么？(a) 一次性查清哪个进程在反复打开 `/etc/myapp/limits.conf`；(b) 写一个带 `-p PID` 和 `-d 秒数` 参数、要分发给同事长期运行的打开文件监视工具；(c) 在一个已有的 Go 运维服务里加一段统计 TCP 重传次数的逻辑。

</Exercise>

### 6.2 编程

<Exercise id="47104" :d="3" :w="2">

写一个 `insn_decode.c`，把下面的字节数组按 8 字节一条拆成 eBPF 指令，打印每条指令的 opcode、dst、src、off、imm：

```c
static const unsigned char bytes[] = {
    0xbf, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x04, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00,
    0x95, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};
```

要求：

1. 用一个位域结构体表示指令，字段按 `code / dst / src / off / imm` 排列；从 `unsigned char` 数组逐字节拼出字段，不要用结构体指针直接指向数组（说明这样做的理由）；
2. 每条指令输出一行，格式如 `0: code=0xbf dst=0 src=1 off=0 imm=0`；
3. 验收：把 `int f(long x) { return x + 3; }` 用 `clang -target bpf -O2 -mcpu=v3 -c` 编译，用 `llvm-objdump -s -j .text f.o` 取出 24 个字节，与上面数组对照；再用 `llvm-objdump -d` 看三行反汇编，确认解码出的 dst、src、imm 与助记符一致。然后再把同一个函数加上 `-mcpu=v1` 编译一次，说明为什么第二条指令的操作码从 `0x04` 变成了 `0x07`。

</Exercise>

<Exercise id="47105" :d="3" :w="3">

写一个只支持 `ldh`、`ldb`、`jeq`、`jgt`、`ret` 的 cBPF 解释器，用它执行 47102 第 3 问写出的 IPv4 TCP 过滤器。

要求：

1. 指令用 `struct { uint16_t code; uint8_t jt; uint8_t jf; uint32_t k; }` 表示，跳转按「下一条指令的编号加上 `jt` 或 `jf`」计算；
2. 读包之前检查偏移加宽度是否越过帧长，越过时按丢弃处理并打印一行提示；
3. 构造四个 64 字节的数组当作以太网帧：ARP（类型 `0x0806`）、IPv4 UDP（`0x0800`，协议号 17）、IPv4 TCP（`0x0800`，协议号 6）、IPv6（`0x86dd`），每行打印帧名和返回值；
4. 验收：ARP、IPv4 UDP、IPv6 返回 0，IPv4 TCP 返回 `0xffffffff`。把 `jeq` 换成 `jgt` 再跑一遍，说明结果为什么变了。

参考实现放在 `code/系统编程/拓展B_eBPF/B_1_eBPF概览/exercises/` 下，做完再对照。

</Exercise>

<Exercise id="47106" :d="3" :w="4">

用普通 C 模拟 3.2 节讲的「事件驱动、附加在钩子上」的执行模型，写 `hooksim.c`：

1. 定义三个事件 `EVENT_OPENAT`、`EVENT_WRITE`、`EVENT_XDP`，每个事件对应一张处理函数表，表项含事件、名字和函数指针；
2. 每个处理函数接收一个 `int` 参数并打印自己的名字，用来观察谁被调用；
3. `trigger(event, arg)` 只调用注册在该事件上的处理函数，同一事件上注册多个时全部按注册顺序调用；
4. 为每个事件记一个触发计数，程序结束时打印三个事件的次数，没有注册处理函数的 `EVENT_XDP` 也要打印；
5. 验收：依次触发 `openat`、`write`、`xdp`，输出中 openat 出现两行（两个处理函数各一行）、write 一行、xdp 零行，末尾打印 `openat=1 write=1 xdp=1`。

另外，用一张表把第 1 节的三条路与第 4 节的三个方向对上：每个方向举一个具体场景，写出它挂在哪个钩子（或哪种机制）上、为什么这个场景不该用内核模块。

最后回答两个问题：这段模拟在什么意义上对应 eBPF 的真实行为，在什么意义上不对应？如果 `EVENT_XDP` 上没有注册任何处理函数，真实内核里「附加了 XDP 程序但没有流量」与「根本没有附加程序」在可观测性上有什么区别？

</Exercise>

<Exercise id="47107" :d="5" :w="4">

写一个 `bpfobj.c`，不包含 `<elf.h>`，自己定义 ELF64 文件头与节头的字段布局，读取一个 `.bpf.o` 并输出：

1. `e_machine` 的值，并说明 247 对应哪一套指令集；
2. 所有名字以 `tp/`、`tracepoint/`、`kprobe/`、`xdp`、`cgroup/`、`lsm/`、`tcx/` 开头的节，打印节名与字节数；
3. `license` 节的字节数与内容，`.BTF` 与 `.BTF.ext` 是否存在；
4. 验收：对 `ret0.bpf.o` 的输出里有一行程序节、16 字节、machine 247；再写一个文件里放两个程序（`tp/syscalls/sys_enter_write` 与 `tp/syscalls/sys_enter_read`）的目标文件，输出两行、各 16 字节。

提示：节名字符串在 `e_shstrndx` 指向的那一节里，`sh_name` 是它的偏移；读文件里的任何结构都要先用 `fseek` 定位，不要假设节头在文件末尾。

参考实现放在 `code/系统编程/拓展B_eBPF/B_1_eBPF概览/exercises/` 下。

</Exercise>

### 6.3 综合与挑战

<Exercise id="47108" :d="6" :w="5">

写一个 `check.c`，在「加载」之前对一个 eBPF 指令数组做静态检查，检查四项：

1. 所有 opcode 都在支持的集合里（至少 `0xb7`、`0xb4`、`0x07`、`0x0f`、`0x05`、`0x55`、`0x95`）；
2. 每条指令的 `dst` 和 `src` 都不大于 10；
3. 无条件跳转与条件跳转的目标都落在 `[0, n)` 之内；
4. 最后一条指令是 `exit`，且从第 0 条出发，所有可达路径最终都能到达某条 `exit`。

用四类输入测试并打印可读的报错：正常的两条指令、缺少 `exit` 的程序、跳转越界的程序、寄存器编号为 11 的程序。

做完之后读 [B.9](/教程/正文/系统编程/拓展B_eBPF/B_9_验证器) 里的验证器日志片段，用一段话说明你的检查器与它差在哪里，至少说出两项你完全没有检查的东西。

</Exercise>

<Exercise id="47109" :d="7" :w="5" scope="*">

把 47105 的 cBPF 解释器与一个最小 eBPF 解释器放进同一个程序，用同一套过滤逻辑做对照：

1. eBPF 侧只实现 `mov32`/`mov64` 立即数、`add` 立即数、`ldx`、条件跳转、无条件跳转与 `exit`，寄存器用 `int64_t r[11]`；
2. 为 ARP 过滤器手写等价的 eBPF 指令序列，语义与 cBPF 版本相同；
3. 对 47105 的四类帧分别运行两个版本，打印两张结果表，断言两者一致；
4. 统计两个版本在每类帧上各执行多少条指令，说明两者的差别来自哪里；
5. 在报告里解释：两个版本的指令条数不同，为什么这不影响「两者语义相同」这个结论；再说明 eBPF 把 `jt`、`jf` 两个偏移换成一个偏移之后，同一条比较逻辑为什么常常要多出一条指令。

验收：四类帧的结果一致。做完之后把 eBPF 版本对应的目标文件用 `llvm-objdump -d` 反汇编一遍，说明反汇编器的助记符与你的写法差在哪里。

</Exercise>

## 7. 参考资料

- ebpf.io. _What is eBPF?_ [https://ebpf.io/what-is-ebpf/](https://ebpf.io/what-is-ebpf/)
- Steven McCanne, Van Jacobson. _The BSD Packet Filter: A New Architecture for User-level Packet Capture_（USENIX Winter 1993）[https://www.tcpdump.org/papers/bpf-usenix93.pdf](https://www.tcpdump.org/papers/bpf-usenix93.pdf)
- Linux 内核文档. _BPF and XDP Reference Guide_ / `Documentation/networking/filter.rst`（cBPF 的寄存器、指令编码、ARP 与 IPv4 TCP 示例、`bpf_asm` 的输出）[https://docs.kernel.org/networking/filter.html](https://docs.kernel.org/networking/filter.html)
- Linux 内核文档. _Classic BPF vs eBPF_（寄存器数量与宽度、指令格式的差异）[https://docs.kernel.org/bpf/classic_vs_extended.html](https://docs.kernel.org/bpf/classic_vs_extended.html)
- Linux 内核文档. _BPF Documentation_ [https://docs.kernel.org/bpf/](https://docs.kernel.org/bpf/)
- Linux 内核文档. _BPF Instruction Set Specification_ [https://docs.kernel.org/bpf/standardization/instruction-set.html](https://docs.kernel.org/bpf/standardization/instruction-set.html)
- D. Thaler (Ed.). _RFC 9669: BPF Instruction Set Architecture (ISA)_，2024 年 10 月 [https://www.rfc-editor.org/rfc/rfc9669](https://www.rfc-editor.org/rfc/rfc9669)
- Jonathan Corbet. _A thorough introduction to eBPF_，LWN.net，2017 年 12 月 [https://lwn.net/Articles/740157/](https://lwn.net/Articles/740157/)
- Brendan Gregg. _eBPF_ 主页（观测类工具的分类）[https://www.brendangregg.com/ebpf.html](https://www.brendangregg.com/ebpf.html)
- Brendan Gregg. _BPF CO-RE, BTF, and libbpf_，2020 年 11 月（BCC 的依赖体积、opensnoop 的体积）[https://www.brendangregg.com/blog/2020-11-04/bpf-co-re-btf-libbpf.html](https://www.brendangregg.com/blog/2020-11-04/bpf-co-re-btf-libbpf.html)
- Wikipedia. _eBPF_（版本与里程碑，写作时逐条核对）[https://en.wikipedia.org/wiki/EBPF](https://en.wikipedia.org/wiki/EBPF)
- Linux 内核发布说明：_Linux 3.5_（seccomp 过滤器）、_Linux 3.18_（eBPF 与 `bpf()`）、_Linux 4.1_（kprobe）、_Linux 4.8_（XDP）、_Linux 4.18_（BTF、AF_XDP）、_Linux 5.3_（有界循环）、_Linux 5.5_（BPF 蹦床）、_Linux 5.7_（BPF LSM）、_Linux 5.8_（环形缓冲区）、_Linux 6.12_（sched_ext）[https://www.kernelnewbies.org/](https://www.kernelnewbies.org/)
- bpf(2) 手册页（`bpf()` 自 Linux 3.18 起提供）[https://man7.org/linux/man-pages/man2/bpf.2.html](https://man7.org/linux/man-pages/man2/bpf.2.html)
- socket(7) 手册页（`SO_ATTACH_FILTER` 的返回值含义）[https://man7.org/linux/man-pages/man7/socket.7.html](https://man7.org/linux/man-pages/man7/socket.7.html)
- tcpdump(1) 手册页（`-d`、`-dd`、`-ddd` 三个选项）[https://www.tcpdump.org/manpages/tcpdump.1.html](https://www.tcpdump.org/manpages/tcpdump.1.html)
- Facebook Engineering. _Open-sourcing Katran, a scalable network load balancer_，2018 年 5 月 [https://engineering.fb.com/2018/05/22/open-source/open-sourcing-katran-a-scalable-network-load-balancer/](https://engineering.fb.com/2018/05/22/open-source/open-sourcing-katran-a-scalable-network-load-balancer/)
- Cloudflare Blog. _How to Drop 10 Million Packets per Second_，2018 年 7 月 [https://blog.cloudflare.com/how-to-drop-10-million-packets/](https://blog.cloudflare.com/how-to-drop-10-million-packets/)
- Cilium 文档. _Introduction to Cilium_ [https://docs.cilium.io/en/stable/overview/intro/](https://docs.cilium.io/en/stable/overview/intro/)
- Linux 内核文档. _sched_ext_ [https://docs.kernel.org/scheduler/sched-ext.html](https://docs.kernel.org/scheduler/sched-ext.html)
- Linux 内核文档. _HID-BPF_ [https://docs.kernel.org/hid/hid-bpf.html](https://docs.kernel.org/hid/hid-bpf.html)
- Microsoft Open Source Blog. _Making eBPF work on Windows_，2021 年 5 月 [https://opensource.microsoft.com/blog/2021/05/10/making-ebpf-work-on-windows/](https://opensource.microsoft.com/blog/2021/05/10/making-ebpf-work-on-windows/)
- Liz Rice. **_Learning eBPF_**，O'Reilly，2023，ISBN 978-1-098-13512-6（章序与术语参照）
- libbpf. _libbpf-bootstrap_（骨架与示例的写法）[https://github.com/libbpf/libbpf-bootstrap](https://github.com/libbpf/libbpf-bootstrap)
