# B.16 eBPF for Windows 与跨平台前景

在企业级网络策略与安全防护体系中，常有团队在 Linux 宿主机上借助 eBPF 构建高性能的流量过滤机制。然而，当部分业务节点需要迁移至 Windows Server 环境时，原有的 `.bpf.c` 探针能否平滑延续？如果直接把源文件复制到 Windows 下，依托同样支持 `-target bpf` 的 clang 编译器，依然能够产出合法的 ELF 目标文件；但若直接将 Linux 上编译好的 `.o` 二进制文件移交 Windows 环境，加载流程会在第一步便遭遇失败。这一对照揭示了当前 eBPF 跨平台生态的核心现状：在语言抽象与源码层面，已初步具备复用策略的可行性；但在底层二进制 ABI 与内核交互契约上，两大操作系统之间依然横亘着显著的异构鸿沟。

这里我们将拆解跨平台 eBPF 的深层机理：梳理 eBPF for Windows 项目在 Windows 体系结构中的时空定位，剖析其采用的验证模型、驱动级执行路径以及外挂式扩展钩子来源；随后逐条对比两大平台在辅助函数调用编号、上下文物理排布及结构体偏移上的深层差异，揭示为何相同的 C 语言表达式在不同宿主头文件下会生成走向完全不同的底层指令序列；最后，我们将从零构建一个能够同时兼顾两大平台的网络访问控制实例，剖析哪些逻辑能够通用、哪些机制必须设立平台适配层，并建立起对跨平台 eBPF 生态现状与演进前景的客观认知。

## 1. 为什么要在 Windows 上运行 eBPF

### 1.1 项目的目标

2021 年 5 月 10 日，微软在其技术博客正式公布了 eBPF for Windows 开源项目。该方案的核心思路并非在 Windows 内核中生硬地复刻整个 Linux 内核子系统，而是采用模块化组合策略，整合现有的跨平台开源成果并桥接至 Windows 的原生子系统：底层字节码依旧由成熟的 LLVM/clang 工具链生成；安全验证委托给基于抽象解释的形式化分析器 [PREVAIL](https://github.com/vbpf/ebpf-verifier)；运行时 JIT 与解释执行则吸收自 [uBPF](https://github.com/iovisor/ubpf) 项目；面向用户空间的编程接口则全方位贴合经典 libbpf 的命名范式。项目致力于抹平平台之间的工具链阻尼，让在 Linux 生态中沉淀的开发习惯与工具资产能够平稳落地至 Windows 平台。

项目的核心定位被明确界定为「源码兼容」（source code compatibility）：凡是调用双平台共有钩子点并使用通用辅助函数的探针源码，均可在两个平台上分别借助各自的宿主环境完成独立编译与挂载。而涉及 Linux 内部专属数据结构的特权级跟踪观测工具，则不在通用兼容的承诺范围内。该项目已将运行基线划定在 Windows 11 与 Windows Server 2022 及更高版本之上。

### 1.2 源码兼容不等于二进制兼容

在工程实践中，「源码兼容」容易被误读为「一次编译、处处运行」的跨平台二进制通用性。然而在底层实现中，eBPF for Windows 仅提供源码级语义支持，与 Linux 的二进制产物互不兼容。在 Linux 编译环境下产出的 ELF 目标文件无法直接注入 Windows 内核，反之亦然。两端借助各自系统头文件编译出的机器码在底层编号与内存布局上存在实质性分歧。

::: concept win-ebpf-source-compat title="源码兼容的含义"

同一份使用通用钩子与辅助函数的 C 源码，可以分别用两个平台的头文件编译成目标文件，并各自加载运行。两个平台的目标文件不能互换：辅助函数编号、程序类型编号、map 类型编号、钩子名称和上下文布局都是各自定义的，加载器按这些编号解释字节码，换一个平台就会解释成另一件事。

:::

造成这一现象的深层技术机理源于两者在 ABI 规范上的独立演化：辅助函数调用号、程序与 map 类型索引、底层探针挂接点以及上下文结构体的物理字节偏移，在两个操作系统内部均由各自的规范独立裁定。目标文件内部的硬编码常量在编译时便已彻底凝固，换到异构平台上会引起全然错乱的调用行为。

<Quiz
  type="single"
  :options="[
    { label: '可以，只要用 clang -target bpf 编译过就行', value: 'A' },
    { label: '可以，但要先用 bpf2c 转成 C 再编译', value: 'B' },
    { label: '不可以，两个平台只是源码兼容，目标文件不能互换', value: 'C' },
    { label: '可以，但只能加载到 JIT 模式', value: 'D' }
  ]"
  answer="C"
>

<template #question>

在 Linux 上用 clang 编好的 `.bpf.o`，能不能直接拿到安装了 eBPF for Windows 的机器上加载？

</template>

正确答案是 **C**。两个平台的辅助函数编号、程序类型编号和上下文布局都不同，加载器无从解释另一个平台的目标文件。第 3 节给出具体的编号和偏移。

</Quiz>

::: generate win-ebpf-source-compat-walk kind=explanation concepts=win-ebpf-source-compat

面向已经会用 Linux 上 libbpf 工具链、但对跨平台移植没有概念的读者。用一个具体例子说明「源码兼容」与「二进制兼容」的区别：同一份 `connect4` 程序在两边都能编译通过，但其中的辅助函数调用编号、上下文里字段的偏移、返回值的取值都不一样。不要总结，只把三处差异逐一讲清楚，并说明为什么这些差异在编译时就已经固定，加载阶段无从调整。

:::

## 2. 架构

### 2.1 三条执行路径

eBPF 指令流从源码落地至 Windows 内核的物理路径，与 Linux 的内核端原位处理存在根本性差异。在 Linux 体系中，内核原语 `bpf()` 系统调用能够原生承接原始字节码，并在内核特权上下文中就地完成验证与 JIT 机器码转译；而 Windows 内核传统上并未预设可随时接入不可信字节码的通用微内核子系统，为此项目规划了三条形态各异的落地路径。

```mermaid
flowchart TB
    SRC["connect_deny.bpf.c"] -->|"clang -target bpf"| OBJ["ELF 目标文件"]
    OBJ --> N["本机模式"]
    OBJ --> J["JIT 模式"]
    OBJ --> I["解释器"]
    N --> V1["bpf2c 调用 PREVAIL 验证"]
    V1 --> GEN["逐条指令生成等价的 C"]
    GEN --> CC["MSVC 编译并链接成 .sys 驱动"]
    SIGN --> EC["ebpfcore.sys 执行上下文"]
    CC --> SIGN["签名"]
    J --> V2["ebpfsvc 验证"]
    V2 --> U["uBPF 编译成本机代码"]
    U --> EC
    I --> EC
    EC --> HOOK["扩展驱动提供的钩子"]
    API["ebpfapi.dll"] --> EC
    TOOL["netsh ebpf、bpftool.exe"] --> API
```

三条路径的处理方式如下：

**本机模式（native）** 是生产部署的核心推荐路径。工具 `bpf2c` 首先将 ELF 目标文件递交 PREVAIL 分析器完成前置静态证明，确认安全后将逐条 eBPF 汇编指令一一映射为等价的 C 语言逻辑，随后借助宿主机的 MSVC 编译器直接构建并链接为标准 Windows 内核驱动文件（`.sys`），最后附带合法的证书签名。由驱动承载的机器指令为纯正的原生架构指令，能够完全契合 Windows 操作系统的代码完整性监管模型。

**JIT 模式** 依赖用户态常驻服务 `ebpfsvc.exe` 进行安全检查，随后调用嵌入的 uBPF JIT 模块将字节码动态编译为机器码，并传递给内核常驻的执行引擎。

**解释器模式** 则在内核上下文中逐条模拟执行字节码，通常仅用于本地开发调试阶段，在正式发布版本中处于剥离状态。

::: concept win-ebpf-native-mode title="本机模式"

本机模式把验证放在编译期：`bpf2c` 先用 PREVAIL 证明程序安全，再把每条指令翻译成等价的 C，最终产物是一个签过名的 Windows 驱动。因为产物是签名的 PE 文件，它在启用了虚拟机监控程序保护的代码完整性（Hypervisor-protected Code Integrity，HVCI）的系统上也能加载，而 JIT 出来的代码页没有可信签名，在 HVCI 下会被拒绝。官方文档因此把本机模式称为部署 eBPF 程序的首选方式。

:::

这种架构分立的核心根源，在于现代企业级 Windows 系统普遍启用的 HVCI（基于虚拟机监控程序保护的代码完整性）安全防护体系。在启用 HVCI 的受保护主机上，虚拟机监控程序严格监控所有内核内存页的执行权限，断然拒绝任何未经受信任公钥体系数字签名的动态代码页执行；JIT 引擎在内核态动态派生的机器码由于缺乏静态签名，将在 HVCI 严苛的检查机制下被直接封杀。而将 eBPF 程序提前转译并封装为合规签名的内核驱动，恰好能够在满足企业级安全合规的前提下绕开这一硬件级防护阻碍。

正因如此，传统的 JIT 动态路径逐步进入退役周期。社区 issue（编号 4997）已明确将移除未签名的 JIT / 解释执行机制列为既定路线，要求剥离其底层对 uBPF 的运行时依赖。在官方推荐的运行时组件安装选项中，默认仅激活本机代码生成路径。

