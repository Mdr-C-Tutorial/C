# 1.2 MCU、MPU、SoC 与执行环境

一块 Blue Pill 插上电脑，ST-LINK 认出芯片，几十 KB 的固件烧进去就跑起来；换一块带 DDR 颗粒的核心板，同样一根 USB 线，烧进去的是几 MB 的 Linux 镜像，冷启动要好几秒。两块板子上都有 Arm 内核，都有存储器和外设，差别在存储系统的组织方式和有没有内存管理单元，软件形态随之分成裸机与 Linux 两条路。

编译时的差别同样直接。同一份 `hello.c`，在主机上编译运行一切正常，交给 `arm-none-eabi-gcc` 却连链接都过不去，报的是一串 `_sbrk`、`_write` 未定义。这些符号不在源文件里，也不在 C 标准库里，它们属于 C 语言规定的另一套运行环境。本节先按存储系统把芯片分类，再看 C 标准对运行环境的两种规定，最后把 `printf` 程序编到 STM32 上，看清链接为什么失败、补上什么才能通过、通过之后又缺了什么。

## 1. 三块芯片

本教程的参考硬件是 STM32F103C8T6 最小系统板。为了说明它处在什么位置，先看三块用途完全不同的芯片。

STM32F103C8T6 是一颗微控制器 (Microcontroller Unit, MCU)。片内有 Cortex-M3 内核、64 KiB Flash、20 KiB SRAM，以及 GPIO、USART、SPI、I²C、ADC、定时器等外设，全部封装在 LQFP48 的 48 个引脚里。程序存放在片内 Flash 上，取指和取数据都直接访问这块 Flash，不需要先搬到别处；从上电到执行第一条指令只要几毫秒，整套最小系统的物料成本在十元上下。

STM32MP157 是另一类芯片。它内部有两个 Cortex-A7 应用核、一个 Cortex-M4 实时核，还有 3D 图形处理器和一堆高速接口，但片内只有几百 KB 的 SRAM，程序和数据必须放在外部的 DDR3 颗粒与 eMMC 上。它带有内存管理单元 (Memory Management Unit, MMU)，能跑完整的 Linux。价格是 F103 的许多倍，还要配上电源管理芯片、DDR 布线和多层电路板。

ESP32-C3 介于两者之间。它用 RISC-V 内核，主频 160 MHz，片内 400 KiB SRAM 与 384 KiB ROM，模组里另封了一片 Flash，还把 Wi-Fi 和低功耗蓝牙的射频部分集成在同一个硅片上。它没有 MMU，跑的是 ESP-IDF 提供的裸机与 RTOS 环境。

| 项目 | STM32F103C8T6 | STM32MP157 | ESP32-C3 |
| --- | --- | --- | --- |
| 内核与主频 | Cortex-M3，72 MHz | 2 × Cortex-A7，最高 800 MHz；1 × Cortex-M4，209 MHz | 32 位 RISC-V 单核，最高 160 MHz |
| 片内存储器 | 64 KiB Flash、20 KiB SRAM | 片内 SRAM 数百 KiB，程序与数据主要放外部 | 400 KiB SRAM、384 KiB ROM，Flash 封装在模组内 |
| 片外存储器 | 无，需要时可外挂 SPI Flash | DDR3/DDR3L、eMMC、SD 卡 | 无，模组内已含 Flash |
| 内存管理单元 | 无 | Cortex-A7 有 MMU | 无 |
| 射频 | 无 | 无 | Wi-Fi 4、蓝牙 5 |
| 典型软件形态 | 裸机、RTOS | 嵌入式 Linux，M4 核跑实时任务 | 裸机、RTOS（ESP-IDF） |
| 本教程中的角色 | 全部实验的参考硬件 | 本节与第 5 章的对照 | 3.8 节的 AT 命令模块 |

三块芯片都能算「一台完整的计算机」，都能用 C 写程序，但「程序放在哪里、谁来管理内存」这两件事完全不同。后文所有关于启动、调试、外设的讨论，默认都指第一块。

## 2. MCU、MPU 与 SoC

