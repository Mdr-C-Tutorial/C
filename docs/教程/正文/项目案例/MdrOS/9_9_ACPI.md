# 9.9 ACPI

高级配置与电源管理接口 (Advanced Configuration and Power Interface)

ACPI 是一种描述硬件接口的方法，用于操作系统与计算机硬件之间的通信。它提供了一种标准化的方法来管理电源管理、热管理和配置功能。

ACPI 允许操作系统控制计算机的电源状态，例如休眠、待机和关闭。包括：系统电源管理（System Power Management）、设备电源管理 (Device Power Management) 和处理器电源管理 (Processor Power Management). 它还提供了一种方法来管理电源供应和电池状态 (Battery Management).

ACPI 可以监测和控制计算机的温度，并根据需要调整风扇的转速。它还可以降低设备的性能以防止过热。

ACPI 提供了一种方法来配置计算机硬件和外部设备。它可以识别和管理各种设备，包括处理器、内存、硬盘驱动器和外部接口等。

ACPI 的运行时模型用于管理在系统运行期间发生的所有 ACPI 事件。它处理中断事件，并评估处理这些事件所需的对象。当用户启动系统时，系统固件会完成 ACPI 的设置、初始化和自我测试。

ACPI 可以实现的功能包括：

- 系统电源管理（System power management）
- 设备电源管理（Device power management）
- 处理器电源管理（Processor power management）
- 设备和处理器性能管理（Device and processor performance management）
- 配置/即插即用（Configuration/Plug and Play）
- 系统事件（System Event）
- 电池管理（Battery management）
- 温度管理（Thermal management）
- 嵌入式控制器（Embedded Controller）
- SMBus 控制器（SMBus Controller）

> ioapic 的开启以及 SMP 多处理器的开启也需要 ACPI

## 1. ACPI Table

ACPI 提供了一组用于描述控制设备和描述设备信息方式的结构，它们按照一种层级结构组织。

以下是 ACPI 提供的一些表

- Root System Description Table (RSDT)
- Fixed ACPI Description Table (FADT)
- Firmware ACPI Control Structure (FACS)
- Differentiated System Description Table (DSDT)
- Secondary System Description Table (SSDT)
- Multiple APIC Description Table (MADT)
- Smart Battery Table (SBST)
- Extended System Description Table (XSDT)
- Embedded Controller Boot Resources Table (ECDT)
- System Locality Distance Information Table (SLIT)
- System Resource Affinity Table (SRAT)
- Corrected Platform Error Polling Table (CPEP)
- Maximum System Characteristics Table (MSCT)
- ACPI RAS Feature Table (RASF)
- Memory Power State Table (MPST)
- Platform Memory Topology Table (PMTT)
- Boot Graphics Resource Table (BGRT)
- Firmware Performance Data Table (FPDT)
- Generic Timer Description Table (GTDT)
- NVDIMM Firmware Interface Table (NFIT)

## 2. APIC

高级可编程中断控制器（Advanced Programmable Interrupt Controller）

一种中断控制器架构，通常多见于 Intel 32 位架构（Intel Architecture-based 32-bit）的 PC 系统。APIC 架构支持多处理器中断管理（中断均匀地分布在所有处理器），多 I/O 子系统支持，与 8259A 兼容，并且支持处理器间中断（Inter-Processor Interrupt，IPI）。这个架构由直属于处理器的本地 APICs（Local APICs）和在（南桥）芯片中的 I/O APIC 组成。

## 3. 习题

这一页把 ACPI 说成了一个框架：一组描述表、一套电源状态、以及靠 MADT 才能开起来的多处理器。正文没有给出任何一段可运行的代码，因为真正的 ACPI 解析要在引导之后读物理内存。下面四道题把这块补上：前两道在主机上遍历固件给出的表结构，后两道把表里的字段接到实际动作（对 IO APIC 编程、通过 PM1 寄存器关机）。整套固定数据在 `code/项目/mdrOS/exercises/9_9/acpi_fixture.h`，所有题目都用它，不用真机。

### 3.1 巩固

