# 1.2 MCU、MPU、SoC 与执行环境

将一块小巧的 Blue Pill 核心板插入电脑的 USB 端口，调试探针瞬间识别出芯片，几十 KB 的固件几秒内烧录完毕，板载 LED 便随即按设定的周期欢快闪烁；如果换成一块搭载 DDR 颗粒的高性能工业核心板，同样的 USB 供电线，烧录进去的却是动辄数兆甚至数十兆字节的 Linux 内核镜像与设备树，开机时屏幕上滚动着漫长的自检信息，冷启动通常要耗费数秒之久。这两块板子同样以 Arm 架构内核为运算中枢，同样具备片上外设与物理存储，但软硬件呈现出的工作状态却大相径庭。这种分水岭的背后，根源在于芯片存储系统的组织架构，以及硬件层面是否配备了内存管理单元（MMU）——这两大基石从物理层面划分出了前后台裸机固件与通用嵌入式 Linux 两条截然不同的软件演进路径。

构建环节的体验落差同样直击本质。同一份极简的 `hello.c` 源码，在主机端使用系统的 GCC 编译运行，终端立刻平稳吐出预期字符；然而一旦原封不动地将其交给交叉编译器 `arm-none-eabi-gcc`，构建过程却在链接阶段戛然而止，终端刷出一长串诸如 `_sbrk`、`_write`、`_exit` 等符号未定义的致命错误。这些符号从未在我们的业务代码中出现，翻遍标准库接口也找不到声明。它们并非偶然的遗漏，而是 C 语言规范在底层悄悄划下的运行环境边界：我们在个人电脑上习以为常的控制台输出、动态堆分配与进程退出机制，背后全赖宿主操作系统的默默托底。当脱离操作系统的温床，直面裸露的微控制器时，整个 C 语言运行时的支撑基座都需要由开发者亲手接续。

## 1. 三块芯片

在深入剖析软件运行基座之前，我们需要在物理层面对计算芯片的形态建立直观的坐标系。本教程的硬件实验均以 STM32F103C8T6 最小系统板为基础展开，但要准确理解它的能力边界与设计哲学，不妨将其与另外两颗具有鲜明工程定位的芯片并排对比：

STM32F103C8T6 是一颗经典的微控制器 (Microcontroller Unit, MCU)。在其紧凑的 LQFP48 封装内，集成了 72 MHz 主频的 Cortex-M3 处理器内核、64 KiB 的片内闪存 (Flash)、20 KiB 的静态随机存取存储器 (SRAM)，以及涵盖通用输入输出（GPIO）、串口（USART）、SPI、I²C、模数转换器（ADC）和通用定时器在内的丰富外设阵列。在这颗芯片上，固件指令直接烧录在片内 Flash 中，CPU 取指和读取静态只读常量均直接寻址该闪存，无需在开机时耗时搬运至外部内存；从电路上电复位信号释放到第一条机器指令开始解码，往往仅需几毫秒的时间，整块核心板的硬件物料成本更是控制在十元人民币左右。

STM32MP157 则代表了另一维度的设计思路。它本质上是一颗应用级微处理器 (MPU)：内部集成了双核 Cortex-A7 高性能处理器、一颗用于实时控制的 Cortex-M4 辅助核、专用的 3D 图形加速硬件以及丰富的高速外设控制器。然而，这颗强大的芯片片内仅预留了数百 KiB 的临时 SRAM，根本放不下完整的系统软件体系。程序代码与动态运行数据必须外挂外部高频 DDR3/DDR4 存储颗粒以及 eMMC 大容量闪存。更关键的是，其 Cortex-A7 内核搭载了硬件内存管理单元 (MMU)，为承载功能完备的嵌入式 Linux 操作系统提供了底层支撑。这种级别的芯片在售价上往往是普通单片机的数十倍，且在硬件设计时需要极为严格的高频阻抗走线、复杂的电源管理芯片（PMIC）时序配合与多层高精度印制电路板。

ESP32-C3 则走出了一条极具时代特色的折中路线。它采用开源的 32 位 RISC-V 指令集架构内核，运算主频可达 160 MHz，硅片内集成了 400 KiB 的大容量 SRAM 与 384 KiB 的引导 ROM，并在外部模组金属罩内紧凑合封了一片 SPI 闪存。最为突出的是，它在方寸硅片上原生集成了完整的 Wi-Fi 4 与低功耗蓝牙 (BLE 5.0) 射频电路前端与基带处理器。它虽然没有搭载 MMU，无法运行标准 Linux，但凭借充裕的内存与原厂高度定制的 RTOS 开发框架（ESP-IDF），在低成本物联网感知与联网控制领域占据了一席之地。

| 项目 | STM32F103C8T6 | STM32MP157 | ESP32-C3 |
| --- | --- | --- | --- |
| 内核与主频 | Cortex-M3，72 MHz | 2 × Cortex-A7，最高 800 MHz；1 × Cortex-M4，209 MHz | 32 位 RISC-V 单核，最高 160 MHz |
| 片内存储器 | 64 KiB Flash、20 KiB SRAM | 片内 SRAM 数百 KiB，程序与数据主要放外部 | 400 KiB SRAM、384 KiB ROM，Flash 封装在模组内 |
| 片外存储器 | 无，需要时可外挂 SPI Flash | DDR3/DDR3L、eMMC、SD 卡 | 无，模组内已含 Flash |
| 内存管理单元 | 无 | Cortex-A7 有 MMU | 无 |
| 射频 | 无 | 无 | Wi-Fi 4、蓝牙 5 |
| 典型软件形态 | 裸机、RTOS | 嵌入式 Linux，M4 核跑实时任务 | 裸机、RTOS（ESP-IDF） |
| 本教程中的角色 | 全部实验的参考硬件 | 本节与第 5 章的对照 | 3.8 节的 AT 命令模块 |

从抽象的图灵机视角来看，这三颗芯片都能被称为“一台完整的计算机”，也都能使用标准 C 语言编写业务逻辑。然而在物理现实中，“指令从哪里实时取出执行”以及“内存空间究竟由谁来划分与管理”，决定了它们所能承载的软件形态与开发法则。在后续关于寄存器配置、中断向量表与片上调试的讨论中，若无特殊声明，均以作为裸机微控制器代表的 STM32F103C8T6 为核心基准。

## 2. MCU、MPU 与 SoC

在嵌入式行业的日常交流中，微处理器 (MPU)、片上系统 (SoC) 与微控制器 (MCU) 这几个缩写经常被交替使用，甚至让初学者感到混淆。要建立清晰的技术判断，需要先剥离商业宣传的标签，回到它们的原始定义与工程边界上。

微处理器 (Microprocessor Unit, MPU) 最初强调的是纯粹的“计算与控制核心”。典型的 MPU 芯片内部专注于搭建流水线、运算器与高速缓存，通常不包含存放持久程序的非易失闪存，片上只有极少量的暂存 RAM，也没有直接集成复杂的传感器采集外设。个人电脑中的通用 x86 CPU、早期服务器芯片以及工业嵌入式中的 i.MX6ULL 等应用处理器均属此类。基于 MPU 构建的系统，必须在外围电路上配齐动态随机存储器（DRAM）、存储固件镜像的 NAND Flash/eMMC、复杂的时钟源以及专用的多路电源转换芯片，整个硬件系统对外部电路的依赖度极高。

