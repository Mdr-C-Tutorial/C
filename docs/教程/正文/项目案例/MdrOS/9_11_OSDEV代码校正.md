# 9.11 OSDev 经典范例的代码校正

在操作系统内核开发领域，OSDev Wiki 是广受参考的社区知识库。然而，其诸多设备驱动条目中的代码范例多成型于早期的 32 位单核环境，且为了简化演示逻辑，往往假设全内存恒等映射并采用了大量硬编码地址。

若直接将这些示例移植到现代 64 位长模式、高半核（Higher-half）虚拟内存映射或启用了分页保护的内核中，极易触发指针截断、DMA 寻址失效以及内存越界覆写等严重故障。本节针对 OSDev 中经典的 AHCI（高级主机控制器接口）SATA 驱动实现，系统梳理关键的代码陷阱与工程校正思路。

## 1. AHCI 驱动实现的架构校正

在参考 OSDev 的 [AHCI 端口内存初始化范例](https://wiki.osdev.org/AHCI#AHCI_port_memory_space_initialization) 时，有以下几处典型的设计陷阱需要重构：

### 1.1 静态硬编码内存基址的动态化重构

在 OSDev 的原始代码第 1 行中：

```c:line-numbers=1
#define AHCI_BASE 0x400000 // 4M
```

将物理内存 `0x400000`（4 MiB 处）硬编码为所有端口控制结构的基地址在生产级内核中极其不可取。这一固定地址极易与内核代码段、页表自身或引导器预留结构发生物理重叠。

在健壮的系统实现中，该区域应当通过内核动态堆（`kmalloc`）或物理页框分配器按需申请连续空间（覆盖 32 个端口通常需要约 1 MiB 缓冲区）。同时必须严格遵循 AHCI 规范的硬件对齐约束：
- 命令列表基址（CLB）必须满足 1 KiB 边界对齐；
- 接收 FIS 基址（FB）必须满足 256 字节边界对齐；
- 命令表基址（CTBA）必须满足 128 字节边界对齐，且单张命令表在物理上不可跨越 4 KiB 边界。

> **物理地址与虚拟地址的严格分离**  
> 外设 DMA 控制器不具备 CPU 内部的 MMU 走查能力，其执行总线读写时直接寻址物理内存。在使用现代高半核引导协议（如 Limine 的高半物理内存直接映射 HHDM）时，写入设备寄存器的必须是扣除了虚拟偏置的真实物理基址，而内核软件对其进行初始化和读写时，则必须使用对应的虚拟地址。

### 1.2 命令表基址（CTBA / CTBAU）的高位补全

在 OSDev 原始代码第 34 行中：

```c:line-numbers=34
cmdheader[i].ctba = AHCI_BASE + (40<<10) + (portno<<13) + (i<<8);
cmdheader[i].ctbau = 0;
```

这段逻辑将高 32 位基址寄存器 `ctbau` 强制置 0，仅在物理内存严格限制在 4 GiB 以下的 32 位环境下能正常运转。

在 64 位长模式下，动态分配的命令表极有可能位于 4 GiB 以上的物理高地址空间。若不显式拆解 64 位物理地址并装填 `ctbau`，DMA 控制器走查命令表时将截断到错误的低地址空间，导致不可预知的内存覆写或命令超时：

```c
uint64_t phys_ctb = AHCI_BASE + (40<<10) + (portno<<13) + (i<<8);
cmdheader[i].ctba = phys_ctb & 0xFFFFFFFF;
cmdheader[i].ctbau = phys_ctb >> 32;
```

### 1.3 物理区域描述符表（PRDT）虚实转换与 64 位适配

在 OSDev 的 [硬盘扇区读取示例](https://wiki.osdev.org/AHCI#Example_-_Read_hard_disk_sectors) 中，代码第 24 行与第 31 行写道：

::: code-group

```c:line-numbers=24
cmdtbl->prdt_entry[i].dba = (uint32_t) buf;
```

```c:line-numbers=31
// Last entry
cmdtbl->prdt_entry[i].dba = (uint32_t) buf;
```

:::

此处直接将软件缓冲区指针 `buf` 强制转换为 32 位并填入数据基址（`dba`），存在双重隐患：
1. **虚拟地址被误当物理地址**：传入的 `buf` 属于当前进程或内核的虚拟地址，而硬件控制器需要的是真实的物理页框地址；
2. **64 位指针截断**：未设置高 32 位数据基地址寄存器（`dbau`）。

在健壮的驱动实现中，无论是读操作还是写操作，均需通过虚实地址转换函数提取出确切的物理地址，并分步装填：

```c
uint64_t phys_buf = (uint64_t)virt_to_phys((uint64_t)buf); // 将虚拟地址转换为硬件 DMA 所需的真实物理地址
cmdtbl->prdt_entry[i].dba = phys_buf & 0xFFFFFFFF;
cmdtbl->prdt_entry[i].dbau = phys_buf >> 32;
```

## 2. 习题

### 2.1 巩固

<Exercise id="50905" :d="2" :w="2">

OSDEV 的端口初始化把三块区域按固定算式排在 `AHCI_BASE` 之后，端口号记为 `portno`：

```c
port->clb  = AHCI_BASE + (portno << 10);
port->fb   = AHCI_BASE + (32 << 10) + (portno << 8);
cmdheader[i].ctba = AHCI_BASE + (40 << 10) + (portno << 13) + (i << 8);
```

(1) 写一个小程序，用 `printf` 打印一张表：`portno` 取 0、1、31，`i` 取 0、31，三列分别是 `clb`、`fb`、`ctba` 的偏移。再手工核对 `portno = 2, i = 5` 一项，确认程序算的和纸上算的相同。

(2) 写出每块区域一个端口占多少字节、32 个端口共占多少字节，以及三块区域合起来的上界。`malloc(1048576)` 够不够，剩下多少？

(3) 命令头里的 `prdtl = 8`，每个命令表 256 字节。说明 8 和 256 这两个数分别由命令表的哪几段算出来。

(4) 常量里的 `32` 和 `40` 是前两块区域的结束位置。一个端口分到的命令表区是 32 项 × 256 字节 = 8 KiB，正好由 `portno << 13` 给出步长。把它改成 `portno << 12` 会怎样？指出相邻两个端口的命令表区重叠多少字节，重叠之后端口 1 写的命令头会落到端口 0 的哪一块里，硬件执行端口 0 的命令时读到的是谁的数据。

</Exercise>

<Exercise id="50906" :d="3" :w="3">

正文把 32 位的 `ctba` 写法改成了 64 位。写一个只在主机上运行的小程序，验证地址的拆分与拼回：

(1) 定义

```c
struct hba_cmd_header {
    uint32_t dw0;    /* 低 16 位是命令 FIS 长度与标志，高 16 位是 PRDTL */
    uint32_t prdbc;  /* 实际传输字节数，由硬件写回 */
    uint32_t ctba;
    uint32_t ctbau;
};

void set_ctba(struct hba_cmd_header *h, uint64_t phys);
uint64_t get_ctba(const struct hba_cmd_header *h);
```

`set_ctba` 写两个 32 位字段，`get_ctba` 拼回。

(2) 写 `check(uint64_t v)`，依次设置、读回、比较，失败时打印原值、`ctba`、`ctbau` 和读回值。用下面几组地址调用（都是 128 字节对齐的合法命令表地址），全部应当通过：

```text
0x0000000000100000
0x00000000FFFFFE00
0x0000000100000000
0x0000000080000080
0x00000001FFFF0000
0x0000000000000000
```

(3) 把 `set_ctba` 改成 `h->ctba = phys;`（C 会把 64 位截断成 32 位，`-Wconversion` 下会有警告），`ctbau` 不写，重跑 `check`，写出哪几组失败、失败时读回值是多少。

(4) 原来的代码里 `cmdheader[i].ctba = AHCI_BASE + (40<<10) + (portno<<13) + (i<<8);` 的右侧全是 `int` 与常量。说明在 64 位内核里这个表达式本身有没有溢出风险，真正的问题出在哪一步。

(5) 把 `AHCI_BASE` 定义成 `0x400000` 时，这套代码能不能访问 4 GiB 以上的命令表？如果 `AHCI_BASE` 来自 `malloc`，在 64 位内核里它可能落在什么范围？

</Exercise>

### 2.2 编程

<Exercise id="50907" :d="4" :w="4">

PRDT（Physical Region Descriptor Table）用来描述一次传输涉及的物理页。每个表项 16 字节：偏移 `0x00` 是数据基址低 32 位，`0x04` 是高 32 位，`0x08` 保留，`0x0C` 的低 22 位是字节数减一，第 31 位是完成中断标志。命令表从 `0x80` 起放 PRDT，256 字节的命令表最多放 8 项。

写一个填充 PRDT 的函数，供一次读取 `len` 字节的 ATA 命令使用：

```c
#define PRDT_MAX 8
#define PRDT_DBC_MAX (4u * 1024 * 1024)   /* 单项最多描述 4 MiB */

struct prdt_entry {
    uint32_t dba;
    uint32_t dbau;
    uint32_t reserved;
    uint32_t dbc;
};

/* 返回写入的项数；放不下时返回 -1，不改动 entries。 */
int build_prdt(struct prdt_entry *entries, uint64_t phys, size_t len, int irq);
```

要求：

(1) 基址必须 4 字节对齐；长度超过单项上限时拆成多项。`dbc` 写字节数减一，最后一项按 `irq` 置第 31 位，其余项该位为 0。

(2) 写测试：对 `len = 1, 4096, 4 MiB - 1, 4 MiB, 4 MiB + 1, 5000` 六种长度，检查各项之和等于 `len`、每项描述的长度不超过 `PRDT_DBC_MAX`、相邻项地址连续、`dbc` 都等于该段长度减一。注意 `4 MiB` 应当写成 `dbc = 0x3FFFFF`，正好占满 22 位。

(3) 用 `phys = 0x2000000`、`len = 4 MiB + 64` 构造一个必须拆成两项的情形，写出两项的 `dba`、`dbau`、`dbc`。再说明为什么 `4 MiB` 正好能放进一项，而 `4 MiB + 1` 不能。

(4) 说明为什么 `dbc` 存的是长度减一，这项设计对单次传输量有什么影响。

(5) 8 项、每项最大 4 MiB，一次命令最多能传多少字节？如果驱动把 `prdtl` 设成 8 但只填了 3 项，剩下的项是什么状态，硬件会读到什么？

</Exercise>

### 2.3 综合与挑战

<Exercise id="50908" :d="5" :w="5">

把写死的 `AHCI_BASE` 换成一个运行时分配的 1 MiB 区域。要求整套布局都在这个区域内，且各段的起始地址满足硬件对齐要求：命令表 128 字节对齐且不跨 4 KiB，命令头列表 1 KiB 对齐，FIS 接收区 256 字节对齐。

(1) 写一个分配器，接口如下。返回的是内核虚拟地址，同时给出物理地址。映射方式取 Limine 的 HHDM：物理地址 `p` 恒在虚拟地址 `hhdm_base + p` 处可访问，`hhdm_base` 由调用者传入：

```c
struct ahci_region {
    void *base;          /* 1 MiB 区域的内核虚拟地址 */
    uint64_t phys_base;  /* 对应的物理地址 */
};

int ahci_region_init(struct ahci_region *r, uint64_t hhdm_base);

/* 返回第 portno 个端口的 clb、fb 的虚拟地址与物理地址；
   命令表第 i 个的地址按 40K + 8K*portno + 256*i 的相同布局给出。 */
void *ahci_clb(const struct ahci_region *r, unsigned portno, uint64_t *phys);
void *ahci_fb(const struct ahci_region *r, unsigned portno, uint64_t *phys);
void *ahci_ctba(const struct ahci_region *r, unsigned portno, unsigned i, uint64_t *phys);
```

(2) 加断言：`clb` 1 KiB 对齐，`fb` 256 字节对齐，`ctba` 128 字节对齐且低 12 位加 256 不超过 4 KiB，三段区域两两不重叠，最高地址不超过 1 MiB。给出编译期或运行期的检查代码。

(3) 用 32 个端口、每端口 32 项的命令表算出各段边界，写一张表：`clb` 起止、`fb` 起止、`ctba` 起止。与正文的 `32<<10`、`40<<10` 对上。

(4) 改成用 `malloc` 分配时，如果返回值不是 4 KiB 对齐，`clb` 的 1 KiB 对齐还成立吗？如果返回值是 `0x...1234`，`ctba` 会不会跨 4 KiB？给出处理办法。

(5) 在 Limine 引导的 64 位内核里，`hhdm_base` 不为 0。说明为什么写给 `clb`、`fb`、`ctba` 的必须是减去 `hhdm_base` 之后的物理地址，而 `memset` 用的必须是虚拟地址。给出这两步各自的代码。

</Exercise>