<Quiz
  type="multi"
  :options="[
    { label: '产物是一个签名的 .sys 驱动', value: 'A' },
    { label: '验证在生成驱动时完成，不在加载时完成', value: 'B' },
    { label: '可以在启用了 HVCI 的系统上加载', value: 'C' },
    { label: '需要内核里有一个常驻的 JIT 编译器', value: 'D' }
  ]"
  :answer="['A', 'B', 'C']"
>

<template #question>

关于本机模式，下列说法哪些正确？

</template>

正确答案是 **A、B、C**。D 说的是 JIT 模式：JIT 需要 `ebpfsvc.exe` 和内核侧的编译产物，本机模式不需要。

</Quiz>

### 2.2 组件分工

Windows 上的 eBPF 由几个二进制组成，各自的位置如下。

| 组件 | 位置 | 作用 |
| --- | --- | --- |
| `ebpfapi.dll` | 用户态 | 对外的共享库，暴露与 libbpf 同名的 API，加载程序、操作 map 都经过它 |
| `ebpfnetsh.dll` | 用户态 | `netsh.exe` 的插件，提供 `netsh ebpf` 系列命令 |
| `ebpfsvc.exe` | 用户态 | 服务进程，负责 JIT 模式下的验证与编译 |
| `ebpfcore.sys` | 内核态 | 执行上下文，提供 map、辅助函数和程序的运行环境 |
| `netebpfext.sys` | 内核态 | 扩展驱动，用 Windows 过滤平台（Windows Filtering Platform，WFP）实现网络类钩子 |
| `bpftool.exe` | 用户态 | bpftool 的移植版本，基于 `ebpfapi.dll` |

内核里的执行上下文只负责「跑程序」和「管 map」，钩子由扩展驱动提供。这个划分和 Linux 不同：Linux 的钩子点分散在 kprobe、tracepoint、cgroup、XDP、LSM 各处，由内核本身实现；Windows 上每类钩子对应一个扩展驱动，要加新的钩子就加一个驱动，不动 `ebpfcore.sys`。网络钩子在 `netebpfext.sys`，进程创建与删除事件由另一个仓库 `ntosebpfext` 提供的扩展负责。

程序类型与 SEC 名称的对应关系放在注册表里，位置是 `HKCU:\Software\eBPF\Providers\SectionData`。每一个前缀对应一组 GUID 和编号，例如 `cgroup/connect4`、`sockops`、`xdp`、`sample_ext`。这解释了第 4 节里 `netsh ebpf show verification bpf.o type=sample_ext` 中那个 `type=` 参数：程序放在 `.text` 里，没有 SEC 名称可循，验证器不知道该按哪种上下文检查，只能在命令行上告诉它。

<Quiz
  type="single"
  :options="[
    { label: '内核里的验证器，与 Linux 相同', value: 'A' },
    { label: '用户态的 PREVAIL 验证器', value: 'B' },
    { label: '硬件虚拟化层', value: 'C' },
    { label: '不需要验证，靠签名保证安全', value: 'D' }
  ]"
  answer="B"
>

<template #question>

在 eBPF for Windows 中，由谁验证 eBPF 程序？

</template>

正确答案是 **B**。PREVAIL 是一个用抽象解释实现的静态分析器，跑在用户态。本机模式下它在 `bpf2c` 生成 C 之前运行，验证与生成在构建阶段完成；JIT 模式下由 `ebpfsvc.exe` 调用它。签名只说明这段二进制来自谁，不说明它安全，所以签名不能替代验证。

</Quiz>

::: generate win-ebpf-arch-walk kind=explanation concepts=win-ebpf-native-mode

面向熟悉 Linux 上 libbpf 加载流程、第一次接触 Windows 执行环境的读者。按「源码编译成目标文件之后发生了什么」的顺序，把本机模式的六步走一遍：验证、生成 C、编译 COFF、链接 PE、签名、由执行上下文加载。说明每一步由哪个组件完成、为什么要放在用户态或编译期，并对比 Linux 上「验证与 JIT 都在内核加载时完成」的做法。

:::

### 2.3 和 Linux 的架构图对照

将 [B.1](/教程/正文/系统编程/拓展B_eBPF/B_1_eBPF概览) 中给出的 Linux 原生架构图与 Windows 模型并排审视，核心差异主要投射在三个维度。

其一在于程序验证发生的时空节点与理论基础。Linux 内核选择将验证器固化在特权内核之中，当用户调用 `bpf()` 加载字节码时，内核自入口处逐条遍历所有执行路径，通过符号执行与分支裂变追踪虚拟寄存器的状态变迁；然而这种暴力展开路径的算法在遭遇复杂分支或深层循环时极易触发指令预算耗尽的状态爆炸。Windows 体系则将验证器完全前置至用户空间，并引入基于抽象解释（Abstract Interpretation）形式化理论的 PREVAIL 分析器。PREVAIL 能够在控制流分支汇合处执行状态的不动点合并（join），天然具备安全判定有限循环的能力。早在 2021 年社区针对该方案的技术综述中便曾指出其双向特征：由于抽象解释需维护格结构（Lattice）与值域约束，在分析大型程序时可能产生更高的内存驻留与更长的分析耗时；同时，它与 Linux 原生验证器在判定准则上并不存在严格的包含关系——存在一部分能被 PREVAIL 证明安全却被 Linux 验证器驳回的程序，反之亦然。两端能否放行特定源码，必须分别在各自环境中实测。

其二在于最终机器码的生成与寄宿方式。Linux 依赖内核原生 JIT 编译器动态翻译机器码并直接驻留内核可执行页；而在开启代码完整性监控（HVCI）的 Windows 环境下，未签名的 JIT 内存页无法通过校验，因此本机模式将这一过程完全置换为编译期动作：通过源码转译与 MSVC 编译，将 BPF 逻辑直接铸造为带有数字签名的正式内核驱动，借助操作系统的标准驱动装载机制载入。

其三在于跟踪点的拓展拓扑。Linux 内核内部将探针锚点原生地织入调度器、网络栈及系统调用表各处；而在 Windows 端，内核执行核心仅维持运行环境与 map 管理，具体的业务探针完全以模块化外挂驱动（如网络过滤扩展 `netebpfext.sys` 或进程事件扩展）的形式提供，实现机制与内核调度主体保持正交解耦。

## 3. 与 Linux eBPF 的差异

### 3.1 一张对照表

| 项目 | Linux | eBPF for Windows |
| --- | --- | --- |
| 验证 | 内核中的验证器，逐条路径模拟寄存器状态 | 用户态 PREVAIL，抽象解释，可提前到构建期 |
| 执行 | 加载时 JIT 成本机代码 | 预编译成签名的 `.sys` 驱动；JIT 模式可选，正在被移除 |
| 辅助函数编号 | `bpf_ktime_get_ns` 为 5，`bpf_get_current_pid_tgid` 为 14 | 分别为 9 和 19 |
| 程序类型编号 | `BPF_PROG_TYPE_XDP` 为 6，`BPF_PROG_TYPE_CGROUP_SOCK_ADDR` 为 18 | 分别为 1 和 3 |
| map 类型编号 | `BPF_MAP_TYPE_PERF_EVENT_ARRAY` 为 4，`RINGBUF` 为 27 | 分别为 14 和 13 |
| 钩子 | kprobe、tracepoint、fentry、LSM、cgroup、XDP、tc 等 | XDP、bind（已弃用）、cgroup sock_addr、sock_ops、进程事件；没有 kprobe 与 tracepoint |
| 上下文 | `struct bpf_sock_addr`，`user_ip4` 在偏移 4 | `bpf_sock_addr_t`，`user_ip4` 在偏移 24 |
| BTF 与 CO-RE | 内核 BTF 加加载期重定位 | 对象文件里有 BTF 节，但没有内核 BTF，没有 CO-RE |
| 许可证 | 部分辅助函数只对 GPL 兼容的程序开放 | 加载时忽略，`bpf_prog_load` 把许可证参数标为未使用 |
| 部署 | root 或相应能力 | 驱动必须签名；开发时开测试签名 |
| 系统要求 | 内核 6.6 以上（本章的 tcx 需要） | Windows 11、Windows Server 2022 及以上 |

表里的编号都取自两个平台的头文件。下面两小节说明这些编号是怎么产生的，以及它们为什么会直接改变编译结果。

### 3.2 编号不同

两个操作系统的内核均采用整型调用号标识各个辅助函数，然而各自维护的编号映射表却存在断层。在 Linux 端，`bpf_helper_defs.h` 是随内核演进由脚本直接从内核头文件中解析生成的衍生文件，`bpf_ktime_get_ns` 被赋予编号 5；而在 eBPF for Windows 早期实现中，接口规范由开发团队手工制定并按需收纳，同样的获取纳秒时间戳函数被排定至编号 9。程序类型与 map 存储类型的枚举常数亦呈现出不同程度的分歧：部分结构碰巧同值（如哈希表类型 `BPF_MAP_TYPE_HASH` 在双端均为 1），而另一些则相差甚远（如环形缓冲区 `BPF_MAP_TYPE_RINGBUF` 在 Linux 端为 27，而在 Windows 端为 13）。