片上系统 (System on Chip, SoC) 则是一个侧重于“高集成度”的工程概念。它的核心哲学是将传统电路板上散落的处理器核心、总线矩阵、内存控制器、图形渲染单元（GPU）、射频基带、乃至各类音视频硬件编解码器全数浓缩进单一硅片之中。如果严格按照字面含义，无论是内部集成了丰富外设与 Flash 的 STM32F103，还是集成了 Wi-Fi/蓝牙射频的 ESP32-C3，亦或是多核应用级芯片 STM32MP157，都已经在片上集成了完整的系统要素，因而都具备 SoC 的特征。在现代工业语境下，“SoC”往往被作为一个包容度很高的商业统称，仅凭这个词很难断定一颗芯片的开发范式。

因此，在技术沟通与项目选型时，真正行之有效的判定标准是抓住以下两个关键的硬件分水岭：

1. **存储系统的执行路径**：芯片上电后，处理器是能够直接在片内集成的闪存（NOR Flash）上原位取指并开始执行（即就地执行 XIP），还是必须借助辅助引导程序，将外部大容量存储器里的代码全盘复制到数十乃至数百兆字节的外部 DRAM 中才能启动？运行时的动态全局数据，是直接受限于片内几十 KB 的微型 SRAM，还是拥有充裕的外置动态内存空间？
2. **是否存在硬件内存管理单元 (MMU)**：内核是否集成了 MMU 硬件电路，能否支持多级页表、虚拟地址转换与访问权限保护？这是支持进程地址空间隔离、安全运行通用分时操作系统（如 Linux）的必要硬件前提。

只要两个判据的落点都在“片内自给自足”且“无 MMU”，那么无论主频多高、外设多么繁杂，这颗芯片在开发法则上就属于微控制器 (MCU)；而如果系统必须外接高频 DRAM、依赖 MMU 构建虚拟地址空间，它就属于典型的微处理器 (MPU) 或应用级 SoC。至于片上是否集成了射频蓝牙、音频编解码器或神经计算加速器，仅决定了其适合拓展的特定垂直场景，并不撼动其底层的系统架构分类。

::: concept emb-exec-classify-by-memory title="按存储系统判断芯片类别"

判断一颗芯片属于 MCU 还是 MPU，看程序和数据放在哪里，以及有没有 MMU，而不是看主频或封装。片内 Flash 就地执行、片内 SRAM 存数据的，是微控制器；必须外接 DRAM、由 MMU 管理地址空间的，是微处理器或应用处理器 SoC。

:::

<Quiz
  type="single"
  :options="[
    { label: '微控制器（MCU）', value: 'A' },
    { label: '微处理器（MPU）', value: 'B' },
    { label: '不能归入 MCU，因为它带射频', value: 'C' },
    { label: '必须运行 Linux 才能使用', value: 'D' }
  ]"
  answer="A"
>

<template #question>

某芯片参数为：Cortex-M33 内核，主频 200 MHz，片内 1 MiB Flash、256 KiB SRAM，带 Wi-Fi 射频，无 MMU。它应当归入哪一类？

</template>

正确答案是 **A**。程序在片内 Flash 上就地执行，数据放在片内 SRAM，没有 MMU，这是微控制器的存储系统；Wi-Fi 射频只是集成度更高，不改变分类。它跑不了 Linux，也不需要。

</Quiz>

## 3. 处理器架构与内核系列

商业模式决定了芯片的技术演化。Arm 公司本身并不直接加工销售硅片实体，而是专注于处理器的微架构设计，并将知识产权（IP）授权给各大半导体芯片厂商。在 Arm 的技术版图里，处理器内核根据应用场景的侧重清晰地划分为三个系列：

- **Cortex-A (Application)**：面向高吞吐量与复杂应用处理。内核标配硬件 MMU，时钟频率通常跨越数百 MHz 直至数 GHz，拥有复杂的分支预测流水线与多级高速缓存，旨在顺畅运行 Linux、Android 等现代多任务操作系统。
- **Cortex-R (Real-time)**：面向严苛的硬实时控制领域。它不以纯粹的峰值吞吐量为主要诉求，而是强调纳秒级确定性的中断响应、紧耦合内存（TCM）以及双核锁步容错，常见于硬盘驱动器读写头控制、汽车电子制动防抱死（ABS）以及移动通信基带处理器中。
- **Cortex-M (Microcontroller)**：专门针对微控制器优化。它坚决砍掉了复杂的动态分支预测与多级通用 MMU，力求做到指令集极致精简、中断进入延迟极低且高度确定（数十个时钟周期内即可完成硬件压栈）、功耗极其优异，是微控制器工程实践中绝对的中流砥柱。

深入到 Cortex-M 系列内部，各型号之间的代际与定位差异主要集中在指令集扩展、硬件算力单元以及安全性配置上：

| 内核 | 架构版本 | 主要特点 |
| --- | --- | --- |
| Cortex-M0、M0+ | ARMv6-M | 硅片面积极小，功耗与成本极低，无硬件除法指令，采用两级流水线 |
| Cortex-M3 | ARMv7-M | 引入 Thumb-2 混合指令集，支持硬件整数除法，内置 NVIC 中断控制器与位带特性 |
| Cortex-M4 | ARMv7E-M | 在 M3 基础上扩充 DSP 单指令多数据（SIMD）指令集，并可选配单精度硬件浮点单元（FPU） |
| Cortex-M7 | ARMv7E-M | 采用双发射超标量流水线，引入片内指令与数据 Cache，运行主频迈向 400 MHz 以上 |
| Cortex-M23 | ARMv8-M Baseline | 针对物联网超低功耗场景，支持 TrustZone 硬件安全隔离技术 |
| Cortex-M33 | ARMv8-M Mainline | 综合 DSP 指令、浮点单元与 TrustZone 安全隔离，作为 M4 的现代演进替代者 |

对比之下，ESP32-C3 采用的是全球开源的 RISC-V 指令集架构，与 Arm 的授权生态毫无瓜葛。RISC-V 规范凭借免授权费、模块化扩展的特点，近年来在以联网通信为主的低成本芯片中增长迅猛；本教程只在第 3 章 3.8 节中将其作为接收 AT 指令的辅助通信模块使用，技术重心始终聚焦在工业普及度极高的 Cortex-M 系列上。

本教程选用的 STM32F103C8T6 搭载的是经典的 Cortex-M3 内核。它完整支持 16 位 Thumb 与 32 位 Thumb-2 混合编码指令集，但没有配备硬件浮点单元（FPU），代码中的所有 `float` 和 `double` 算术运算均由编译器插入的软浮点软件库逐位模拟；片上集成了强大的嵌套向量中断控制器 (NVIC) 和系统滴答定时器 (SysTick)，具备 32 位硬件整数除法指令，但并未集成存储器保护单元 (MPU)。这一硬件裁切可以在 ST 官方提供的芯片级设备头文件里得到确证：翻开 `stm32f103xb.h`，宏定义 `__MPU_PRESENT` 被明确写死为 0，而中断优先级分组位宽 `__NVIC_PRIO_BITS` 为 4，内核版本 `__CM3_REV` 则为 `0x0200`（即 r2p0 版本内核）。如何借助设备头文件直观操作芯片底层，将在 [3.1 驱动接口、CMSIS 与设备头文件](/教程/正文/嵌入式/3_驱动_通信与中间件/3_1_驱动接口_CMSIS与设备头文件) 中展开详述。

::: generate emb-exec-cortex-m-series kind=explanation concepts=emb-exec-classify-by-memory