微处理器 (Microprocessor Unit, MPU) 这个词指的是只有处理器核心、没有片内程序存储器和外设的芯片，典型代表是个人电脑里的 x86 处理器，以及 i.MX6ULL 这类嵌入式应用处理器。使用 MPU 的系统必须在外围配上 DRAM、Flash 和电源管理芯片，存储器管理、外设访问都依赖外部电路和操作系统。

片上系统 (System on Chip, SoC) 说的是集成度：把处理器、图形处理器、射频、存储控制器、各种外设集成到同一块硅片上。按这个定义，STM32F103、STM32MP157、ESP32-C3 都是 SoC，这个词单独出现时说明不了多少问题，工程里更多是作为产品分类使用。

三个词因此有大量重叠。同一颗 STM32MP157，有人叫它 MPU，有人叫它 SoC；ESP32-C3 既可以说成集成射频的 SoC，也可以叫 MCU。工程沟通中真正有用的是下面两个判断，按顺序看：

1. 存储系统。程序是从片内 Flash 直接取指执行，还是要先加载到外部 DRAM？静态数据放在片内 SRAM，还是几十 MB 的 DRAM？
2. 有没有 MMU。有 MMU 才能给每个进程一套独立的虚拟地址空间，这是跑通用操作系统的前提。

两条都落在「片内」的，是 MCU；程序必须放在外部 DRAM、并且带 MMU 的，是 MPU 或应用处理器 SoC。射频、GPU 这类集成的部件不影响这个判断，只影响它能做什么产品。

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

Arm 公司自己不卖芯片，只设计内核并授权。它的内核按用途分三个系列：

- Cortex-A (Application)：面向应用处理器，带 MMU，主频从几百 MHz 到数 GHz，跑 Linux、Android。
- Cortex-R (Real-time)：面向硬实时，带紧耦合存储器与确定性中断响应，常见于硬盘控制器、车载制动、基带处理。
- Cortex-M (Microcontroller)：面向微控制器，指令集精简、中断延迟固定、功耗低，是本节和后续各章的对象。

Cortex-M 系列内部的差异集中在指令集版本和外设上：

| 内核 | 架构版本 | 主要特点 |
| --- | --- | --- |
| Cortex-M0、M0+ | ARMv6-M | 指令最少，成本最低，没有硬件除法指令 |
| Cortex-M3 | ARMv7-M | Thumb-2 指令集，硬件除法，NVIC，位带 |
| Cortex-M4 | ARMv7E-M | 在 M3 基础上加 DSP 指令和单精度浮点单元（可选） |
| Cortex-M7 | ARMv7E-M | 双发射流水线、指令与数据缓存，主频更高 |
| Cortex-M23 | ARMv8-M Baseline | 面向低功耗，可带 TrustZone |
| Cortex-M33 | ARMv8-M Mainline | DSP、浮点、TrustZone，M4 的后续 |

ESP32-C3 用的是 RISC-V 指令集，和 Arm 没有关系。RISC-V 是开放的指令集规范，任何人都可以实现，近年在低成本 MCU 上增长很快；本教程只在 3.8 节把它当 AT 命令模块使用，不涉及它的编程。

本教程的 STM32F103C8T6 用的是 Cortex-M3。它只执行 Thumb 与 Thumb-2 指令，没有浮点单元，浮点运算由软件模拟；带嵌套向量中断控制器 (Nested Vectored Interrupt Controller, NVIC) 和系统节拍定时器 (SysTick)；有硬件除法指令，但没有存储器保护单元 (Memory Protection Unit)。最后一点可以在 ST 的设备头文件里直接查到：`stm32f103xb.h` 把 `__MPU_PRESENT` 定义为 0，同一个文件里 `__NVIC_PRIO_BITS` 是 4，`__CM3_REV` 是 0x0200（内核 r2p0）。设备头文件的用法在 [3.1 驱动接口、CMSIS 与设备头文件](/教程/正文/嵌入式/3_驱动_通信与中间件/3_1_驱动接口_CMSIS与设备头文件) 里详细讲。