下面这个程序只调用两个辅助函数，用两套头文件各编译一次，看反汇编里的 `call` 编号。完整文件在 `code/系统编程/拓展B_eBPF/B_16_eBPF_for_Windows与跨平台/helper_id.c`：

```c
#if defined(MDR_PLATFORM_WINDOWS)
#include "bpf_helpers.h"
#else
typedef unsigned long long __u64;
typedef unsigned int __u32;
static __u64 (*const bpf_ktime_get_ns)(void) = (void *)5;
static __u64 (*const bpf_get_current_pid_tgid)(void) = (void *)14;
#define SEC(NAME) __attribute__((section(NAME), used))
#endif

SEC("cgroup/connect4")
int probe_helpers(void *ctx) {
    __u64 t = bpf_ktime_get_ns();
    __u64 p = bpf_get_current_pid_tgid();

    (void)ctx;
    if (p == 0) {
        return (int)t;
    }
    return 0;
}
```

Linux 侧的两个函数指针故意按 `bpf_helper_defs.h` 里的编号写成 5 和 14，这样这个文件不依赖 `vmlinux.h` 也能编译。两次编译的反汇编如下。

::: terminal title="PowerShell - helper_id"

<span class="prompt">PS&gt;</span> clang -target bpf -Wall -Wextra -Werror -O2 -c helper_id.c -o helper_linux.o

<span class="prompt">PS&gt;</span> llvm-objdump --triple=bpf -S --no-print-imm-hex helper_linux.o

```text
helper_linux.o:  file format elf64-bpf

Disassembly of section cgroup/connect4:

0000000000000000 <probe_helpers>:
       0:      85 00 00 00 05 00 00 00     call 5
       1:      bf 06 00 00 00 00 00 00     r6 = r0
       2:      85 00 00 00 0e 00 00 00     call 14
       3:      15 00 01 00 00 00 00 00     if r0 == 0 goto +1 <probe_helpers+0x28>
       4:      b4 06 00 00 00 00 00 00     w6 = 0
       5:      bc 60 00 00 00 00 00 00     w0 = w6
       6:      95 00 00 00 00 00 00 00     exit
```

<span class="prompt">PS&gt;</span> clang -target bpf -Wall -Wextra -Werror -O2 -DMDR_PLATFORM_WINDOWS -I&lt;ebpf-for-windows&gt;\include -c helper_id.c -o helper_windows.o

<span class="prompt">PS&gt;</span> llvm-objdump --triple=bpf -S --no-print-imm-hex helper_windows.o

```text
helper_windows.o:  file format elf64-bpf

Disassembly of section cgroup/connect4:

0000000000000000 <probe_helpers>:
       0:      85 00 00 00 09 00 00 00     call 9
       1:      bf 06 00 00 00 00 00 00     r6 = r0
       2:      85 00 00 00 13 00 00 00     call 19
       3:      15 00 01 00 00 00 00 00     if r0 == 0 goto +1 <probe_helpers+0x28>
       4:      b4 06 00 00 00 00 00 00     w6 = 0
       5:      bc 60 00 00 00 00 00 00     w0 = w6
       6:      95 00 00 00 00 00 00 00     exit
```

:::

同一句 `bpf_ktime_get_ns()`，一边是 `call 5`，一边是 `call 9`；`bpf_get_current_pid_tgid()` 一边是 `call 14`，一边是 `call 19`。把 Linux 编出的目标文件交给 Windows 的执行上下文，第 0 条指令会去调用编号 5 的辅助函数，而在 Windows 的编号表里 5 号是 `bpf_tail_call`。同一个编号在两个平台指向不同的函数，字节码即使被接受，执行的行为也完全对不上，所以加载器干脆不接受别的平台的目标文件。

同样的道理适用于 map 类型：`SEC(".maps")` 里 `__uint(type, BPF_MAP_TYPE_RINGBUF)` 被翻译成一个数字写进 `maps` 节，Linux 写 27，Windows 写 13，内核按自己的表去建对象。

::: warning 两边的头文件不能混用

编译 Windows 目标文件时必须用 eBPF for Windows 的 `include` 目录，不能拿 Linux 的 `bpf_helpers.h` 顶替，也不能让两套头文件同时出现在 `-I` 路径里。混用不会报错，编译器只会采用先找到的那一份，编出的目标文件编号全错，直到加载时才暴露出来。

:::

<Quiz
  type="multi"
  :options="[
    { label: '同一份源码用两套头文件编译，辅助函数调用编号不同', value: 'A' },
    { label: '同一个 ctx 字段在两套头文件里偏移不同', value: 'B' },
    { label: 'map 类型编号在两边不同', value: 'C' },
    { label: 'Windows 上的目标文件仍是 ELF 格式', value: 'D' }
  ]"
  :answer="['A', 'B', 'C', 'D']"
>

<template #question>

关于两个平台的差异，下列说法哪些正确？

</template>

正确答案是 **A、B、C、D**。D 容易被忽略：Windows 的加载器读的仍然是 ELF 目标文件，产物变成 `.sys` 是后面 `bpf2c` 的事；目标文件的格式一样，里面的编号和布局不一样。

</Quiz>

### 3.3 同一个 ctx，两套布局

上下文结构体的差异比编号更隐蔽，因为它影响的是每一条访存指令的偏移。Linux 的 `struct bpf_sock_addr` 在 `vmlinux.h` 里，Windows 的 `bpf_sock_addr_t` 在 `ebpf_nethooks.h` 里，两者同名不同构。

下面这个程序把两边定义的关键字段偏移打出来。完整文件在 `code/系统编程/拓展B_eBPF/B_16_eBPF_for_Windows与跨平台/ctx_layout.c`：

```c
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "ebpf_nethooks.h"

/* Linux 侧：vmlinux.h 中 struct bpf_sock_addr 的字段，按原样抄录。 */
struct bpf_sock_addr_linux {
    uint32_t user_family;
    uint32_t user_ip4;
    uint32_t user_ip6[4];
    uint32_t user_port;
    uint32_t family;
    uint32_t type;
    uint32_t protocol;
    uint32_t msg_src_ip4;
    uint32_t msg_src_ip6[4];
    void *sk;
};

#define SHOW(label, type)                                                                                          \
    printf("%-8s %-16s size %2lu  user_ip4 @ %2lu  user_port @ %2lu  protocol @ %2lu\n", label, #type,              \
           (unsigned long)sizeof(type), (unsigned long)offsetof(type, user_ip4),                                   \
           (unsigned long)offsetof(type, user_port), (unsigned long)offsetof(type, protocol))

int main(void) {
    SHOW("Linux", struct bpf_sock_addr_linux);
    SHOW("Windows", bpf_sock_addr_t);
    return 0;
}
```

这个程序在 Windows 上用 MinGW 编译运行，输出如下。

::: terminal title="PowerShell - ctx_layout"

<span class="prompt">PS&gt;</span> gcc -std=c17 -Wall -Wextra -I&lt;ebpf-for-windows&gt;\include ctx_layout.c -o ctx_layout.exe

<span class="prompt">PS&gt;</span> ./ctx_layout.exe

```text
Linux    struct bpf_sock_addr_linux size 72  user_ip4 @  4  user_port @ 24  protocol @ 36
Windows  bpf_sock_addr_t  size 64  user_ip4 @ 24  user_port @ 40  protocol @ 44
```

:::

两个结构体都只有十来个数组成员，为什么偏移差这么多？Linux 的定义把 `user_family`、`user_ip4`、`user_ip6`、`user_port` 放在最前面，所以 `user_ip4` 在第 4 个字节。Windows 的定义把 `family` 放第一个，然后是一个匿名结构体：

```c
typedef struct bpf_sock_addr
{
    uint32_t family;
    struct
    {
        union { uint32_t msg_src_ip4; uint32_t msg_src_ip6[4]; };
        uint16_t msg_src_port;
    };
    struct
    {
        union { uint32_t user_ip4; uint32_t user_ip6[4]; };
        uint16_t user_port;
    };
    uint32_t protocol;
    uint32_t compartment_id;
    uint64_t interface_luid;
} bpf_sock_addr_t;
```

匿名结构体里的联合体占 16 字节，端口占 2 字节，整个结构体按 4 字节对齐，于是每个匿名结构体占 20 字节。`family` 4 字节加上第一个匿名结构体的 20 字节，`user_ip4` 落在 24；再加 20 字节，第二个匿名结构体的 `user_port` 落在 40。

这个差别直接体现在汇编里。下一节的两个版本的 `connect4` 程序，Linux 版读 `user_ip4` 用的是 `*(u32 *)(r1 + 4)`，Windows 版用的是 `*(u32 *)(r1 + 24)`。同一个字段，两套偏移。

还有一处更细的差别：`user_port` 在 Linux 的 `vmlinux.h` 里是 `__u32`，在 `ebpf_nethooks.h` 里是 `uint16_t`。所以 Linux 版读 4 个字节再与 `0xffff` 取掩码，Windows 版直接读 2 个字节。

两个结构体里还有一处不能靠 sizeof 看出来的差别：字段的语义。`bpf_sock_addr_t` 的注释写明，`user_ip4` 在出方向（connect）时是远端地址、在入方向（recv_accept）时是本地地址；`msg_src_ip4` 则相反。Linux 的 `vmlinux.h` 没有这些注释，得去查 `bpf_sock_addr_kern` 和 cgroup 钩子的文档。写跨平台程序时，字段名的相似会掩盖这层差别。