面向只学过 C 语言、第一次接触 Arm 内核的读者，把 Cortex-M0、M3、M4、M7 放在一起对照：用「一条 32 位整数乘法要几个周期」「有没有浮点单元」「中断最多嵌套几层」这三个可感知的指标，说明选型时为什么电机控制常用 M4、低成本家电常用 M0、而本教程选 M3。不要复述表格。

:::

## 4. 执行环境：独立环境与宿主环境

在初学 C 语言时，大多数开发者默认所有的 C 程序都遵循相同的运行范式：写下一个名为 `main` 的主入口，顺理成章地调用 `printf` 输出结果，需要存储空间时随时通过 `malloc` 向系统索取，最后在 `main` 的末尾写下 `return 0;`。但在 ISO C 标准体系中，这套习以为常的模型其实仅仅属于运行环境的其中一种分类。

标准严谨地将 C 语言的执行环境划分为两类：**宿主环境 (hosted environment)** 与 **独立环境 (freestanding environment)**。

所谓宿主环境，指的是程序运行在一套成熟的操作系统之上。程序启动时由操作系统的加载器为其分配平坦的虚拟地址空间并初始化运行时栈帧，支持完整的 `int main(int argc, char *argv[])` 命令行参数传入；标准输入输出流（`stdin`、`stdout`、`stderr`）被透明地重定向至控制台窗口或管道文件；调用 `malloc` 时由操作系统的内核机制扩展虚拟内存页；当程序执行完毕或发生致命错误时，操作系统能够完整回收全部内存与句柄资源。个人电脑、服务器以及智能手机上的通用 C 程序，无一例外都属于这种范畴。

与此相对，独立环境则面向直接运行在物理裸金属之上的系统。在这里，没有操作系统为代码保驾护航，C 标准对独立环境的运行保证极度克制：代码究竟如何从硬件上电中被唤醒、入口函数是否必须命名为 `main`、主函数允许接受怎样的参数签名，全数交由具体平台的工具链实现自行裁量。更为关键的是，在标准严谨的规范里，独立环境**仅保证**以下 9 个基础头文件的可用性（其中 `<stdalign.h>` 与 `<stdnoreturn.h>` 为 C11 规范引入）：

| 头文件 | 内容 |
| --- | --- |
| `<float.h>` | 浮点类型的范围与精度 |
| `<iso646.h>` | `and`、`or`、`not` 等替代记号 |
| `<limits.h>` | 整数类型的范围 |
| `<stdalign.h>` | `alignas`、`alignof` 宏（C11） |
| `<stdarg.h>` | 可变参数列表 |
| `<stdbool.h>` | `bool`、`true`、`false` |
| `<stddef.h>` | `size_t`、`ptrdiff_t`、`offsetof`、`NULL` |
| `<stdint.h>` | `uint32_t` 等固定宽度整数类型 |
| `<stdnoreturn.h>` | `noreturn` 宏（C11） |

这意味着，我们在桌面开发中天天接触的 `<stdio.h>`、`<stdlib.h>` 与 `<string.h>`，根本不在独立环境的标准保证清单中。

然而在实际工程开发中，我们在写微控制器固件时往往依然能够无误地 `#include <stdio.h>` 并在代码中写下 `printf` 或 `memcpy`。这是因为嵌入式交叉工具链在发布时，通常额外打包了一份针对裸机环境高度精简裁剪的 C 标准库（如 newlib 或 newlib-nano）。工具链虽然允许我们调用这些接口，但函数背后原本由操作系统提供的底层服务却是一片空白：`printf` 需要把排版好的字符送到具体的字符输出端，`malloc` 需要知道动态堆区的物理边界，而 `fopen` 则要求底层具备物理文件系统。当这些操作系统服务在芯片上不复存在时，若开发者不加干预，构建工具链就无法达成符号闭环。

::: concept emb-exec-freestanding-headers title="独立环境的保证范围"

在独立环境中，C 标准只保证 `<float.h>`、`<iso646.h>`、`<limits.h>`、`<stdalign.h>`、`<stdarg.h>`、`<stdbool.h>`、`<stddef.h>`、`<stdint.h>`、`<stdnoreturn.h>` 这 9 个头文件可用，不保证 `<stdio.h>`、`<stdlib.h>`、`<string.h>` 中的函数。工具链通常仍提供这些函数，但它们依赖的底层服务要由程序自己实现。

:::

<Quiz
  type="multi"
  :options="[
    { label: '<stdint.h>', value: 'A' },
    { label: '<stdio.h>', value: 'B' },
    { label: '<stddef.h>', value: 'C' },
    { label: '<string.h>', value: 'D' }
  ]"
  :answer="['A', 'C']"
>

<template #question>

按 C 标准，在独立环境（freestanding）中保证可用的头文件是哪些？

</template>

正确答案是 **A、C**。`<stdint.h>` 与 `<stddef.h>` 在独立环境的保证名单里，前者提供 `uint32_t` 这类固定宽度类型，后者提供 `size_t`、`offsetof`。`<stdio.h>` 与 `<string.h>` 不在名单中——它们能包含、能链接，是因为工具链额外提供了实现，标准不保证。

</Quiz>

交叉编译器在默认状态下仍会按宿主环境的假设展开预处理，只有明确传入 `-ffreestanding` 编译开关时，编译器才会严格收拢至独立环境模式。我们可以通过编译器预定义的 `__STDC_HOSTED__` 宏清晰地观察到这一行为差异：

::: terminal title="bash - 查看 __STDC_HOSTED__"

```text
$ arm-none-eabi-gcc -dM -E - < /dev/null | grep STDC_HOSTED
#define __STDC_HOSTED__ 1
$ arm-none-eabi-gcc -ffreestanding -dM -E - < /dev/null | grep STDC_HOSTED
#define __STDC_HOSTED__ 0
```

:::

值得注意的是，即便在 `-ffreestanding` 的纯粹独立环境下，现代 GCC 编译器依然强制要求运行环境提供 `memcpy`、`memmove`、`memset` 以及 `memcmp` 这四个内存操作函数。这是因为编译器的激进优化器在识别出连续内存块的按字节搬运、大体积结构体之间的直接赋值或整块内存清零循环时，会自动在生成的汇编机器码中直接发射出对这些函数的底层调用——哪怕你的源码里根本没有包含 `<string.h>`。

例如，在后文 [1.8 复位、启动与链接](/教程/正文/嵌入式/1_平台与工具链/1_8_复位_启动与链接) 中实现的复位启动代码中，有一段负责将 Flash 中初始数据复制到 SRAM 的经典循环，当我们以 `-O2` 优化级别编译该文件后，查看其未解析符号列表：

::: terminal title="bash - 启动文件的目标文件依赖什么"

```text
$ arm-none-eabi-gcc -std=c11 -mcpu=cortex-m3 -mthumb -O2 -Wall -Wextra \
    -c startup_stm32f103.c -o startup_stm32f103.o
$ arm-none-eabi-nm -u startup_stm32f103.o
         U _ebss
         U _edata
         U _estack
         U _sbss
         U _sdata
         U _sidata
         U main
         U memcpy
         U memset
```

:::

目标文件输出中标记为 `U` (Undefined) 的符号，表示当前文件仅发出调用，具体的实现依赖后续链接阶段从其他模块中拼合。在这张依赖表中，`_estack`、`_sdata` 等符号由链接脚本注入的物理地址定位，`main` 来自开发者的顶层业务，而原本看似纯手写的搬运循环，已经被编译器悄然转换成了对 C 标准库 `memcpy` 与 `memset` 的外部调用。这组看似细微的链接符号契约，正是整个裸机固件运作的最底层骨架。