::: generate emb-exec-cortex-m-series kind=explanation concepts=emb-exec-classify-by-memory

面向只学过 C 语言、第一次接触 Arm 内核的读者，把 Cortex-M0、M3、M4、M7 放在一起对照：用「一条 32 位整数乘法要几个周期」「有没有浮点单元」「中断最多嵌套几层」这三个可感知的指标，说明选型时为什么电机控制常用 M4、低成本家电常用 M0、而本教程选 M3。不要复述表格。

:::

## 4. 执行环境：独立环境与宿主环境

C 标准把程序的运行环境分成两种。

宿主环境 (hosted environment) 指程序跑在操作系统之上。有 `main` 函数，有命令行参数，标准输入输出接到终端或文件，`malloc` 向操作系统要内存，程序结束由操作系统回收资源。个人电脑上的 C 程序都是这一类。

独立环境 (freestanding environment) 指程序直接跑在硬件上，没有操作系统。C 标准对它的要求少得多：程序如何启动、有没有 `main`、`main` 长什么样，全由实现决定；标准只保证下面这 9 个头文件可用（其中 `<stdalign.h>` 与 `<stdnoreturn.h>` 是 C11 加入的）：

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

`<stdio.h>`、`<stdlib.h>`、`<string.h>` 不在保证之列，但它们通常仍能包含，因为交叉工具链照样提供了一份 C 标准库。区别在于这些库函数背后依赖的操作系统服务没有了：`printf` 要把字符送到某个地方，`malloc` 要向某个地方要内存，`fopen` 要有一个文件系统。这些服务在裸机上都不存在。

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

工具链默认按宿主环境编译，加 `-ffreestanding` 才切换到独立环境。预定义的 `__STDC_HOSTED__` 宏把这件事写得很直白：

::: terminal title="bash - 查看 __STDC_HOSTED__"

```text
$ arm-none-eabi-gcc -dM -E - < /dev/null | grep STDC_HOSTED
#define __STDC_HOSTED__ 1
$ arm-none-eabi-gcc -ffreestanding -dM -E - < /dev/null | grep STDC_HOSTED
#define __STDC_HOSTED__ 0
```

:::

即使是独立环境，GCC 也要求提供 `memcpy`、`memmove`、`memset`、`memcmp` 四个函数。原因是优化器会把一些循环和结构体赋值改写成对它们的调用，这与源文件里有没有包含 `<string.h>` 无关。1.8 节的启动文件里有一段复制 `.data` 的循环，编译之后它变成了对 `memcpy` 的调用：

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

符号前面的 `U` 表示未定义，要由链接时找到的实现补上。`_estack`、`_sdata` 这些来自链接脚本，`main` 来自应用，`memcpy` 与 `memset` 来自 C 标准库。这几组依赖关系构成了裸机程序最基本的骨架。

裸机程序里的 `main` 也不是被操作系统调用的。复位处理函数准备好栈、复制完 `.data`、清完 `.bss` 之后，用一条普通的函数调用跳到 `main`（[1.8 复位、启动与链接](/教程/正文/嵌入式/1_平台与工具链/1_8_复位_启动与链接)）。`main` 返回之后没有一个「上一层」可以交还控制权，所以固件的 `main` 一律以 `for (;;)` 结束，永不返回。

::: concept emb-exec-main-no-return title="裸机程序的 main 由启动代码调用"

在裸机上，`main` 是一个普通函数，由复位处理函数在完成存储器和栈的初始化之后调用。调用者不是操作系统，`main` 返回后没有可交还的对象，因此固件的 `main` 必须以 `for (;;)` 之类的无限循环结束。C 标准中「从 `main` 返回等价于调用 `exit`」的规定在这里没有实际意义。

:::

## 5. 实验：同一个 printf 程序

光说「标准库依赖操作系统服务」不够直观。把同一份程序分别编给主机和 STM32，链接器会把缺失的东西一个一个列出来。