### 3.4 没有 BTF 就没有 CO-RE

[B.8](/教程/正文/系统编程/拓展B_eBPF/B_8_libbpf_BTF与CO_RE) 讲过 CO-RE 的三方分工：clang 在编译时记录每一次结构体成员访问，libbpf 在加载时读取目标内核的 BTF，按字段名重新算出偏移，把指令里的偏移改掉。这一套要成立，必须有一份描述目标内核类型布局的 BTF。

Windows 没有这样一份 BTF。Windows 内核的类型信息不以 BTF 的形式公开，eBPF for Windows 也不提供。加载接口里的 `core_relos` 字段注释直接写着「Not supported, must be zero」。对象文件里的 `.BTF` 节照常有，那是 clang 为程序自身的类型生成的，`bpf2c` 用它解析 `.maps` 节里的 map 定义、辅助函数原型和全局对象，和 CO-RE 是两回事。

结果是：在 Windows 上，程序里的结构体偏移就是编译时算好的偏移，没有任何东西会在加载时修正它。所以 `connect_deny.bpf.c` 里的 `ctx->user_ip4` 必须对着 `ebpf_nethooks.h` 编译，偏移才对得上；如果拿 Linux 的 `vmlinux.h` 编，偏移差了 20 字节，程序读到的是另一个字段。

::: tip Windows 上的「可移植」指什么

没有 CO-RE，不代表 eBPF for Windows 的程序只能在一种 Windows 版本上跑：`ebpf_nethooks.h` 里的上下文结构由项目自己维护，跨 Windows 版本是稳定的。不能移植的是「为 Linux 内核结构体写的观测程序」——`task_struct`、`file`、`sock` 这些结构在 Windows 上根本不存在，也没有 BTF 可供修正。

:::

### 3.5 安装与签名

在 Linux 上加载 eBPF 程序只需要 root 或几个能力位。在 Windows 上，本机模式的产物是内核驱动，加载它要满足下面三条之一：

1. 驱动由微软的代码签名根签发；
2. 机器上挂了内核调试器；
3. 系统开启了测试签名模式，驱动带测试签名，测试证书已安装。

项目自己的二进制目前还没有微软的生产签名，所以只能按后两条来。官方文档的建议是：准备一台虚拟机，打开测试签名，在其中安装。这也是本页不在本机安装 eBPF for Windows 的原因。

安装走 MSI，安装目录默认是 `C:\Program Files\ebpf-for-windows`。装完之后要把 `netsh` 的插件挂上，命令是 `netsh add helper <path>\ebpfnetsh.dll`，路径必须是绝对路径。之后 `netsh ebpf` 系列子命令才可用。

## 4. 在 Windows 上编译与检查

这一节跟随官方教程的第二部分：写一个只有一句 `return 0` 的程序，编译、反汇编、看节表，再用 `pragma` 把两个函数分到不同的节。命令与官方教程一致，没有加 `-mcpu`——教程没写这个选项，加上反而不好对照。

本机用的是 LLVM 22.1.3 的 `clang` 与 `llvm-objdump`，比教程要求的 18.1.8 新，指令层面没有差别，显示上有三处不同：函数名前面多打一行地址（`0000000000000000 <func>:`，教程里是 `func:`）；立即数默认按十六进制打印，所以下面的命令都加了 `--no-print-imm-hex`，不加会把 `r0 = 0` 显示成 `r0 = 0x0`；`int` 返回值用 32 位子寄存器 `w0` 承载，指令字节 `b4` 与教程的 `b7` 同属立即数移动（`b4` 是 32 位，`b7` 是 64 位），返回 0 时两者等价。

### 4.1 最小程序

文件 `bpf.c` 只有三行：

```c
int func()
{
    return 0;
}
```

编译并反汇编：

::: terminal title="PowerShell - bpf"

<span class="prompt">PS&gt;</span> clang -target bpf -Werror -O2 -c bpf.c -o bpf.o

<span class="prompt">PS&gt;</span> llvm-objdump --triple=bpf -S --no-print-imm-hex bpf.o

```text
bpf.o:  file format elf64-bpf

Disassembly of section .text:

0000000000000000 <func>:
       0:      b4 00 00 00 00 00 00 00     w0 = 0
       1:      95 00 00 00 00 00 00 00     exit
```

:::

两条指令。`w0` 是返回值寄存器 `r0` 的低 32 位，先置 0，再 `exit`。这和 [B.4](/教程/正文/系统编程/拓展B_eBPF/B_4_程序的解剖_受限的C) 讲的寄存器约定一致：R0 放返回值，R10 是帧指针。

### 4.2 加调试信息

加 `-g` 再编一份，反汇编里就能看到源码行：

::: terminal title="PowerShell - bpf-d"

<span class="prompt">PS&gt;</span> clang -target bpf -Werror -g -O2 -c bpf.c -o bpf-d.o

<span class="prompt">PS&gt;</span> llvm-objdump --triple=bpf -S --no-print-imm-hex bpf-d.o

```text
bpf-d.o:  file format elf64-bpf

Disassembly of section .text:

0000000000000000 <func>:
;     return 0;
       0:      b4 00 00 00 00 00 00 00     w0 = 0
       1:      95 00 00 00 00 00 00 00     exit
```

:::

再加 `-l` 会多打文件与行号。默认写的是编译时的绝对路径；编译时加 `-fdebug-compilation-dir=.`，输出里就是相对路径：

::: terminal title="PowerShell - 带行号的反汇编"

<span class="prompt">PS&gt;</span> clang -target bpf -Werror -g -O2 -fdebug-compilation-dir=. -c bpf.c -o bpf-dl.o

<span class="prompt">PS&gt;</span> llvm-objdump --triple=bpf -S -l bpf-dl.o

```text
bpf-dl.o:  file format elf64-bpf

Disassembly of section .text:

0000000000000000 <func>:
; func():
; .\bpf.c:3
;     return 0;
       0:      b4 00 00 00 00 00 00 00     w0 = 0x0
       1:      95 00 00 00 00 00 00 00     exit
```

:::

加 `-g` 的代价是目标文件里多出一批节。用 `-h` 看两个文件的节表：

::: terminal title="PowerShell - llvm-objdump -h"

<span class="prompt">PS&gt;</span> llvm-objdump --triple=bpf -h bpf.o

```text
bpf.o:  file format elf64-bpf

Sections:
Idx Name          Size     VMA              Type
  0               00000000 0000000000000000
  1 .strtab       00000030 0000000000000000
  2 .text         00000010 0000000000000000 TEXT
  3 .llvm_addrsig 00000000 0000000000000000
  4 .symtab       00000048 0000000000000000
```

<span class="prompt">PS&gt;</span> llvm-objdump --triple=bpf -h bpf-d.o

```text
bpf-d.o:  file format elf64-bpf

Sections:
Idx Name                   Size     VMA              Type
  0                        00000000 0000000000000000
  1 .strtab                000000c3 0000000000000000
  2 .text                  00000010 0000000000000000 TEXT
  3 .debug_abbrev          0000003a 0000000000000000 DEBUG
  4 .debug_info            00000037 0000000000000000 DEBUG
  5 .rel.debug_info        00000040 0000000000000000
  6 .debug_str_offsets     0000001c 0000000000000000 DEBUG
  7 .rel.debug_str_offsets 00000050 0000000000000000
  8 .debug_str             000000a9 0000000000000000 DEBUG
  9 .debug_addr            00000010 0000000000000000 DEBUG
 10 .rel.debug_addr        00000010 0000000000000000
 11 .BTF                   00000099 0000000000000000
 12 .BTF.ext               00000050 0000000000000000
 13 .rel.BTF.ext           00000020 0000000000000000
 14 .debug_frame           00000028 0000000000000000 DEBUG
 15 .rel.debug_frame       00000020 0000000000000000
 16 .debug_line            00000059 0000000000000000 DEBUG
 17 .rel.debug_line        00000030 0000000000000000
 18 .debug_line_str        0000003b 0000000000000000 DEBUG
 19 .llvm_addrsig          00000000 0000000000000000
 20 .symtab                00000108 0000000000000000
```

:::

只有 `.text` 带 `TEXT` 标记。这一点在 Windows 上有实际意义：`netsh ebpf show sections` 只列出非空且带 `TEXT` 的节，因为它找的是程序。

`.BTF` 与 `.BTF.ext` 两节值得记下来。前者是程序自身的类型信息，后者是函数与行号信息，以及 CO-RE 的重定位记录。第 3.4 节会说明，Windows 上有这两节，CO-RE 却用不了。

### 4.3 用 pragma 分节

一个源文件可以放多个程序，办法是把函数分到不同的节。`SEC` 宏是一种写法，`#pragma clang section` 是另一种：pragma 之后的函数都进指定的节，直到下一个 pragma。

```c
#pragma clang section text="myprog"

int func()
{
    return 0;
}

#pragma clang section text="another"

int anotherfunc()
{
    return 1;
}
```

编译后看节表：

::: terminal title="PowerShell - bpf2"

<span class="prompt">PS&gt;</span> clang -target bpf -Werror -O2 -c bpf2.c -o bpf2.o

<span class="prompt">PS&gt;</span> llvm-objdump --triple=bpf -h bpf2.o