不仅如此，在独立环境的裸机系统中，`main` 函数的执行生命周期也有着根本性的重构。这里没有任何操作系统调度器拉起进程，当芯片复位引脚释放后，处理器硬件自动读取向量表中的复位向量，跳入复位处理函数（`Reset_Handler`）。在该底层汇编/C 函数亲手设置好栈顶指针、搬运完数据段并清零未初始化内存后，才会通过一条普通的跳转指令跃迁进 `main` 函数。当 `main` 内部的逻辑走完，在没有父进程可以返回的世界里，处理器若执行一条 `bx lr` 试图返回，程序计数器将直接滑向充满不确定性的非法地址，诱发硬故障（HardFault）死锁。因此，嵌入式微控制器固件中的 `main` 函数在设计上绝对严禁退出，必须以死循环 `for (;;)` 牢牢锁住控制流。

::: concept emb-exec-main-no-return title="裸机程序的 main 由启动代码调用"

在裸机上，`main` 是一个普通函数，由复位处理函数在完成存储器和栈的初始化之后调用。调用者不是操作系统，`main` 返回后没有可交还的对象，因此固件的 `main` 必须以 `for (;;)` 之类的无限循环结束。C 标准中「从 `main` 返回等价于调用 `exit`」的规定在这里没有实际意义。

:::

## 5. 实验：同一个 printf 程序

抽象地讨论「标准库依赖操作系统服务」往往令人隔靴搔痒。只有亲手将同一份看似平淡无奇的 C 源码分别投喂给本地宿主编译器与嵌入式交叉编译器，并在终端观察链接器如何严厉地报错拒绝，才能切身体会到运行环境对代码的深层塑造。

实验代码位于配套仓库的 `code/嵌入式/1_平台与工具链/1_2_MCU_MPU_SoC与执行环境/` 目录。源文件 `hello.c` 极其规整，定义了一个具备非零初值的全局静态对象 `g_boot_magic`、一个用于存储采样数值的全局数组 `g_sample_buf`，并在主函数 `main` 中填充数组、执行累加，最后调用 `printf` 格式化打印计算结果：

```c
#include <stdint.h>
#include <stdio.h>

uint32_t g_boot_magic = 0x103C8000u;
uint32_t g_sample_buf[16];

static uint32_t fill_and_sum(uint32_t seed) {
    uint32_t sum = 0;
    for (size_t i = 0; i < 16; ++i) {
        g_sample_buf[i] = seed + (uint32_t)i;
        sum += g_sample_buf[i];
    }
    return sum;
}

int main(void) {
    uint32_t total = fill_and_sum(g_boot_magic);
    printf("sample[0] = 0x%08lX, sum = 0x%08lX\n",
           (unsigned long)g_sample_buf[0],
           (unsigned long)total);
    return 0;
}
```

### 5.1 在主机上编译运行

在宿主电脑的命令行中，调用本地工具链对该文件直接进行编译构建并运行：

::: terminal title="PowerShell - 主机上的 hello"

```text
$ gcc -std=c17 -Wall -Wextra hello.c -o hello
$ ./hello
sample[0] = 0x103C8000, sum = 0x03C80078
```

:::

这个过程顺理成章：`printf` 将格式化好的 ASCII 字符串写入标准 I/O 位于内存中的行缓冲区，当换行符 `\n` 到达或缓冲区填满时，C 运行时库自动发起底层的 `write` 系统调用，将缓冲区指针与字节长度移交给宿主操作系统；操作系统随后调度显卡驱动或终端模拟器，在屏幕窗口上绘制出清晰的点阵文字。在整个链条中，开发者完全无需关心输出字符究竟是如何穿越物理总线送达显示硬件的。

### 5.2 直接交给交叉编译器

现在，我们改用嵌入式交叉编译器 `arm-none-eabi-gcc` 重新编译这份完全相同的源文件。为了让编译器生成针对 Cortex-M3 内核的 Thumb-2 指令集，我们传入 `-mcpu=cortex-m3 -mthumb` 参数，其余编译选项保持一致：

::: terminal title="bash - 直接链接 hello.c"

```text
$ arm-none-eabi-gcc -mcpu=cortex-m3 -mthumb -O2 -Wall -Wextra hello.c -o hello.elf
…/ld.exe: …/libc.a(libc_a-exit.o): in function `exit':
exit.c:(.text.exit+0x14): undefined reference to `_exit'
…/ld.exe: …/libc.a(libc_a-closer.o): in function `_close_r':
closer.c:(.text._close_r+0xc): undefined reference to `_close'
…/ld.exe: …/libc.a(libc_a-lseekr.o): in function `_lseek_r':
lseekr.c:(.text._lseek_r+0x14): undefined reference to `_lseek'
…/ld.exe: …/libc.a(libc_a-readr.o): in function `_read_r':
readr.c:(.text._read_r+0x14): undefined reference to `_read'
…/ld.exe: …/libc.a(libc_a-writer.o): in function `_write_r':
writer.c:(.text._write_r+0x14): undefined reference to `_write'
…/ld.exe: …/libc.a(libc_a-sbrkr.o): in function `_sbrk_r':
sbrkr.c:(.text._sbrk_r+0xc): undefined reference to `_sbrk'
…/ld.exe: …/libc.a(libc_a-fstatr.o): in function `_fstat_r':
fstatr.c:(.text._fstatr+0xe): undefined reference to `_fstat'
…/ld.exe: …/libc.a(libc_a-isattyr.o): in function `_isatty_r':
isattyr.c:(.text._isatty_r+0xc): undefined reference to `_isatty'
…/ld.exe: …/libc.a(libc_a-abort.o): in function `abort':
abort.c:(.text.abort+0xa): undefined reference to `_exit'
…/ld.exe: …/libc.a(libc_a-signalr.o): in function `_kill_r':
signalr.c:(.text._kill_r+0xe): undefined reference to `_kill'
…/ld.exe: …/libc.a(libc_a-signalr.o): in function `_getpid_r':
signalr.c:(.text._getpid_r+0x0): undefined reference to `_getpid'
collect2.exe: error: ld returned 1 exit status
```

:::

仔细阅读输出信息：编译前端没有吐出任何语法警告或错误，这说明 `hello.c` 的每一行都百分之百符合 ISO C 语法规则；卡住流程的是链接器 `ld`。

交叉工具链附带的 C 标准库是嵌入式专用的 newlib。库内部在实现 `printf`、`exit`、`abort` 等功能时，为了实现架构跨平台，将所有需要与硬件或宿主 OS 打交道的动作统统抽象成了一批以前导下划线开头的底层接口，这批函数在工程规范中被称为**系统调用桩 (system call stubs)**。链接器报错的本质，是因为标准库本身只提供了高层算法，而假定具体目标平台的使用者必须补齐这批连接硬件的桩函数。去重后正好是 10 个未定义符号，每一个符号都精准对应着一件裸机硬件上先天缺失的系统能力：