示例在 `code/嵌入式/1_平台与工具链/1_2_MCU_MPU_SoC与执行环境/`，`hello.c` 定义了一个有初值的全局对象、一个数组，在 `main` 里填数组、求和、打印：

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

::: terminal title="PowerShell - 主机上的 hello"

```text
$ gcc -std=c17 -Wall -Wextra hello.c -o hello
$ ./hello
sample[0] = 0x103C8000, sum = 0x03C80078
```

:::

这一步没有任何特别之处：`printf` 把格式化结果写进 C 标准库的缓冲区，缓冲区满或程序结束时通过 `write` 系统调用交给操作系统，操作系统再把字符送到控制台。整条路上，`hello.c` 一行都没管过输出是怎么发生的。

### 5.2 直接交给交叉编译器

把编译器换成 `arm-none-eabi-gcc`，加上 `-mcpu=cortex-m3 -mthumb` 指定目标内核，其余照旧：

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

编译器本身没有报错，`hello.c` 完全符合 C 语言；出错的是链接。它找到的 C 标准库是 `arm-none-eabi` 版本，库里的 `printf`、`exit`、`abort` 都靠一组以下划线开头的函数完成真正的输入输出和内存分配，这组函数称为系统调用桩 (system call stub)，约定由使用库的人提供。10 个未定义符号，每一个都对应一件裸机上没有的事：

| 符号 | 谁调用它 | 原本要做什么 |
| --- | --- | --- |
| `_write` | `printf` 输出时的 `_write_r` | 把一块字符写到文件描述符 1 |
| `_read` | `scanf`、`getchar` 等 | 从文件描述符读入字符 |
| `_close` | `fclose`、程序退出 | 关闭文件描述符 |
| `_lseek` | `fseek` | 移动文件位置 |
| `_fstat` | 判断描述符是否为终端 | 取文件状态 |
| `_isatty` | 判断输出要不要行缓冲 | 判断是否为终端设备 |
| `_sbrk` | `malloc` 背后的 `_sbrk_r` | 扩大堆区 |
| `_exit` | `exit`、`abort` | 结束进程 |
| `_kill`、`_getpid` | `signal`、`abort` | 进程与信号管理 |

::: details 完整的链接错误

上面的输出保留了每一条引用，其中 `_exit` 被 `exit` 与 `abort` 各引用一次，按符号名去重后是 10 个。用 `arm-none-eabi-gcc ... hello.c 2>&1 | grep -o "undefined reference to .*" | sort -u` 可以直接得到这份名单。

:::

### 5.3 补上系统调用桩

GCC 为这件事准备了现成的选项。`--specs=nano.specs` 选择体积更小的 newlib-nano，`--specs=nosys.specs` 链接一份全是空实现的系统调用桩，让链接通过：

::: terminal title="bash - 加上 specs 之后"

```text
$ arm-none-eabi-gcc -mcpu=cortex-m3 -mthumb -O2 -Wall -Wextra hello.c \
    --specs=nano.specs --specs=nosys.specs -o hello.elf
$ arm-none-eabi-size hello.elf
   text	   data	    bss	    dec	    hex	filename
   5824	    108	    432	   6364	   18dc	hello.elf
```

:::

链接期间屏幕上会滚过 6 条 `_close is not implemented and will always fail` 之类的警告，这是 `nosys.specs` 在提醒：这些桩只是让链接通过，一调用就失败。链接出来的文件仍然带着一套主机程序的外壳，`readelf -h` 能看到入口地址是 C 运行时的启动代码，而不是 1.8 节里自己写的 `Reset_Handler`：

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

`Machine: ARM` 说明指令集是 Arm，`Tag_CPU_name: "7-M"` 与 `Tag_THUMB_ISA_use: Thumb-2` 说明它瞄准的是 ARMv7-M 的 Thumb-2 内核，也就是 Cortex-M3。`Flags` 里的 `soft-float ABI` 表示浮点参数用通用寄存器传递，这颗芯片没有浮点单元。入口地址 0x8101 的最低位是 1，那是 Thumb 状态的标志位，[1.8](/教程/正文/嵌入式/1_平台与工具链/1_8_复位_启动与链接) 里读复位向量时也见过同样的奇数。`nm` 把这个地址对到了哪个函数上：