```text
bpf2.o:  file format elf64-bpf

Sections:
Idx Name          Size     VMA              Type
  0               00000000 0000000000000000
  1 .strtab       00000047 0000000000000000
  2 .text         00000000 0000000000000000 TEXT
  3 myprog        00000010 0000000000000000 TEXT
  4 another       00000010 0000000000000000 TEXT
  5 .llvm_addrsig 00000000 0000000000000000
  6 .symtab       00000060 0000000000000000
```

<span class="prompt">PS&gt;</span> llvm-objdump --triple=bpf -S --section=myprog --no-print-imm-hex bpf2.o

```text
bpf2.o:  file format elf64-bpf

Disassembly of section myprog:

0000000000000000 <func>:
       0:      b4 00 00 00 00 00 00 00     w0 = 0
       1:      95 00 00 00 00 00 00 00     exit
```

<span class="prompt">PS&gt;</span> llvm-objdump --triple=bpf -S --section=another --no-print-imm-hex bpf2.o

```text
bpf2.o:  file format elf64-bpf

Disassembly of section another:

0000000000000000 <anotherfunc>:
       0:      b4 00 00 00 01 00 00 00     w0 = 1
       1:      95 00 00 00 00 00 00 00     exit
```

:::

`.text` 还在，但大小是 0——两个函数都搬走了。`myprog` 返回 0，`another` 返回 1。

### 4.4 用 netsh 验证与查看

下面这几条命令要把程序加载进驱动，本机没有安装 eBPF for Windows，输出照抄官方教程的示例，标为「输出类似」。

`netsh ebpf show sections` 列举文件里的程序节：

::: terminal title="netsh - show sections"

```text
netsh> ebpf show sections bpf2.o

             Section    Type  # Maps    Size
====================  ======  ======  ======
              myprog  unspec       0       2
             another  unspec       0       2
```

:::

和 `llvm-objdump -h` 的结果对照，`netsh` 少列了 `.text`（大小 0）和所有非代码节，只留下两个有内容的程序节。`Size` 的单位是指令条数，两条指令显示为 2。

验证一个程序：

::: terminal title="netsh - show verification"

```text
netsh> ebpf show verification bpf.o type=sample_ext

Verification succeeded
Program terminates within 6 instructions
```

:::

当 ELF 目标文件中仅包含单个非空代码节时，验证器默认选取该节进行分析；若包含多个程序节，则默认截取排在最前的段落（例如在 `bpf2.o` 中即为 `myprog`）。若需针对指定节段实施验证，可直接在文件名后追加节名参数：`netsh ebpf show verification bpf2.o another type=sample_ext`。

显式指定 `type=sample_ext` 是必不可少的。因为 `bpf.c` 中的函数直接归入通用的 `.text` 节区，缺乏具备特定前缀的 SEC 命名语义，分析器无从推断其运行上下文结构，必须通过命令行显式注入程序类型。示例扩展驱动注册的测试类型 `sample_ext` 在注册表中被赋予专用类型编号 999。

若在命令末尾追加 `level=verbose` 参数，便能完整窥见 PREVAIL 静态分析器逐步推演证明的详细心智模型。这段输出生动诠释了形式化抽象解释的计算过程：在每条待分析指令的前后，均会显式推导出一组代表安全约束的「不变式」（invariant），明确列出当前点位各虚拟寄存器的抽象类型标注与严谨的值域闭区间。

::: details PREVAIL 的逐步输出（摘自官方教程）

```text
netsh> ebpf show verification bpf.o type=sample_ext level=verbose

Pre-invariant : [
    instruction_count=0,
    meta_offset=[-4098, 0],
    packet_size=[0, 65534],
    r1.ctx_offset=0, r1.type=ctx, r1.value=[1, 2147418112],
    r10.numeric_size=0, r10.stack_offset=512, r10.type=stack, r10.value=[512, 2147418112]]
Stack: Numbers -> {}
entry:
  goto 0;

Post-invariant: [
    instruction_count=1,
    ...
0:
  r0 = 0;
  goto 1;
...
Verification succeeded
Program terminates within 6 instructions
```

在初始不变式状态中，`r10.type=stack, r10.stack_offset=512` 确立了 R10 始终锚定在大小为 512 字节的安全私有栈顶；`r1.type=ctx` 则标明首个输入参数为只读上下文指针。在推演管线中，程序每模拟执行一条指令，分析器便将演进后的约束条件代入后置不变式；一旦遭遇条件分支，则在两条潜在流向路径上分别收窄对应寄存器的取值区间，并在最终控制流交汇处实施凸包合并（join）。这套基于格理论的区间传播机制，与 Linux 验证器基于路径分裂与状态修剪的模型在数学层面上殊途同归。

:::

装载程序用 `add program`，查看用 `show programs`：

::: terminal title="netsh - add program"

```text
netsh> ebpf add program .\myxdp.o xdp_test
netsh> ebpf show programs

    ID  Pins  Links  Mode       Type           Name
======  ====  =====  =========  =============  ====================
 65568     1      1  JIT        xdp_test       my_xdp_parser
```

:::

`Mode` 一列会显示 `JIT`、`INTERPRET` 或 `NATIVE`。本机模式装进去的程序显示 `NATIVE`，因为它在编译期就已经是本机代码。`Pins` 是从命名空间里引用它的路径数，`Links` 是附加点数量。

## 5. 一份源码，两个平台

### 5.1 按平台切换的头文件

[B.13](/教程/正文/系统编程/拓展B_eBPF/B_13_安全应用_seccomp_LSM与cgroup) 写过一个 `connect_deny.bpf.c`，挂在 `cgroup/connect4` 上拒绝连接 `127.0.0.1:20002`。那一版是 Linux 专用的。要让它同时能编到 Windows，需要改三处：上下文类型名、两个返回值的取值、头文件的包含方式。

共用策略放在一个小头文件里，它不包含任何平台头。完整文件在 `code/系统编程/拓展B_eBPF/B_16_eBPF_for_Windows与跨平台/connect_deny.h`：

```c
/* 两个平台共用的部分：策略常量。
 *
 * 头文件不包含任何平台头，两边都能直接包含。
 */
#ifndef CONNECT_DENY_H
#define CONNECT_DENY_H

/* 拒绝的目标，地址按主机字节序写，端口按主机字节序写 */
#define DENY_ADDR 0x7f000001u /* 127.0.0.1 */
#define DENY_PORT 20002

#endif
```

主体用条件编译切换平台差异。完整文件在 `code/系统编程/拓展B_eBPF/B_16_eBPF_for_Windows与跨平台/connect_deny.bpf.c`：

```c
#if defined(MDR_PLATFORM_WINDOWS)
#include "bpf_endian.h"
#include "bpf_helpers.h"
typedef bpf_sock_addr_t connect_ctx_t;
#define VERDICT_ALLOW BPF_SOCK_ADDR_VERDICT_PROCEED_SOFT
#define VERDICT_DENY BPF_SOCK_ADDR_VERDICT_REJECT
#else
#include "vmlinux.h"
#include <bpf/bpf_endian.h>
#include <bpf/bpf_helpers.h>
typedef struct bpf_sock_addr connect_ctx_t;
#define VERDICT_ALLOW 1
#define VERDICT_DENY 0
#endif

#include "connect_deny.h"

struct {
    __uint(type, BPF_MAP_TYPE_ARRAY);
    __uint(max_entries, 1);
    __type(key, __u32);
    __type(value, __u64);
} blocked SEC(".maps");

SEC("cgroup/connect4")
int connect4(connect_ctx_t *ctx) {
    if (bpf_ntohl(ctx->user_ip4) == DENY_ADDR && bpf_ntohs(ctx->user_port) == DENY_PORT) {
        __u32 key = 0;
        __u64 *count = bpf_map_lookup_elem(&blocked, &key);

        if (count != 0) {
            __sync_fetch_and_add(count, 1);
        }
        return VERDICT_DENY;
    }
    return VERDICT_ALLOW;
}

char LICENSE[] SEC("license") = "Dual BSD/GPL";
```

条件编译只覆盖三类差异，其余部分两边完全一样：许可证、map 定义、判断条件、计数和返回值的选择。这就是「源码兼容」在实际代码里的样子。

::: compare left-title="Linux：1 放行、0 拒绝" right-title="Windows：枚举值决定放行还是拒绝"

```c
typedef struct bpf_sock_addr connect_ctx_t;
#define VERDICT_ALLOW 1
#define VERDICT_DENY 0
```

---

```c
typedef bpf_sock_addr_t connect_ctx_t;
#define VERDICT_ALLOW BPF_SOCK_ADDR_VERDICT_PROCEED_SOFT
#define VERDICT_DENY BPF_SOCK_ADDR_VERDICT_REJECT
```

:::

左栏是 Linux 的约定：`cgroup/connect4` 的返回值按最低位解释，1 放行，0 拒绝，拒绝时 `connect()` 得到 `EPERM`。右栏是 Windows 的约定：`ebpf_sock_addr_verdict_t` 有三个取值，`REJECT` 硬拒绝，`PROCEED_SOFT` 软放行，`PROCEED_HARD` 硬放行；`PROCEED_SOFT` 在 WFP 里对应软允许，后面的策略层还能否掉它。