| 符号 | 谁调用它 | 原本要做什么 |
| --- | --- | --- |
| `_write` | `printf` 输出底层依赖的 `_write_r` | 把指定缓冲区内的字符输出到文件描述符 1 (stdout) |
| `_read` | `scanf`、`getchar` 等输入函数 | 从指定文件描述符读取输入字符 |
| `_close` | `fclose` 以及程序终止析构流程 | 关闭不再使用的文件描述符 |
| `_lseek` | `fseek`、`rewind` | 重新定位文件内部的读写偏移游标 |
| `_fstat` | C 库在判断描述符属性时调用 | 查询文件元数据状态（如文件大小、设备类型） |
| `_isatty` | C 库用来决定是否启用行缓冲 | 判定指定的文件描述符是否为交互式终端字符设备 |
| `_sbrk` | `malloc` 与 `free` 背后的核心分配原语 `_sbrk_r` | 动态向高地址延伸堆区（Heap）的物理边界 |
| `_exit` | `exit()`、`abort()` 等终止逻辑 | 彻底终结当前运行进程，返回退出码 |
| `_kill`、`_getpid` | 信号处理函数 `signal` 以及 `abort()` | 查询当前进程 ID 与向进程投递异步异常信号 |

::: details 完整的链接错误

上面的输出保留了每一条引用，其中 `_exit` 被 `exit` 与 `abort` 各引用一次，按符号名去重后是 10 个。用 `arm-none-eabi-gcc ... hello.c 2>&1 | grep -o "undefined reference to .*" | sort -u` 可以直接得到这份名单。

:::

### 5.3 补上系统调用桩

面对这 10 个悬空的符号，GCC 工具链为开发者提供了快捷的桩规范文件（specs）。其中 `--specs=nano.specs` 会指示链接器选用专为小内存微控制器深度裁剪的 `newlib-nano` 库实现（移除了浮点格式化解析的大量开销，显著减小体积）；而 `--specs=nosys.specs` 则会自动链接进一份由工具链官方提供的“空操作（dummy）系统调用桩集合”，强行把这 10 个未解析符号填平：

::: terminal title="bash - 加上 specs 之后"

```text
$ arm-none-eabi-gcc -mcpu=cortex-m3 -mthumb -O2 -Wall -Wextra hello.c \
    --specs=nano.specs --specs=nosys.specs -o hello.elf
$ arm-none-eabi-size hello.elf
   text	   data	    bss	    dec	    hex	filename
   5824	    108	    432	   6364	   18dc	hello.elf
```

:::

在构建过程中，终端虽然没有报错中断，但滚出了多行类似 `_close is not implemented and will always fail` 的编译警告。这是 `nosys.specs` 在发出严肃声明：它所提供的不过是一堆一经调用便直接返回 `-1` 错误码的“纸糊桩”，目的仅仅是让链接阶段勉强通过。

更为关键的是，即便链接生成了 `hello.elf` 文件，这份产物也绝对不能直接烧写进 STM32 芯片中运行。我们可以通过只读 ELF 查看工具 `readelf` 审视其头部元信息：

::: terminal title="bash - 看产物是什么架构（节选）"

```text
$ arm-none-eabi-readelf -h hello.elf
ELF Header:
  Magic:   7f 45 4c 46 01 01 01 00 00 00 00 00 00 00 00 00
  Class:                             ELF32
  Data:                              2's complement, little endian
  …
  Type:                              EXEC (Executable file)
  Machine:                           ARM
  …
  Entry point address:               0x8101
  …
  Flags:                             0x5000200, Version5 EABI, soft-float ABI
  …

$ arm-none-eabi-readelf -A hello.elf
Attribute Section: aeabi
File Attributes
  Tag_CPU_name: "7-M"
  Tag_CPU_arch: v7
  Tag_CPU_arch_profile: Microcontroller
  Tag_THUMB_ISA_use: Thumb-2
  …
```

:::

从属性来看，`Machine: ARM` 与 `Tag_CPU_arch_profile: Microcontroller` 确认了指令集确实已正确编译为针对 Cortex-M3 的 Thumb-2 编码；`soft-float ABI` 证实了浮点由软件模拟。

然而，致命的问题隐藏在入口地址 `Entry point address: 0x8101` 上。我们通过符号表查看该地址绑定了哪个函数：

::: terminal title="bash - 入口是 C 运行时的启动代码"

```text
$ arm-none-eabi-nm hello.elf | grep -E "_mainCRTStartup| main$"
00008100 T _mainCRTStartup
00008030 T main
```

:::

0x8100 加上指示 Thumb 指令状态的最低位 1，构成了入口地址 0x8101。链接器默认绑定的程序入口是标准 C 运行时的引导函数 `_mainCRTStartup`。它原本期望在由操作系统构建的环境中接管流程、准备堆区、初始化 C 库并最终跳向 `main`。更糟糕的是，由于我们在命令行中没有指定链接脚本，链接器采用了通用模拟器的默认布局，将整个代码段直接安排在从 `0x00008000` 开始的虚拟地址上。这与 STM32F103 的实际物理内存映射（片内 Flash 起始于 `0x08000000`，片内 SRAM 起始于 `0x20000000`）完全相悖，且芯片复位后硬件必须第一时间读取的中断向量表根本无迹可寻。

::: details 程序真烧进去会怎样

`hello.elf` 里没有合法的向量表，复位后处理器硬件从物理地址 0 取出的第一个 32 位字不会是合法的初始栈顶指针，取出的第二个字也不会是指向代码段的有效复位地址，芯片会立即触发未定义指令或总线故障而锁死。这份文件仅用于静态剖析链接机制，真正的固件镜像必须由专用的启动汇编、硬件链接脚本与 `-nostartfiles` 共同构建，我们将在第 2 章 2.4 节点亮第一颗 LED 时完整呈现这套体系。

:::

### 5.4 自己写系统调用桩

依靠 `--specs=nosys.specs` 虽然可以迅速消除链接错误，但代价是将所有的 I/O 字符直接弃置于虚无。为了搞清 `printf` 究竟是如何将字符传递到底层硬件的，最彻底的办法是由我们亲手实现一份极简的系统调用桩。

在配套源码的 `syscalls.c` 中，我们为 newlib 提供了最小可行实现，其中最核心的两个函数如下：

```c
extern char end[];

void *_sbrk(ptrdiff_t incr) {
    static char *heap_end;
    if (heap_end == NULL) {
        heap_end = end;
    }
    char *prev_heap_end = heap_end;
    heap_end += incr;
    return prev_heap_end;
}

int _write(int fd, const void *buf, size_t count) {
    (void)fd;
    (void)buf;
    return (int)count;
}
```

其机理一目了然：
- `_sbrk` 充当动态堆内存（Heap）的分配推进器。`end` 是链接脚本在未初始化数据段（`.bss`）之后自动安插的一个定位符号，堆内存从这个物理边界开始向高地址空间延伸。每当 `malloc` 需要开辟新的连续空间时，便调用 `_sbrk` 传入增量 `incr`，该函数便在物理 SRAM 中将堆顶游标向前推移，并返回之前的旧边界指针。
- `_write` 则是所有标准字符流输出的物理终点站。在上面的极简版本中，我们为了避免阻塞硬件仅做示意，把请求写入的字节数原样返回，字符本身并未输出到任何物理器件。然而，一旦我们将这里的空语句替换为向微控制器的串口发送数据寄存器（USART_DR）逐字节推入数据，就演变成了 [2.7 UART](/教程/正文/嵌入式/2_MCU裸机开发/2_7_UART) 篇章中经典的串口 `printf` 重定向；如果将其替换为 Arm 调试模块的仪器化跟踪宏单元（ITM），就构成了 [1.7 SWD、JTAG 与片上调试](/教程/正文/嵌入式/1_平台与工具链/1_7_SWD_JTAG与片上调试) 中的硬件高速追踪输出。