::: terminal title="bash - 入口是 C 运行时的启动代码"

```text
$ arm-none-eabi-nm hello.elf | grep -E "_mainCRTStartup| main$"
00008100 T _mainCRTStartup
00008030 T main
```

:::

0x8100 加 1 等于 0x8101，链接器把 C 运行时的启动函数 `_mainCRTStartup` 设成了入口。这个函数会准备堆、初始化 C 库、调用 `main`，最后调用 `exit`。它是为有操作系统的主机准备的，放在 F103 上既没有意义，也没有地方放：不给链接脚本时，链接器按默认脚本把映像从 0x0000_8000 排起，5824 字节的 `text` 段连同 432 字节的 `bss` 落在哪一段地址空间里，与 F103 的 Flash 和 SRAM 无关。

::: details 程序真烧进去会怎样

`hello.elf` 里没有向量表，复位后硬件从地址 0 取的第一个字不会是合法的栈顶。这份文件不是固件，它只是用来观察链接过程。真正的固件由 1.8 节的启动文件、链接脚本和 `-nostartfiles` 组成，2.4 节点亮第一颗 LED 时会给出完整的一套。

:::

### 5.4 自己写系统调用桩

`nosys.specs` 能省事，但它把输出丢进了黑洞。想知道 `printf` 最终把字符交给了谁，可以照库里约定的原型自己写一份。仓库里的 `syscalls.c` 就是最小的一版，其中两个函数最要紧：

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

`_sbrk` 负责堆：`end` 是链接脚本在 `.bss` 之后定义的一个符号，堆从这里往高地址增长，每次 `malloc` 要多少，就把顶端往上推多少。`_write` 是输出的终点：这里它只是把长度原样返回，字符一个也没有送出去。把它改成向串口寄存器写字节，就是 [2.7 UART](/教程/正文/嵌入式/2_MCU裸机开发/2_7_UART) 里的重定向；换成执行 ITM 或半主机的输出，就是 [1.7 SWD、JTAG 与片上调试](/教程/正文/嵌入式/1_平台与工具链/1_7_SWD_JTAG与片上调试) 讲的两种调试输出。

带上这份 `syscalls.c`，只加 `--specs=nano.specs` 也能链接，不再需要 `nosys.specs`：

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

三个符号都在自己的映像里：`_sbrk` 与 `_write` 由 `syscalls.c` 提供，`end` 由链接脚本给出。自己写固件时它来自 [1.8](/教程/正文/嵌入式/1_平台与工具链/1_8_复位_启动与链接) 的 `stm32f103c8.ld`，那段脚本里写着 `PROVIDE(end = .)`，把符号放在 `.bss` 之后；像这里不指定脚本时，链接器的默认脚本里有同样的一行，`nm` 于是把它标成 `B`，地址与 `.bss` 的末尾重合。

同样的 `printf`，在主机上能看到输出，在芯片上却什么也看不到，差别全在 `_write` 里。这也是本节要说明的一件事：C 代码本身不区分宿主环境和独立环境，是链接时选进来的库与桩决定了它能做哪些事。

::: compare left-title="以为加了 specs 就能看到输出" right-title="自己实现 _write，把字符送到某处"

```text
$ arm-none-eabi-gcc ... --specs=nano.specs --specs=nosys.specs hello.c \
    -o hello.elf
$ arm-none-eabi-size hello.elf
   text	   data	    bss	    dec	    hex	filename
   5824	    108	    432	   6364	   18dc	hello.elf
```

链接通过，`printf` 的字符被 `_write` 的空实现原样丢弃，串口和调试器上都看不到任何东西。

---