这里有一处巧合值得说清楚。枚举值从 0 起编号，`REJECT` 是 0、`PROCEED_SOFT` 是 1，与 Linux 的「0 拒绝、1 放行」正好对上。本机的程序打印出来是（完整文件在 `code/系统编程/拓展B_eBPF/B_16_eBPF_for_Windows与跨平台/verdict.c`）：

::: terminal title="PowerShell - verdict"

<span class="prompt">PS&gt;</span> gcc -std=c17 -Wall -Wextra -I&lt;ebpf-for-windows&gt;\include verdict.c -o verdict.exe

<span class="prompt">PS&gt;</span> ./verdict.exe

```text
REJECT        = 0
PROCEED_SOFT  = 1
PROCEED_HARD  = 2
```

:::

把 Windows 版的 `VERDICT_*` 换成 1 和 0，编译出的字节码与原来一模一样，行为也正确。那为什么还要按平台写宏？因为两个理由。其一，数值相同是枚举声明顺序的偶然结果，两个平台并没有约定这个值；枚举哪天调整顺序，硬编码 1 和 0 的程序就静默改变了语义。其二，`PROCEED_HARD` 在 Linux 的「1 放行」里没有对应物，需要硬放行时必须用 Windows 的名字。用枚举写，读代码的人一眼就知道返回值是三种判定之一，不会当成随便一个非零数。

第二个理由同时也是一处容易出错的地方：在 Windows 上返回 `2` 是硬放行，在 Linux 上返回 `2` 仍然是「最低位为 0，拒绝」。这类差别编译器发现不了，只有在目标机上跑才看得出来。

### 5.2 两个版本的目标文件

Linux 版用本章统一的命令编译，这里为了和 Windows 版对照，去掉了 `-mcpu`：

::: terminal title="bash - 编译 Linux 版"

```text
$ clang -target bpf -Wall -Wextra -Werror -g -O2 -I. -c connect_deny.bpf.c -o connect_deny.bpf.o
$ llvm-objdump --triple=bpf -S --no-print-imm-hex connect_deny.bpf.o

connect_deny.bpf.o:     file format elf64-bpf

Disassembly of section cgroup/connect4:

0000000000000000 <connect4>:
; int connect4(connect_ctx_t *ctx) {
       0:      b4 00 00 00 01 00 00 00     w0 = 1
;     if (bpf_ntohl(ctx->user_ip4) == DENY_ADDR && bpf_ntohs(ctx->user_port) == DENY_PORT) {
       1:      61 12 04 00 00 00 00 00     w2 = *(u32 *)(r1 + 4)
       2:      56 02 0e 00 7f 00 00 01     if w2 != 16777343 goto +14 <connect4+0x88>
       3:      61 11 18 00 00 00 00 00     w1 = *(u32 *)(r1 + 24)
       4:      54 01 00 00 ff ff 00 00     w1 &= 65535
       5:      56 01 0b 00 4e 22 00 00     if w1 != 8782 goto +11 <connect4+0x88>
       6:      b4 01 00 00 00 00 00 00     w1 = 0
;         __u32 key = 0;
       7:      63 1a fc ff 00 00 00 00     *(u32 *)(r10 - 4) = w1
       8:      bf a2 00 00 00 00 00 00     r2 = r10
       9:      07 02 00 00 fc ff ff ff     r2 += -4
;         __u64 *count = bpf_map_lookup_elem(&blocked, &key);
      10:      18 01 00 00 00 00 00 00 00 00 00 00 00 00 00 00     r1 = 0 ll
      12:      85 00 00 00 01 00 00 00     call 1
;         if (count != 0) {
      13:      15 00 02 00 00 00 00 00     if r0 == 0 goto +2 <connect4+0x80>
      14:      b7 01 00 00 01 00 00 00     r1 = 1
;             __sync_fetch_and_add(count, 1);
      15:      db 10 00 00 01 00 00 00     r1 = atomic_fetch_add((u64 *)(r0 + 0), r1)
      16:      b4 00 00 00 00 00 00 00     w0 = 0
; }
      17:      95 00 00 00 00 00 00 00     exit
```

:::

Windows 版用同一份源文件、加上宏、换一组头文件：

::: terminal title="PowerShell - 编译 Windows 版"

<span class="prompt">PS&gt;</span> clang -target bpf -Wall -Wextra -Werror -O2 -g -DMDR_PLATFORM_WINDOWS -I&lt;ebpf-for-windows&gt;\include -I. -c connect_deny.bpf.c -o connect_deny_win.bpf.o

<span class="prompt">PS&gt;</span> llvm-objdump --triple=bpf -S --no-print-imm-hex connect_deny_win.bpf.o

```text
connect_deny_win.bpf.o:     file format elf64-bpf

Disassembly of section cgroup/connect4:

0000000000000000 <connect4>:
; int connect4(connect_ctx_t *ctx) {
       0:      b4 00 00 00 01 00 00 00     w0 = 1
;     if (bpf_ntohl(ctx->user_ip4) == DENY_ADDR && bpf_ntohs(ctx->user_port) == DENY_PORT) {
       1:      61 12 18 00 00 00 00 00     w2 = *(u32 *)(r1 + 24)
       2:      56 02 0d 00 7f 00 00 01     if w2 != 16777343 goto +13 <connect4+0x80>
       3:      69 11 28 00 00 00 00 00     w1 = *(u16 *)(r1 + 40)
       4:      56 01 0b 00 4e 22 00 00     if w1 != 8782 goto +11 <connect4+0x80>
       5:      b4 01 00 00 00 00 00 00     w1 = 0
;         __u32 key = 0;
       6:      63 1a fc ff 00 00 00 00     *(u32 *)(r10 - 4) = w1
       7:      bf a2 00 00 00 00 00 00     r2 = r10
       8:      07 02 00 00 fc ff ff ff     r2 += -4
;         __u64 *count = bpf_map_lookup_elem(&blocked, &key);
       9:      18 01 00 00 00 00 00 00 00 00 00 00 00 00 00 00     r1 = 0 ll
      11:      85 00 00 00 01 00 00 00     call 1
;         if (count != 0) {
      12:      15 00 02 00 00 00 00 00     if r0 == 0 goto +2 <connect4+0x78>
      13:      b7 01 00 00 01 00 00 00     r1 = 1
;             __sync_fetch_and_add(count, 1);
      14:      db 10 00 00 01 00 00 00     r1 = atomic_fetch_add((u64 *)(r0 + 0), r1)
      15:      b4 00 00 00 00 00 00 00     w0 = 0
; }
      16:      95 00 00 00 00 00 00 00     exit
```

:::

两段反汇编并排看，得到四个具体结论。

第一，`user_ip4` 的偏移是 `+4` 和 `+24`，`user_port` 是 `+24` 和 `+40`，与 3.3 节量出来的布局一致。同一个字段名，编译成了两个不同的偏移。

第二，`user_port` 的宽度不同：Linux 版读 `u32` 再取低 16 位，Windows 版直接读 `u16`。这是两份头文件里字段类型不同造成的，与优化无关。

第三，字节序转换没有出现在指令里。源码写的是 `bpf_ntohs(ctx->user_port) == DENY_PORT`，反汇编里却只有一个常数比较，常数是 `8782`。8782 是 `0x224E`，正好是 `20002`（`0x4E22`）的高低字节互换。clang 把「取反字节序再和常数比」改写成「直接和字节序相反的常数比」，省掉了一条 `bswap`。同理，`0x7f000001` 在指令里写作 `16777343`，即 `0x0100007F`。读反汇编时看到这种数，先按字节序换算一遍再判断是不是 bug。

第四，`__sync_fetch_and_add` 两边都生成 `db` 开头的 64 位原子指令。这一条源于 clang 版本，与平台无关：clang 22 生成的是 `db 10 00 00 01 …`，立即数字节为 `01`，带 `BPF_FETCH` 位，所以显示成 `atomic_fetch_add`；clang 18 生成 `db 10 00 00 00 …`，不带 FETCH 位，显示成 `lock +=`。两者语义相同——这里不关心旧值，省掉 FETCH 位少一次寄存器写入。判断原子操作是否生成了，看首字节 `db` 比看助记符可靠。

::: tip 读反汇编时先看字节

`llvm-objdump` 给 eBPF 打印的助记符在不同版本之间改过几次，同一份字节可能显示成 `r0 = 1` 或 `w0 = 1`，`lock *(u64 *)(r0 + 0) += r1` 或 `atomic_fetch_add((u64 *)(r0 + 0), r1)`。字节那一列是权威的，助记符只是它的解释。对照 RFC 9669 的指令表，`0xb7` 是 64 位立即数移动，`0xb4` 是 32 位立即数移动，`0xdb` 是 64 位原子操作，第 5 个字节的低 4 位是原子操作的子操作码。

:::

两个目标文件的节表还有一处差别：

::: terminal title="PowerShell - 两个版本的节表"

<span class="prompt">PS&gt;</span> llvm-objdump --triple=bpf -h connect_deny_win.bpf.o

```text
Sections:
Idx Name                   Size     VMA              Type
  0                        00000000 0000000000000000
  1 .strtab                0000013a 0000000000000000
  2 .text                  00000000 0000000000000000 TEXT
  3 cgroup/connect4        00000088 0000000000000000 TEXT
  4 .relcgroup/connect4    00000010 0000000000000000
  5 .ebpf_for_windows      00000001 0000000000000000 DATA
  6 .maps                  00000020 0000000000000000 DATA
  7 license                0000000d 0000000000000000 DATA
```