将我们亲手编写的 `syscalls.c` 与 `hello.c` 一并交给交叉编译器，此时仅需声明 `--specs=nano.specs`，便可彻底抛弃 `nosys.specs`：

::: terminal title="bash - 用自己的系统调用桩"

```text
$ arm-none-eabi-gcc -mcpu=cortex-m3 -mthumb -O2 -Wall -Wextra hello.c syscalls.c \
    --specs=nano.specs -o hello.elf
$ arm-none-eabi-size hello.elf
   text	   data	    bss	    dec	    hex	filename
   5800	    108	    432	   6340	   18c4	hello.elf
$ arm-none-eabi-nm hello.elf | grep -E "_write$|_sbrk$| end$"
00008174 T _sbrk
00008190 T _write
0000a8c4 B end
```

:::

此时查看符号表，`_sbrk` 与 `_write` 已经名正言顺地归入我们自己提供的代码段中（类型为 `T`），而用于定位堆起点的符号 `end` 也由链接器在 `.bss` 之后妥善生成（类型为 `B`）。当后续我们在 [1.8 复位、启动与链接](/教程/正文/嵌入式/1_平台与工具链/1_8_复位_启动与链接) 中编写自定义链接脚本 `stm32f103c8.ld` 时，正是通过在该脚本末尾书写 `PROVIDE(end = .);`，精确把控动态堆内存的物理起始地址。

这也为我们揭示了一个深刻的本质：**C 语言源码本身从来不主动区分自身究竟身处宿主环境还是独立环境**。决定一套代码能否在特定物理硬件上从容立足的，是构建阶段被挑选进来的 C 运行时库、链接脚本规划的内存蓝图，以及开发者为沟通物理世界而亲手打下的系统调用桩。

::: compare left-title="依靠 dummy 桩勉强通过链接" right-title="自主实现 _write 将数据定向至物理硬件"

```text
$ arm-none-eabi-gcc ... --specs=nano.specs --specs=nosys.specs hello.c \
    -o hello.elf
$ arm-none-eabi-size hello.elf
   text	   data	    bss	    dec	    hex	filename
   5824	    108	    432	   6364	   18dc	hello.elf
```

链接虽然顺利通过，但所有由 `printf` 格式化生成的字符流都被哑桩悄无声息地丢弃，在逻辑分析仪、串口助手与调试终端中皆无法捕捉到任何有效信号。

---

```c
/* 重定向至 SWO 单线输出（见 1.7 节）；
   若将循环体替换为向串口 USART1->DR 写入，即为 2.7 节串口打印 */
int _write(int fd, const void *buf, size_t count) {
    (void)fd;
    for (size_t i = 0; i < count; ++i) {
        ITM_SendChar(((const unsigned char *)buf)[i]);
    }
    return (int)count;
}
```

:::

## 6. 软件形态对硬件的要求

前文在 [1.1 嵌入式系统与软件路线](/教程/正文/嵌入式/1_平台与工具链/1_1_嵌入式系统与软件路线) 中，我们从业务复杂度和响应精度的角度对比了裸机、RTOS 与 Linux 的架构选型。而在工程实践的硬币另一面，无论软件构想多么宏伟，最终都必须由具体的芯片物理资源来承载：

| 软件形态 | 需要的硬件条件 | STM32F103C8T6 是否满足 |
| --- | --- | --- |
| 裸机（超级循环 + 中断） | 片内 Flash 与 SRAM，中断控制器，无需 MMU | 完全满足，20 KiB SRAM 足以从容支撑多外设协同 |
| RTOS | 在裸机基础上，每个任务需开辟专属私有栈，RAM 预算显著拉升 | 满足基础运行，但需严谨核算多任务栈开销 |
| 带任务隔离的 RTOS | 需集成存储器保护单元 (MPU)，任务发生地址越界时能触发硬件异常 | 不满足，芯片设备头文件中 `__MPU_PRESENT` 为 0 |
| 嵌入式 Linux | 硬件 MMU、数十兆字节以上的高速 DRAM、大容量块存储介质与复杂时钟电源管理 | 完全不满足，既无 MMU 支持，SRAM 亦仅有 20 KiB |

值得特别强调的是，**存储器保护单元与微处理器在英文缩写中恰好都是 MPU**，但两者的技术含义有着天壤之别。微处理器 (Microprocessor Unit) 是一整颗计算处理芯片的形态定位；而存储器保护单元 (Memory Protection Unit) 则仅仅是微控制器内核内部可选配的一组硬件监控寄存器电路。存储器保护单元能够将物理内存划分为数个受保护的区域，并针对不同模式赋予只读、读写或禁止执行权限。由于 STM32F103 硬件上未集成此单元，其上运行的 RTOS 多任务之间无法构筑物理隔离墙：任何一个子任务中不慎写出的野指针，都可能静默篡改另一个任务的核心控制对象，导致隐蔽难排查的逻辑偶发失效（我们在 [4.7 FreeRTOS](/教程/正文/嵌入式/4_RTOS与固件架构/4_7_FreeRTOS) 中会进一步审视这一特性的影响）。

此外，微控制器内核的执行模式也与程序的健壮性息息相关。Cortex-M3 在硬件层面上原生支持两种执行模式：
- **线程模式 (Thread mode)**：芯片复位后的默认模式，日常的 `main` 函数以及由其调用的常规子函数均在该模式下执行。
- **处理模式 (Handler mode)**：一旦发生硬件中断或系统异常，CPU 硬件流水线自动挂起当前任务并压栈，强制跃迁进入处理模式执行中断服务程序。中断返回后自动退回线程模式。

在这两种模式之上，还叠加了**特权级 (Privileged)** 与 **非特权级 (Unprivileged)** 的权限约束。在裸机前后台开发中，固件通常始终运行在线程模式的特权级下，中断服务则运行在处理模式的特权级下，拥有不受限制地修改任意寄存器与内存区域的绝对特权。

最后需要审视的是代码在存储介质中的驻留形式。在 STM32F103 这类标准微控制器上，片内只读闪存（Flash）与运行内存（SRAM）被统一编排在同一个连续的 32 位平坦物理地址空间内。Cortex-M3 的总线矩阵允许 CPU 的指令接口直接从 Flash 物理单元中读取机器指令并译码执行，这种机制在计算机体系结构中被称为**就地执行 (execute in place, XIP)**。这使得芯片无需像通用 PC 那样在上电时将全部庞大的二进制可执行文件搬入仅有 20 KiB 的 SRAM 中；SRAM 可以被完整、纯粹地留给动态运行的堆栈与数据对象。而当后续迈向更高端的应用级 SoC 或外部挂载慢速 SPI Flash 的复杂系统时，取指速率与存储拓扑将发生深刻变化，这一物理底层差异在第 5 章探讨嵌入式 Linux 的多级引导机制时，还将再次成为架构推演的焦点。

## 7. 习题

### 7.1 巩固

<Exercise id="60107" :d="1" :w="2">

写一个主机端程序 `classify_chip.c`，把下面五颗芯片按本节的判据分类，只依据片内 Flash、SRAM 与有无 MMU 三个字段：