<Exercise id="50050" :d="2" :w="2">

ACPI 的入口是 RSDP（Root System Description Pointer）。固件把它放在 `0xE0000`～`0xFFFFF` 之间的某处，每个 16 字节边界上都要查一遍。下面是从 `0xE0000` 起的 128 字节：

```c
static const uint8_t rsdp_region[128] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x52, 0x53, 0x44, 0x20, 0x50, 0x54, 0x52, 0x20,
    0xC3, 0x44, 0x45, 0x43, 0x4F, 0x59, 0x20, 0x00, 0xAD, 0xDE, 0x00, 0x00,
    0x00, 0x52, 0x53, 0x44, 0x20, 0x50, 0x54, 0x52, 0x20, 0x56, 0x4D, 0x44,
    0x52, 0x54, 0x53, 0x54, 0x00, 0xEF, 0xBE, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x52, 0x53, 0x44, 0x20, 0x50, 0x54, 0x52, 0x20, 0x0E, 0x4D, 0x44, 0x52,
    0x54, 0x53, 0x54, 0x00, 0x00, 0xF4, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};
```

(1) RSDP 的签名是 `"RSD PTR "`（末尾有一个空格，共 8 字节）。逐字节扫描这 128 个字节，哪几个偏移上出现了这个签名？把它们各自的 20 字节（从签名起）抄出来。

(2) RSDP 的第 8 字节是校验和：从签名开始的 20 个字节按 8 位相加必须为 0。对三个候选逐个算，哪几个通过、哪一个不通过？通过的两个里，RSDT 地址字段各是多少？

(3) 三个候选里有两个是故意放的干扰项。规范要求在 `0xE0000`～`0xFFFFF` 之间按 16 字节边界扫描，`0x25` 处的签名校验和是过的，却仍然不该被采用。说明扫描循环为什么要写 `off += 16` 而不是 `off++`。再指出下面三种实现各会选中哪个候选：只查签名、逐字节扫描；只查签名、按 16 字节步长；查签名与校验和、按 16 字节步长。

(4) 偏移 `0x10` 那个候选的 RSDT 地址字段是 `0xDEAD`，明显是无效值，但它的校验和也不对。如果只检查签名不检查校验和，会拿这个假地址去做什么？写出 32 位下这个地址落在哪一段物理内存（按 PC 的内存布局：低 640 KiB、`0xA0000`～`0xFFFFF` 是固件区、`0xC0000000` 起是 PCI 空间）。

验收：第 (2) 问的三个校验和依次是 `0x01`、`0x00`、`0x00`；第 (3) 问三种实现依次选中 `0x10`、`0x10`、`0x60`；第 (4) 问的 `0xDEAD` 落在低 640 KiB。

</Exercise>

<Exercise id="50051" :d="3" :w="3">

RSDT 是一张「表目录」：它的表头之后是一串 4 字节物理地址，每个指向另一张表。给定 RSDT 在 `0x1F400`，内容如下：

```c
static const uint8_t rsdt_table[56] = {
    0x52, 0x53, 0x44, 0x54, 0x38, 0x00, 0x00, 0x00, 0x01, 0x85, 0x4D, 0x44,
    0x52, 0x54, 0x53, 0x54, 0x52, 0x53, 0x44, 0x54, 0x44, 0x45, 0x4D, 0x4F,
    0x01, 0x00, 0x00, 0x00, 0x4D, 0x44, 0x52, 0x20, 0x01, 0x00, 0x00, 0x00,
    0x00, 0xF0, 0x01, 0x00, 0x00, 0xF1, 0x01, 0x00, 0x00, 0xF2, 0x01, 0x00,
    0x00, 0xF3, 0x01, 0x00, 0x00, 0xF5, 0x01, 0x00,
};
```

五张表分别放在 `0x1F000`（FADT，116 字节）、`0x1F100`（MADT，108 字节）、`0x1F200`（DSDT，42 字节）、`0x1F300`（SSDT，36 字节）与 `0x1F500`（不存在）。所有表的表头布局相同：