```c
/* 送到 SWO，见 1.7；把循环体换成写 USART1->DR 就是 2.7 的重定向 */
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

[1.1 嵌入式系统与软件路线](/教程/正文/嵌入式/1_平台与工具链/1_1_嵌入式系统与软件路线) 比较了裸机、RTOS、Linux 三种软件形态的选型。选型的另一半是硬件能不能撑住，这一半由存储系统决定：

| 软件形态 | 需要的硬件条件 | STM32F103C8T6 是否满足 |
| --- | --- | --- |
| 裸机（超级循环 + 中断） | 片内 Flash 与 SRAM，中断控制器，无需 MMU | 满足，20 KiB SRAM 对大多数实验够用 |
| RTOS | 在裸机基础上，每个任务一份栈，RAM 需求随之上升 | 满足，但要给每个任务的栈留出空间 |
| 带任务隔离的 RTOS | 存储器保护单元，任务越界时触发异常 | 不满足，`__MPU_PRESENT` 为 0 |
| 嵌入式 Linux | MMU、数十 MB 以上的 DRAM、块存储与文件系统 | 不满足，没有 MMU，SRAM 只有 20 KiB |

存储器保护单元和微处理器的英文缩写都是 MPU，两者没有关系。前者是 Cortex-M 的一个可选部件，把内存分成若干区域并为每个区域设定访问权限；STM32F103 没有这个部件，所以它上面的 RTOS 任务之间没有硬件隔离，一个任务写坏指针会直接改掉别的任务的数据，[4.7 FreeRTOS](/教程/正文/嵌入式/4_RTOS与固件架构/4_7_FreeRTOS) 会再回到这一点。

Cortex-M3 的两种执行模式与代码里见到的一切都有关，这里先交代一句，细节在第 4 章展开。处理器平时在线程模式 (Thread mode) 下运行 `main` 和普通函数，进入中断时自动切到处理模式 (Handler mode)。两种模式都可以是特权级或非特权级，复位后是线程模式加特权级；非特权级用来运行不被完全信任的代码，让某些指令和寄存器访问触发异常。裸机程序自始至终停在线程模式的特权级，只有中断服务程序在处理模式下执行。

最后是程序放在哪里。STM32F103 的 Flash 与 SRAM 在同一个地址空间里，Cortex-M3 可以直接从 Flash 取指，这种执行方式叫就地执行 (execute in place, XIP)。SRAM 只有 20 KiB，把整个程序复制进去既放不下也不必要。有些系统不是这样：外部 SPI Flash 的读取速度远低于内核主频，必须先把代码搬到 RAM 或经过缓存才能执行，启动时间和内存预算都要按这个来算。这一点在第 5 章讲嵌入式 Linux 的启动过程时还会遇到。

## 7. 习题

### 7.1 巩固

<Exercise id="61101" :d="1" :w="2">

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

<Exercise id="61102" :d="2" :w="2">

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

<Exercise id="61103" :d="3" :w="3">

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

<Exercise id="61104" :d="3" :w="3">

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

<Exercise id="61105" :d="3" :w="3">

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

<Exercise id="61106" :d="4" :w="3">

本节说 `printf` 的字符最终交给 `_write`，中间隔着 `_write_r`。这个调用链可以用 `arm-none-eabi-objdump -d` 直接看出来。

以正文 5.4 的产物为起点（`hello.c` 加上自己写的 `syscalls.c`，只加 `--specs=nano.specs`）：

(1) 用 `arm-none-eabi-objdump -d hello.elf > dis.txt`，在 `dis.txt` 里找出 `_write_r`，贴出它调用 `_write` 的那几条指令，指出 `bl` 的目标地址与 `_write` 的 `nm` 地址是否一致。

(2) `printf` 到 `_write_r` 之间并不是直接调用。找出 `_vfprintf_r` 与 `__sfvwrite_r`，说明中间那一层为什么是 `blx` 而不是 `bl`，被调用的函数指针存在哪里。

(3) 把 `_write` 改成向环形缓冲区写入，重新链接，用 `arm-none-eabi-nm -S` 比较改动前后 `_write` 的地址与大小。缓冲区是 `volatile` 的，为什么 `-O2` 不会把循环里的写操作优化掉？

</Exercise>

<Exercise id="61107" :d="4" :w="4">

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

<Exercise id="61108" :d="5" :w="4" scope="*">

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