```c
struct chip {
    const char *name;
    unsigned onchip_flash_kib;
    unsigned sram_kib;
    int has_mmu;
};

static const struct chip chips[] = {
    { "STM32F103C8T6",    64u,  20u, 0 },
    { "STM32MP157",        0u, 512u, 1 },
    { "ESP32-C3",       4096u, 400u, 0 },
    { "Cortex-M33 demo", 1024u, 256u, 0 },
    { "i.MX6ULL",          0u, 128u, 1 },
};
```

输出每颗芯片的一行结论，规则是：有 MMU 为 `application processor`，无 MMU 且片内 Flash 不为 0 为 `microcontroller`，两者都不满足为 `needs review`。用 `gcc -std=c17 -Wall -Wextra` 编译，期望输出：

```out
STM32F103C8T6    flash=  64 KiB sram=  20 KiB mmu=0 -> microcontroller
STM32MP157       flash=   0 KiB sram= 512 KiB mmu=1 -> application processor
ESP32-C3         flash=4096 KiB sram= 400 KiB mmu=0 -> microcontroller
Cortex-M33 demo  flash=1024 KiB sram= 256 KiB mmu=0 -> microcontroller
i.MX6ULL         flash=   0 KiB sram= 128 KiB mmu=1 -> application processor
```

然后回答：ESP32-C3 的 4096 KiB Flash 并不在硅片内，为什么填 4096 仍然得出正确结论，填 0 会得出什么？

</Exercise>

<Exercise id="60108" :d="2" :w="2">

`hello.c` 在 F103 上直接交给 `arm-none-eabi-gcc` 时，链接器报出 10 个未定义符号。本页的表把它们归成「谁调用它」与「原本要做什么」两列。

(1) 不看表，写出这 10 个符号，并按「输出、文件、内存、进程」四类各归到一组。

(2) 下面的程序与 `hello.c` 同源，只把 `printf` 换成写一个全局对象，不做任何输出：

```c
#include <stddef.h>
#include <stdint.h>

uint32_t g_boot_magic = 0x103C8000u;
uint32_t g_sample_buf[16];

static uint32_t fill_and_sum(uint32_t seed) {
    uint32_t sum = 0;
    for (size_t i = 0; i < 16; ++i) {
        g_sample_buf[i] = seed + (uint32_t)i;
        sum += g_sample_buf[i];
    }
    return sum;
}

volatile uint32_t g_total;

int main(void) {
    g_total = fill_and_sum(g_boot_magic);
    return 0;
}
```

用 `arm-none-eabi-gcc -mcpu=cortex-m3 -mthumb -O2 -Wall -Wextra noprintf.c -o noprintf.elf` 链接，把报出的未定义符号与 (1) 比较：少了哪四个？这四者分别经由哪条库内路径被 `printf` 引出来（提示：一个与输出流的缓冲区设置有关，一个与 `abort` 的实现有关）。

(3) 用 `-nostartfiles` 重新链接同一份源文件，观察符号集合的变化，解释 `crt0` 与 C 库各自引出了哪些桩。

</Exercise>

### 7.2 编程

<Exercise id="60109" :d="3" :w="3">

仓库里 `code/嵌入式/1_平台与工具链/1_2_MCU_MPU_SoC与执行环境/syscalls.c` 提供了 10 个桩，其中 `_write` 把字符全部丢弃。本节末尾的 `compare` 给出过把 `_write` 改成向 ITM 输出的一行写法。

写一份只有 8 个函数的 `syscalls_min.c`，让它与 `hello.c` 链接时 `printf` 能通过、且不产生 `_kill`、`_getpid` 的未定义引用。要求：

1. 保留 `_sbrk`、`_write`、`_read`、`_close`、`_fstat`、`_isatty`、`_lseek`、`_exit` 八个，去掉 `_kill` 与 `_getpid`。
2. `_sbrk` 以链接脚本的 `end` 符号为堆起点，用 `extern char end[]` 声明。
3. `_write` 不丢弃字符：把它写进一个全局的 `volatile char g_console_buf[256]` 环形缓冲区（同时维护写指针与计数），供后续断点观察。`_write` 必须返回它「写出去」的字节数，否则 `printf` 内部会把它当成错误。
4. `_exit` 用 `for (;;)` 停住，不要返回。

验收：

```text
$ arm-none-eabi-gcc -mcpu=cortex-m3 -mthumb -O2 -Wall -Wextra \
    hello.c syscalls_min.c --specs=nano.specs -o hello.elf
$ arm-none-eabi-nm -u hello.elf
```

最后一条命令应当没有任何输出。再用 `arm-none-eabi-size hello.elf` 与正文里带 10 个桩的版本（`text` 5800，`data` 108，`bss` 432）对照：`text` 会涨（`_write` 里多了取模与边界判断），`bss` 会涨 256 以上（多出的 `g_console_buf` 与两个计数器）。把差额拆成「去掉两个桩省下的」与「环形缓冲区多出来的」两部分，用 `arm-none-eabi-nm -S` 里 `_kill`、`_getpid`、`g_console_buf` 的大小核对。

</Exercise>

<Exercise id="60110" :d="3" :w="3">

写一个主机端工具 `abi_report.c`，读取 `arm-none-eabi-readelf -A` 的输出（从标准输入读，不调用外部程序），把 ELF 属性整理成一行结论：

```text
self-check: cpu=7-M profile=Microcontroller thumb=Thumb-2 float=soft
```

要求：

1. 输入是 `readelf -A` 的原始文本，形如 `  Tag_CPU_name: "7-M"`、`  Tag_CPU_arch_profile: Microcontroller`、`  Tag_THUMB_ISA_use: Thumb-2`。字段缺失时打印 `unknown`。
2. `float` 一列由 `Tag_ABI_VFP_args` 是否存在决定：存在为 `hard`，不存在为 `soft`。
3. 不接受命令行参数，输入为空时退出码为 1。

用四份真实的属性输出验证，四份都由本机的 `arm-none-eabi-gcc` 生成：

```bash
arm-none-eabi-gcc -mcpu=cortex-m0  -mthumb -O2 -c empty.c -o m0.o
arm-none-eabi-gcc -mcpu=cortex-m3  -mthumb -O2 -c empty.c -o m3.o
arm-none-eabi-gcc -mcpu=cortex-m4  -mthumb -O2 -c empty.c -o m4.o
arm-none-eabi-gcc -mcpu=cortex-m4  -mthumb -mfpu=fpv4-sp-d16 -mfloat-abi=hard -O2 -c empty.c -o m4hf.o
```

`empty.c` 只有一个空函数。把四份输出分别喂给程序，期望依次得到 `cpu=6S-M`、`cpu=7-M`、`cpu=7E-M`、`cpu=7E-M`，最后一份的 `float=hard`。再说明：为什么 `-mcpu=cortex-m3 -mfloat-abi=hard` 连编译都过不去，而 `-mfpu` 不加时 `cortex-m4` 仍然是 `soft`。

</Exercise>

<Exercise id="60111" :d="3" :w="3">

写一个程序 `freestanding_check.c`，用预处理宏在编译期判断当前编译目标，把结论写进两个全局常量：

```c
#if defined(__ARM_ARCH_7EM__)
#define CORE_NAME "ARMv7E-M"
#elif defined(__ARM_ARCH_7M__)
#define CORE_NAME "ARMv7-M"
#elif defined(__ARM_ARCH_6M__)
#define CORE_NAME "ARMv6-M"
#else
#define CORE_NAME "unknown"
#endif

const char *core_name = CORE_NAME;
```