:::

第 5 行那个 `.ebpf_for_windows` 节只在 Windows 版里有，内容是 1 个字节的 `true`。它来自 `bpf_helpers_platform.h` 里的一行：

```c
const bool __ebpf_for_windows_tag __attribute__((section(".ebpf_for_windows"))) = true;
```

只要包含了 Windows 的头文件，这一行就会被编进目标文件，给加载器留一个「这是按 Windows 头文件编的」的标记。ebpf-go 的文档把它当作识别 Windows 目标文件的办法。这个标记不能保证编号正确，它只说明编的时候用的是哪套头文件。

### 5.3 哪些能移植，哪些必须重写

在跨平台实践中，最适宜直接复用的是无状态的判定策略逻辑与通用 map 定义。在上述实例中，`if` 分支的过滤条件、计数用的数组 map、原子的原子操作表达式以及通用的许可证声明，在两个平台上均可保持完全一致。当系统策略抽象为规则集合（例如「目的地址 + 端口 + 过滤处置动作」）时，完全可以将这类规则模型抽取为独立的业务头文件或共享 map，在两个平台间无缝共享相同的业务逻辑。

然而，必须为各个目标环境量身定制适配层的部分同样十分明确，主要集中在三处关键契约：钩子类型的语义映射、上下文结构体的类型定义以及返回值的规约。不同的操作系统在挂接机制上各有其底层原语——Linux 端的 `cgroup/connect4` 依托内核 cgroup 子系统对套接字生命周期进行拦截，而 Windows 端的同名节区在底层则由扩展驱动挂载在 WFP 的 `ALE_AUTH_CONNECT` 过滤层之上；上下文类型决定了内存读取指令的偏移常量；返回值枚举则规定了内核判定放行或阻断的通信协议。这些物理差异无法通过简单的宏完全掩盖，稳健的工程实践是将它们隔离在各平台的轻量适配层中，使主体策略代码得以保持单一真理源。

在真实的工程演进中，跨平台移植的核心工作量主要消耗在适配层，策略业务代码本身的重写成本反而相对可控。评估一个已有 eBPF 工具的移植可行性时，首要动作是梳理其所依赖的辅助函数与探针类型。Windows 的 `bpf_helper_defs.h` 目前仅声明了 35 个基础辅助函数（而 Linux 现代内核已扩充至两百余个），`ebpf_structs.h` 涵盖的程序类型亦仅有十种左右。如果目标程序的逻辑完全收敛在这个共性交集之内，移植工作通常数日即可完成；但一旦业务强依赖 kprobe、fentry 或 raw tracepoint 去深入解构特定内核内部函数栈，便会遭遇物理缺失，只能另辟蹊径重构监测方案。

## 6. 跨平台的前景

### 6.1 指令集的标准化

2024 年 10 月，IETF 正式发布了 RFC 9669《BPF Instruction Set Architecture (ISA)》，该规范已被确立为拟议标准（Proposed Standard），由 D. Thaler 担任主编。该标准从形式上严格规范了 BPF 汇编指令的操作码编码、各类计算与访存指令的语义规范、一致性合规分组（conformance group）以及 IANA 注册表结构。这份文档立足于指令集的通用计算语义，并不介入具体操作系统的钩子点、特定辅助函数体系或上下文结构。它解决的是在不同的执行引擎之间「如何一致地解读字节码指令」这一核心命题；至于具体的程序在异构系统内核中能否成功挂载与运行，则完全取决于特定平台的运行时扩展支持。

IETF BPF 工作组的章程界限十分严明：聚焦于跨平台共性技术标准的沉淀，不介入特定操作系统的专属私有实现；首先对现存的工业实践进行规范化记录并确立通用的扩展演进流程，在此基础上渐进式发展通用的标准扩展。根据工作组的长期路线图规划，除了已经定稿的基础指令集规范，后续还将推进验证器行为预期与安全执行基线（信息性 RFC）、BTF 类型系统规范（标准级 RFC）、跨平台可移植目标文件格式约定、通用跨平台 map 抽象、通用辅助函数集与程序类型集，以及统一的体系架构全景文档。唯有当这一整套标准化协议栈完全成型，「单份目标文件无感漫游加载」才具备格式与协议层面的坚实依托；在此之前，跨平台交互依然依赖各家工具链的适配契约。

### 6.2 基金会与用户态运行时

在开源生态治理层面，eBPF 基金会于 2021 年宣告成立，Meta、Google、Isovalent、Microsoft 与 Netflix 等企业作为创始成员共同参与建设，致力于协调上游社区演进、加速工业界落地并提升整体安全标准。基金会与 IETF 工作组各司其职：基金会统筹开源项目群与生态治理，工作组则专注沉淀严谨的跨平台标准文本。

与内核级实现并行推进的另一大支线，是将 eBPF 执行引擎整体搬迁至用户空间。uBPF 作为轻量级虚拟机，长期充当 Windows 早期 JIT 探索与各类用户态沙箱的基石；而更为新锐的 bpftime 等项目则进一步拓展了边界：借助动态二进制指令改写（Dynamic Binary Instrumentation）与用户态内存拦截技术，它能够将 eBPF 探针直接注入到标准二进制程序的 uprobe、USDT 乃至系统调用拦截点上，全程无需内核特权亦无需 root 权限支持；状态 map 则依托共享内存跨进程安全流转，前端工具链则无缝兼容现有的 clang、libbpf 与 bpftrace，甚至在用户态直接引入 PREVAIL 完成安全性静态证明。

然而用户态沙箱的物理边界同样不可忽视：大部分与硬件及内核子系统强相关的特权辅助函数无法映射，直接解引用内核关键对象更无从谈起。它的核心定位在于承载用户空间进程的高性能自观测与沙箱治理，而无法在操作系统层面完全替代内核原生的 XDP 与 cgroup 治理流水线。

### 6.3 跨平台的发展空间

综合现阶段的技术演进脉络，跨平台 eBPF 真正具备大规模工程通用潜力的领域，高度聚焦于网络安全访问策略与端到端流量控制。这类控制逻辑往往具备良好的正交性：其涉及的拦截动作（网络连接、端口绑定、报文收发）在各操作系统抽象层均有对应模型，事件上下文结构紧凑且由扩展平台自主定义，核心判定逻辑本质上是纯粹的数值比对与状态流转，完全不依赖特定操作系统内部深层的脆弱数据结构。前文演示的 `connect_deny` 防护探针便极具代表性：剥离外层适配外壳后，其核心计算仅包含两次轻量的数值检查与一次计数递增。

相比之下，深度的内核追踪与系统观测工具在跨平台场景下面临着近乎天然的屏障。系统追踪往往需要深度刺探 `task_struct`、`file` 或 `sock` 等内核内部核心数据结构，这些结构属于各个操作系统的核心机密，即便在 Linux 单一平台上不同发行版之间，亦需仰赖 CO-RE 与精准的 BTF 重定位机制方能跨版本维系；在面对 Windows 截然不同的对象管理与微内核拓扑时，更无从建立结构体映射。Windows 上最贴近系统审计的扩展探针，也仅能在 `ntosebpfext` 中暴露出几个由平台抽象出的通用进程创建与终止元数据字段，与 Linux 原生的调度器和虚拟文件系统上下文不存在直接等价关系。

因此，对「跨平台 eBPF」的理性工程预期应当是：核心业务与策略逻辑保持单一源头，平台差异交由两侧专门的适配桩吸收；汇编指令层面有严格标准可循，而特定上下文与探针则始终受制于操作系统的安全边界。指望完全不经适配直接将单份编译出的 `.bpf.o` 投递至异构操作系统或用户态环境中无缝运转，在当前及可预见的未来依然不切实际。

## 7. 习题

### 7.1 巩固

<Exercise id="47601" :d="1" :w="1">

编译第 4 节的 `bpf.c` 和 `bpf2.c`，用 `llvm-objdump --triple=bpf -h` 看节表，回答：

1. `bpf.o` 里有几个节带 `TEXT` 标记？哪个节里有指令？
2. `bpf2.o` 的 `.text` 大小为什么是 0？
3. `bpf2.o` 里 `myprog` 和 `another` 各有多少字节？换成指令条数是几条？

</Exercise>

<Exercise id="47602" :d="2" :w="2">

用 `helper_id.c` 做一组对照实验：

1. 按 Linux 的编号（5 和 14）编译出 `helper_linux.o`，按 Windows 的编号（9 和 19）编译出 `helper_windows.o`，贴出两个反汇编里 `call` 后面的数字。
2. 把 Windows 那次的编号改回 5 和 14，再编一次，反汇编有变化吗？为什么？
3. 假设把 Linux 编出的目标文件交给 Windows 执行，第 0 条指令会调用 Windows 编号表里的哪个辅助函数？查 `include/ebpf_structs.h` 的 `bpf_func_id` 枚举回答。

</Exercise>

<Exercise id="47603" :d="2" :w="2">

`ctx_layout.c` 只打印了三个字段。把它扩展成打印 `family`、`protocol`、`compartment_id` 三个字段在两边结构体里的偏移，然后回答：