| 偏移 | 长度 | 字段 |
| --- | --- | --- |
| 0 | 4 | 签名 |
| 4 | 4 | 表长（含表头） |
| 8 | 1 | 修订号 |
| 9 | 1 | 校验和 |
| 10 | 6 | OEM ID |
| 16 | 8 | OEM 表 ID |
| 24 | 4 | OEM 修订号 |
| 28 | 4 | 创建者 ID |
| 32 | 4 | 创建者修订号 |

(1) RSDT 的表长字段是多少？表头占 36 字节，算出一共有几个表地址。把这五个地址写成十六进制。

(2) 打一份「表清单」：对每个地址读出 36 字节表头，打印签名、表长、修订号、OEM 表 ID 与校验和字段。`0x1F500` 应当报「未映射」，不要当成读到了 0。

(3) 校验一：把整张表（长度取自表长字段）的字节按 8 位求和，结果为 0 才有效。四张存在的表里哪一张通不过？它的校验和字段应改成多少才通过？

(4) 校验二：表长字段必须是 4 的倍数且不小于 36，否则后面按表长做的扫描会越界。写一个 `table_check(addr, max_len)`，把「未映射」「表长非法」「校验和错」三种情况分别返回不同的错误码，并在 `main` 里逐表打印结论。

(5) 为什么 RSDT 里能出现一个指向未映射地址的项？固件写这张表时不会犯这种错，那这一项在这个习题里代表了什么真实情况的简化？

验收：第 (2) 问五行的签名依次是 `FACP`、`APIC`、`DSDT`、`SSDT` 与「未映射」；第 (3) 问指出 SSDT 的校验和应改成 `0x95`；第 (4) 问的三种错误码各不相同。

</Exercise>

### 3.2 编程

<Exercise id="50052" :d="4" :w="4">

解析 MADT，把它变成「这台机器有几个核、每个核的 APIC ID 是多少、中断怎么路由」。

MADT 在 `0x1F100`，108 字节。表头之后从偏移 44 开始是一串变长条目，每条的首字节是类型、次字节是长度（含这两字节）：

| 类型 | 名称 | 关键字段 |
| --- | --- | --- |
| 0 | Processor Local APIC | 偏移 2：ACPI 处理器 ID；偏移 3：APIC ID；偏移 4：4 字节标志，bit 0 为 1 表示启用 |
| 1 | I/O APIC | 偏移 2：I/O APIC ID；偏移 4：4 字节 MMIO 基址；偏移 8：4 字节 GSI 基址 |
| 2 | Interrupt Source Override | 偏移 2：总线；偏移 3：源 IRQ；偏移 4：4 字节 GSI；偏移 8：2 字节标志（bit 1 极性 0 高有效 / 1 低有效，bit 3 触发方式 0 边沿 / 1 电平） |

要求：

(1) 实现 `madt_walk()`：从偏移 44 开始按长度字段前进，遇到长度小于 2 或越过表长就停下并报错。逐条打印类型与长度，验证条目边界是 `44, 52, 60, 68, 76, 88, 98`（最后一个在 `0x62`，长度 10）。

(2) 打印每个处理器的 ACPI ID、APIC ID 与启用状态。统计总核数与启用核数，以及最大的 APIC ID。

(3) 打印 I/O APIC 的 MMIO 基址与 GSI 基址，再打印两条中断源覆盖：源 IRQ 0 覆盖到哪个 GSI，源 IRQ 9 覆盖到哪个 GSI，各自的极性与触发方式。两条记录的标志字分别是 `0x0000` 和 `0x000F`，按上表的位定义解出结论。

(4) 写 `gsi_of(irq)`：先查覆盖表，没有覆盖就返回 `irq + gsi_base`。对 IRQ 0、IRQ 2、IRQ 9、IRQ 11 各求一次，列出结果。

(5) 回答：为什么 `ioapic 的开启以及 SMP 多处理器的开启也需要 ACPI`（正文原话）？如果不用 MADT，内核能靠什么知道有几个核？从 `cpuid`、BIOS 数据区、写死的常量三条路里各举一个，并说明为什么 [9.1 项目概述](/教程/正文/项目案例/MdrOS/9_1_项目概述) 的 7.3 节说不要用 `cpuid` 取多核代号。