再加一个 `int has_hw_div`，用 `__ARM_FEATURE_IDIV` 是否为 1 决定。要求：

1. 同一份源文件在 `-mcpu=cortex-m0`、`cortex-m3`、`cortex-m4` 三种选项下各预处理一次，用 `arm-none-eabi-gcc -E -P -mcpu=... probe.c` 读出结论并列出表格。

2. 用同样的办法确认 `__STDC_HOSTED__` 在加不加 `-ffreestanding` 时分别是 1 和 0，把两条 `-dM -E` 的输出贴出来。

3. 把两行结论（`core_name`、`has_hw_div`）与正文里 Cortex-M 内核表逐项对照，指出哪几项与表一致、哪几项表里没写。

</Exercise>

<Exercise id="60112" :d="4" :w="3">

本节说 `printf` 的字符最终交给 `_write`，中间隔着 `_write_r`。这个调用链可以用 `arm-none-eabi-objdump -d` 直接看出来。

以正文 5.4 的产物为起点（`hello.c` 加上自己写的 `syscalls.c`，只加 `--specs=nano.specs`）：

(1) 用 `arm-none-eabi-objdump -d hello.elf > dis.txt`，在 `dis.txt` 里找出 `_write_r`，贴出它调用 `_write` 的那几条指令，指出 `bl` 的目标地址与 `_write` 的 `nm` 地址是否一致。

(2) `printf` 到 `_write_r` 之间并不是直接调用。找出 `_vfprintf_r` 与 `__sfvwrite_r`，说明中间那一层为什么是 `blx` 而不是 `bl`，被调用的函数指针存在哪里。

(3) 把 `_write` 改成向环形缓冲区写入，重新链接，用 `arm-none-eabi-nm -S` 比较改动前后 `_write` 的地址与大小。缓冲区是 `volatile` 的，为什么 `-O2` 不会把循环里的写操作优化掉？

</Exercise>

<Exercise id="60113" :d="4" :w="4">

写一份最小固件的链接约定验证程序，不用任何 C 库，只用独立环境保证的头文件。

1. 写 `nostdlib_main.c`：包含 `<stddef.h>` 与 `<stdint.h>`，定义一个有初值的全局对象、一个十六字节数组，在 `main` 里累加求和并写进一个 `volatile` 结果对象，最后 `for (;;)` 停住。要求 `#include` 里不出现 `<stdio.h>`、`<stdlib.h>`、`<string.h>`。
2. 用下面的命令链接，不加任何 `--specs`、不加 `-lc`：

```bash
arm-none-eabi-gcc -std=c17 -mcpu=cortex-m3 -mthumb -O2 -Wall -Wextra \
    -ffreestanding -nostdlib -Wl,-e,main nostdlib_main.c -o nostdlib.elf
```

3. 记录 `arm-none-eabi-nm -u nostdlib.elf`（应当没有输出）、`arm-none-eabi-size nostdlib.elf`、以及 `arm-none-eabi-readelf -h nostdlib.elf | grep Entry` 的入口地址。解释入口地址为什么以 1 结尾，以及这个 `.elf` 为什么仍然不能烧进 F103。

4. 分三步把这个程序「弄坏」，每一步记录新的报错并说明原因：

   - 把 `main` 里的 `for (;;)` 去掉，让 `main` 正常返回，用 `arm-none-eabi-objdump -d nostdlib.elf` 看 `main` 末尾的 `bx lr`：既然没有任何启动代码调用过 `main`，`lr` 里此刻是谁留下的值？把这条 `bx lr` 与正文里「固件的 `main` 一律以 `for (;;)` 结束」那句对上。
   - 在源文件里加一句 `#include <string.h>` 并调用 `strlen("abc")`；
   - 把 `uint32_t` 的求和应用改成 `uint64_t`，用 `uint64_t` 做除法。

</Exercise>

<Exercise id="60114" :d="5" :w="4" scope="*">

给一节嵌入式课程写一个「执行环境判定」小工具 `env_probe.c`，它同时面向主机与交叉编译器，用同一份源文件回答三个问题：这份代码在哪种环境下编译、目标内核属于哪一系列、目标上哪些 C 标准库设施必须由用户补上。

要求：

1. 用 `__STDC_HOSTED__`、`__ARM_ARCH`、`__ARM_ARCH_PROFILE` 三个宏判断，把结论写成一行：

```text
hosted=1 arch=7 profile=77 (Cortex-M) float=soft
```

在主机上编译并运行，应当得到 `hosted=1` 与非 Arm 的结论（`arch` 一栏打印 `unknown`）；用 `arm-none-eabi-gcc -mcpu=cortex-m3 -mthumb` 交叉编译后，用 `arm-none-eabi-objdump -s -j .rodata` 或调试器读出同一行字符串。

2. 程序里维护一张表，把 `_write`、`_sbrk`、`memcpy`、`memset`、`strlen` 五个符号映射到「何时需要用户提供」：`_write` 与 `_sbrk` 在链接完整的 newlib 时由 `crt0` 与库的退出路径引出，因此哪怕 `main` 是空的也会缺；`memcpy`、`memset` 只在这段代码被单独编译、还没有链接 C 库时以未定义符号出现，因为它们的调用是优化器生成的；`strlen` 在 `-ffreestanding` 下不会被内建函数折叠，会真的变成一个外部调用。用 `arm-none-eabi-nm -u` 验证，注意区分「看 `-c` 编出的 `.o`」与「看链接后的 `.elf`」：

   - `printf` + 不加 specs，看链接后的 `.elf`；
   - 空 `main` + `--specs=nano.specs`，看链接后的 `.elf`；
   - 一个 64 个 `uint32_t` 成员的结构体整体赋值 + `-ffreestanding -O2`，只看 `-c` 编出的 `.o`（链接后 C 库会补上 `memcpy`，`.elf` 里就看不到了）。

3. 把三种构建的未定义符号集合打印成并排的表格，并在结语里写清：为什么空 `main` 也会缺 `_sbrk`，而只做结构体赋值的 `.o` 缺的是 `memcpy`，以及为什么后一种情况在链接之后会消失。

提示：三种构建都可以在主机上用 `arm-none-eabi-gcc` 完成，不需要硬件。

</Exercise>

## 8. 参考资料

1. GCC 官方文档，[Standards](https://gcc.gnu.org/onlinedocs/gcc/Standards.html)：`-ffreestanding`、`__STDC_HOSTED__` 与独立环境的要求。
2. newlib 文档，[System Calls](https://sourceware.org/newlib/libc.html#Stubs)：`_write`、`_sbrk` 等桩函数的原型与语义。
3. Arm，[Armv7-M Architecture Reference Manual](https://developer.arm.com/documentation/ddi0403/latest)：Cortex-M3 的模式、特权级与指令集。
4. STMicroelectronics，[*DS5319 Datasheet: STM32F103x8, STM32F103xB*, Rev 20](https://www.st.com/resource/en/datasheet/stm32f103c8.pdf)：F103C8T6 的存储器容量与引脚。
5. C++ Reference，[Conformance](https://en.cppreference.com/w/c/language/conformance)：独立环境与宿主环境的定义、独立环境保证的头文件。
6. STMicroelectronics，[cmsis_device_f1](https://github.com/STMicroelectronics/cmsis_device_f1)：`stm32f103xb.h` 中的 `__MPU_PRESENT`、`__NVIC_PRIO_BITS`、`__CM3_REV`。