1. 哪几个字段在两边恰好同偏移，哪几个不同？
2. 两个结构体的 `sizeof` 差多少？差的这 8 个字节来自哪里？
3. 在 `connect_deny.bpf.c` 里把 `ctx->user_port` 换成 `ctx->protocol`，两边的反汇编偏移各是多少？和第 1 问的答案对得上吗？

</Exercise>

### 7.2 编程

<Exercise id="47604" :d="3" :w="3">

把 `connect_deny.bpf.c` 扩展成 IPv4 和 IPv6 两个版本，同一份源文件，两个平台都能编：

1. 加一个 `SEC("cgroup/connect6")` 的程序，判断 `ctx->user_ip6` 是不是 `::1`（`0x00...01`）且端口是 `DENY_PORT`；
2. 两个程序共用同一个 `blocked` map；
3. 用第 5 节的两条命令分别编译，要求 `-Wall -Wextra -Werror` 下无警告；
4. 在 SignalX 上用 `bpftool gen skeleton` 生成骨架，确认骨架里 `progs` 和 `links` 各有 `connect4`、`connect6` 两个成员。

提示：`user_ip6` 是 4 个 32 位字的数组，第一个字的高位在前。比较 `::1` 只需比较前三个字为 0、最后一个字为 `bpf_htonl(1)`。

</Exercise>

<Exercise id="47605" :d="4" :w="4">

把策略从「写死的常数」改成「从 map 里查」，两个平台共用同一份逻辑：

1. 定义一个哈希 map，键是 `{ __u32 addr; __u16 port; }` 结构体，值是 `__u32`，0 表示拒绝、1 表示放行；
2. 内核态程序先按 `ctx->user_ip4` 和 `ctx->user_port` 查表，查到就按值返回，查不到返回默认动作；
3. 默认动作做成一个 `const volatile __u32` 对象，用户态在 load 之前写；
4. 在 Linux 上写一个用户态程序，用 `bpf_map__update_elem` 往表里塞三条规则再附加到测试 cgroup；在 Windows 上给出对应的 `netsh` 或 `bpf_map_update_elem` 调用序列（只需写出命令或函数名，不必运行）；
5. 分别编译两个目标文件，确认逻辑部分的指令序列一致，只有偏移和返回值常量不同。

</Exercise>

<Exercise id="47606" :d="4" :w="3">

写一份 `Makefile`，一条命令产出两个平台的目标文件：

1. 目标 `linux` 用 `cc`/`clang -target bpf` 加 `vmlinux.h`；目标 `windows` 用 `clang -target bpf -DMDR_PLATFORM_WINDOWS`，头文件路径从 make 的 `EBPFW_INCLUDE` 取，未设置时报错退出；
2. 两个目标编完后各跑一次 `llvm-objdump -h`，把节表打印出来；
3. 加一个 `check` 目标，验证两个目标文件里 `cgroup/connect4` 节都存在，且 Windows 版里多一个 `.ebpf_for_windows` 节；
4. 验收：`make linux`、`make windows`、`make check` 三条命令的退出码都是 0，`make windows` 在未设置 `EBPFW_INCLUDE` 时给出明确的错误信息。

Windows 的头文件可以从 eBPF for Windows 仓库的 `include` 目录取，也可以从 NuGet 包 `eBPF-for-Windows` 解出。

</Exercise>

<Exercise id="47607" :d="4" :w="3">

第 4 节用 `netsh ebpf show sections` 看到了程序节列表。用自己编出的目标文件走一遍同样的检查流程，把能做的部分做掉：

1. 写出三条命令：列举节、验证、反汇编，并用本机的 `llvm-objdump` 完成第三条；
2. 说明 `netsh ebpf show verification` 为什么需要 `type=` 参数，而 `show disassembly` 不需要；
3. 如果程序放在 `cgroup/connect4` 节里而不是 `.text` 里，`type=` 还要不要写？从注册表的 `SectionData` 说明理由。

</Exercise>

<Exercise id="47608" :d="6" :w="5">

给两个平台的 `bpf_sock_addr` 写一个编译期校验头文件 `ctx_check.h`：

1. 用 `_Static_assert` 校验 `user_ip4`、`user_port`、`protocol` 三个字段在各自平台上的偏移，值分别用本页量出来的数字；
2. 头文件要能被 `clang -target bpf` 编译，因此不能包含 `<assert.h>` 或运行时代码，只用 `_Static_assert` 和 `offsetof`；
3. 在 `connect_deny.bpf.c` 里包含它，两个平台各编译一次，要求通过；
4. 故意把 Windows 那组数字改成 Linux 的偏移，确认编译失败，贴出错误信息；
5. 说明这个头文件在什么情况下会失效——提示：它校验的是头文件版本，不是运行时的实际布局。

</Exercise>

### 7.3 综合与挑战

<Exercise id="47609" :d="7" :w="6" scope="*">

设计一个跨平台的连接策略组件，要求同一份策略源码同时服务 Linux 与 Windows，并给出可执行的项目骨架：

1. **目录结构**：分出 `policy/`（纯逻辑）、`adapters/`（平台钩子与 ctx）、`loaders/`（两个平台的加载程序）、`tests/`，说明每个目录的职责与依赖方向；
2. **接口**：给 `policy/` 设计一个只依赖整数和定长数组的接口，例如 `int policy_decide(__u32 addr, __u16 port, __u8 proto)`，说明为什么接口里不能出现指针和平台结构体；
3. **适配层**：写出 Linux 和 Windows 两个适配文件的骨架，说明每个需要填哪几样东西（钩子名、ctx 类型、返回值转换）；
4. **构建**：写出一份能同时产出两个目标文件的构建脚本，并说明如何在不安装 eBPF for Windows 的情况下做编译期检查；
5. **验证**：设计三层验证，分别覆盖纯逻辑（主机上跑单元测试）、目标文件（节表与反汇编检查）、运行行为（在检测机上按什么步骤验证），每层给出可执行的命令或判定标准；
6. **风险评估**：列出三处你认为最容易在移植时出错的地方，并说明各自的发现手段。

</Exercise>

## 8. 参考资料

- microsoft/ebpf-for-windows：[仓库与 README](https://github.com/microsoft/ebpf-for-windows)、[docs/tutorial.md](https://github.com/microsoft/ebpf-for-windows/blob/main/docs/tutorial.md)、[docs/GettingStarted.md](https://github.com/microsoft/ebpf-for-windows/blob/main/docs/GettingStarted.md)、[docs/NativeCodeGeneration.md](https://github.com/microsoft/ebpf-for-windows/blob/main/docs/NativeCodeGeneration.md)、[docs/InstallEbpf.md](https://github.com/microsoft/ebpf-for-windows/blob/main/docs/InstallEbpf.md)、[docs/isa-support.rst](https://github.com/microsoft/ebpf-for-windows/blob/main/docs/isa-support.rst)
- 头文件：[include/ebpf_nethooks.h](https://github.com/microsoft/ebpf-for-windows/blob/main/include/ebpf_nethooks.h)、[include/ebpf_structs.h](https://github.com/microsoft/ebpf-for-windows/blob/main/include/ebpf_structs.h)、[include/bpf_helpers_platform.h](https://github.com/microsoft/ebpf-for-windows/blob/main/include/bpf_helpers_platform.h)、[include/linux/bpf.h](https://github.com/microsoft/ebpf-for-windows/blob/main/include/linux/bpf.h)
- [eBPF for Windows 发布页](https://github.com/microsoft/ebpf-for-windows/releases)；[issue 4997：Deprecate JIT code](https://github.com/microsoft/ebpf-for-windows/issues/4997)
- 微软开源博客：[Making eBPF work on Windows](https://opensource.microsoft.com/blog/2021/05/10/making-ebpf-work-on-windows/)（2021 年 5 月 10 日）
- LWN：[eBPF for Windows](https://lwn.net/Articles/857215/)（Quentin Monnet，2021 年 6 月 10 日）
- ebpf-go：[Windows support](https://ebpf-go.dev/guides/windows-support/)
- PREVAIL 验证器：[vbpf/ebpf-verifier](https://github.com/vbpf/ebpf-verifier)；论文 [Simple and Precise Static Analysis of Untrusted Linux Kernel Extensions](https://dl.acm.org/doi/10.1145/3314221.3314590)（PLDI 2019）
- uBPF：[iovisor/ubpf](https://github.com/iovisor/ubpf)；用户态运行时：[eunomia-bpf/bpftime](https://github.com/eunomia-bpf/bpftime)
- XDP for Windows：[microsoft/xdp-for-windows](https://github.com/microsoft/xdp-for-windows)；进程事件扩展：[microsoft/ntosebpfext](https://github.com/microsoft/ntosebpfext)
- IETF：[RFC 9669 BPF Instruction Set Architecture (ISA)](https://www.rfc-editor.org/rfc/rfc9669)、[BPF 工作组文档](https://datatracker.ietf.org/wg/bpf/documents/)、[工作组章程](https://datatracker.ietf.org/wg/bpf/about/)
- eBPF 基金会：[ebpf.foundation](https://ebpf.foundation/)；成立时间与创始成员见 [Wikipedia: eBPF](https://en.wikipedia.org/wiki/EBPF)
- 本页配套代码：`code/系统编程/拓展B_eBPF/B_16_eBPF_for_Windows与跨平台/`