验收：第 (1) 问的七个偏移与第 (2) 问的四个数字（4 核、2 启用、最大 APIC ID 5）全部吻合；第 (4) 问 IRQ 0 得到 GSI 2、IRQ 9 得到 GSI 9、IRQ 11 得到 GSI 11。

</Exercise>

<Exercise id="50053" :d="5" :w="4">

把 FADT 与 APIC 接到实际动作上，做两个能跑的实验。

实验一：对 I/O APIC 编程，把一条 GSI 接到指定向量。

(1) 从 FADT 读出 `SCI_INT`、`PM1a_EVT_BLK`、`PM1a_CNT_BLK`、`PM_TMR_BLK`、`GPE0_BLK` 与各自的长度字段，打印成一张表。字段偏移：`SCI_INT` 在 `0x2E`（2 字节）、`PM1a_EVT_BLK` 在 `0x38`、`PM1a_CNT_BLK` 在 `0x40`、`PM_TMR_BLK` 在 `0x4C`、`GPE0_BLK` 在 `0x50`，长度在 `0x58`、`0x59`、`0x5B`、`0x5C`。

(2) I/O APIC 的寄存器通过两个 MMIO 双字访问：偏移 `0x00` 写寄存器号，偏移 `0x10` 读写数据。中断重定向表项（redirection table entry，RTE）的寄存器号是 `0x10 + 2 * gsi`，低 32 位在偶数号、高 32 位在奇数号。低 32 位的 bit 0～7 是向量，bit 13 是极性（1 为低有效），bit 15 是触发方式（1 为电平），bit 16 是屏蔽位。按 MADT 的覆盖表，写出两个 RTE 的低 32 位：

- 定时器走的是源 IRQ 0，经覆盖落到 GSI 2，向量取 `0x20 + 0`；
- 源 IRQ 9 经覆盖落到 GSI 9，向量取 `0x20 + 9`，极性与触发方式按覆盖表那条记录的标志字来。

实测两个值分别是 `0x00000020` 与 `0x0000A029`。再写出这两个 RTE 对应的寄存器号（各要写几个寄存器）。

(3) 说明为什么系统时钟的中断要经过 IRQ 0 → GSI 2 的覆盖，而内核代码里看到的却是「定时器接在 GSI 2 上、向量 0x20」。如果不做覆盖，直接按 `GSI = IRQ` 把定时器编到 GSI 0，会收到哪个中断？

实验二：用 PM1a_CNT 关机。

(4) 补全 `code/项目/mdrOS/exercises/9_9/acpi_pm_starter.c` 里的 `acpi_enable()`：向 `SMI_CMD` 写 `ACPI_ENABLE`，然后轮询 `PM1a_CNT` 的 bit 0（SCI_EN），最多 1000 次。模拟器要读三次才置位。跑出来的 `polls` 应当是 3。

(5) 补全 `acpi_shutdown()`：读 `PM1a_CNT`，保留低 13 位，把 SLP_TYP（bit 10～12）置 5、SLP_EN（bit 13）置 1，写回。模拟器把 PM1a_CNT 初值设为 `0x0000`，写完应当是 `0x3401`。说明为什么必须先置 SCI_EN 再写 SLP_EN，顺序反过来会怎样。

(6) `PM1_CNT_LEN` 在 FADT 里的作用是告诉内核这个寄存器有多宽（2 字节还是 4 字节）。如果按 4 字节去写而硬件只实现了 2 字节，读回来的高 16 位是什么？为什么内核要用 `PM1_CNT_LEN` 而不是写死 `uint16_t`？

验收：第 (2) 问两个 RTE 值与上面一致；第 (5) 问程序输出的 `after shutdown: PM1a_CNT=0x3401` 与 `after enable` 行的 `polls=3` 都要对上；第 (6) 问能说出读未实现位时的两种可能结果（全 0 或全 1，取决于芯片）。

</Exercise>
