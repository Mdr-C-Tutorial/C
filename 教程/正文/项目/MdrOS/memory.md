# 内存管理

内存管理在OS中是一项重要功能, 这不仅是能给每个用户程序, 设备驱动合理分配内存, 也关乎OS是否能保障内核内存的安全。

在32位模式下, OS有两种内存管理方式

* 分页式内存管理
* 分段式内存管理

> 在64位长模式下, 分段式内存管理被废除, 分页式内存管理是强制性的

## 分段式内存管理

此种内存管理非常落后, 且无法整理碎片化的内存。故本教程不作详细代码指导，仅简单介绍一下其功能。

分段式内存管理将程序的地址空间划分为逻辑上的段，每个段可以有自己的访问权限和保护机制。
与分页不同，分段允许变量大小的内存分配，这更符合程序的自然结构。
分段的一个主要优点是它能够提供更强的隔离性和安全性，因为每个段可以被赋予独立的访问但是，分段可能会导致内存碎片和复杂的地址转换过程。

### 全局描述符表 (Global Description Table)

在保护模式下通过段寄存器和段偏移量寻址，但是此时段寄存器保存的数据意义与实模式不同。
此时的CS和SS寄存器后13位相当于GDT表中某个描述符的索引，即段选择子。第2位存储了TI值(0代表GDT，1代表LDT)，第0、1位存储了当前的特权级(CPL)。

#### GDT段描述符结构

一个GDT段描述符占用8个字节，包含三个部分：

* 段基址(32位)，占据描述符的第16～39位和第55位～63位，前者存储低16位，后者存储高16位
* 段界限(20位)，占据描述符的第0～15位和第48～51位，前者存储低16位，后者存储高4位。
* 段属性(12位)，占据描述符的第39～47位和第49～55位，段属性可以细分为8种：TYPE属性、S属性、DPL属性、P属性、AVL属性、L属性、D/B属性和G属性。


### 如何配置

在32位保护模式下, GDT的配置是必须的。这里我们用C配置一个简易的GDT, 方便我们后期使用分页式内存管理

1. 结构体声明

在`description_table.h`头文件中声明

    ```c
    #define GDT_LENGTH 6

    #define SA_RPL3 3

    typedef struct gdt_entry_t {
        uint16_t limit_low;           // 段基址 | 低16位置
        uint16_t base_low;            // 段基址 | 高16位置
        uint8_t base_middle;
        uint8_t access;
        uint8_t granularity;
        uint8_t base_high;
    } __attribute__((packed)) gdt_entry_t;

    typedef struct gdt_ptr_t {
        uint16_t limit;
        uint32_t base;
    } __attribute__((packed)) gdt_ptr_t;

    
    void gdt_install(); 
    ```

2. 源文件

在`gdt.c`中声明

    ```c
    #include "description_table.h"

    gdt_entry_t gdt_entries[GDT_LENGTH];
    gdt_ptr_t gdt_ptr;
    ```

3. 各种函数实现

    ```c
    void gdt_set_gate(uint32_t num, uint32_t base, uint32_t limit, uint8_t access, uint8_t gran) {
        gdt_entries[num].base_low     = (base & 0xFFFF);
        gdt_entries[num].base_middle  = (base >> 16) & 0xFF;
        gdt_entries[num].base_high    = (base >> 24) & 0xFF;

        gdt_entries[num].limit_low    = (limit & 0xFFFF);
        gdt_entries[num].granularity  = (limit >> 16) & 0x0F;

        gdt_entries[num].granularity |= gran & 0xF0;
        gdt_entries[num].access       = access;
    }

    void gdt_install() {
        gdt_ptr.limit = sizeof(gdt_entry_t) * GDT_LENGTH - 1;
        gdt_ptr.base = (uint32_t)&gdt_entries;

        gdt_set_gate(0, 0, 0, 0, 0);   // 按照 Intel 文档要求，第一个描述符必须全 0
        gdt_set_gate(1, 0, 0xFFFFFFFF, 0x9A, 0xCF);     // 指令段
        gdt_set_gate(2, 0, 0xFFFFFFFF, 0x92, 0xCF);     // 数据段
        gdt_set_gate(3, 0, 0xFFFFFFFF, 0xFA, 0xCF);     // 用户模式代码段
        gdt_set_gate(4, 0, 0xFFFFFFFF, 0xF2, 0xCF);     // 用户模式数据段

        gdt_flush((uint32_t)&gdt_ptr);
    }
    ```

4. `gdt_flush` 的汇编函数实现

    ```asm
    [global gdt_flush]

    gdt_flush: ; void gdt_flush(uint32_t gdtr);
        mov eax, [esp + 4] ; [esp+4]按规定是第一个参数，即gdtr
        lgdt [eax] ; 加载新gdt指针，传入的是地址

        mov ax, 0x10 ; 新数据段
        mov ds, ax
        mov es, ax
        mov fs, ax
        mov gs, ax
        mov ss, ax ; 各数据段全部划给这一数据段
        jmp 0x08:.flush ; cs没法mov，只能用farjmp/farcall，这里用farjmp刷新cs
    .flush:
        ret ; 刷新完毕，返回
    ```


## 分页式内存管理

在分页系统下,一个程序发出的虚拟地址由两部分组成: 页面号和页内偏移值。
对于32位寻址的系统，如果页面大小为4KB，则页面号占20位，页内偏移值占12位。
而这个翻译过程由内存管理单元(MMU)完成，MMU根据CPU中CR3寄存器存储的页目录基址来寻找页表,进而将线性地址翻译成物理地址再去寻址。
内存管理单元按照该物理地址进行相应访问后读出或写入相关数据。

以下虚拟地址(也叫线性地址)的构成如图所示

![虚拟地址结构](/images/项目/MdrOS/page_virtual.png)

### 页表

页表的根本功能是提供从虚拟页面到物理页框的映射。
因此，页表的记录条数与虚拟页面数相同。
MMU依赖于页表来进行一切与页面有关的管理活动，这些活动包括判断某一页面号是否在内存里，页面是否受到保护，页面是否非法空间等等。
由于页表的特殊地位，决定了它是由硬件直接提供支持，即页表是一个硬件数据结构。

> 所以, 页表整体必须在物理地址上具有结构有效性 (也就是页表所在的区域其线性地址与物理地址必须是一一对应的)
> 否则, MMU会发生严重的错误导致计算机系统直接重启,或者出现缺页情况(某一线性地址无法映射到指定的物理地址)

### 物理内存管理

为了方便管理,我们将物理内存分成以4kb为大小的一个一个页框,这些页框可以被页映射。

物理内存管理没有过多的复杂逻辑结构, 你只需要能合理分配即可(常用方法是在页框头打上标记以区分该页框是否被占用)

> 页框不止可以以4kb为大小, 你也可以设置为4MB、1GB等大小。但是那样的页太大了,一般用于64位更大的内存上管理


由于C代码对于内存管理整个模块的耦合性较高, 无法单独将代码片段完整展示出来。故列出了所有的源文件

* [**kheap.c**](/教程/示例代码/项目/mdrOS/kheap.c) 内核堆实现
* [**page.c**](/教程/示例代码/项目/mdrOS/page.c) 分页实现
* [**memory.h**](/教程/示例代码/项目/mdrOS/memory.h) 各种结构的预定义
* [**memory.c**](/教程/示例代码/项目/mdrOS/memory.c) 常用内存操作函数

> 其中可以删除有关task的代码, 该代码是多进程的一部分实现。我们没进行到多进程那一步可以先不用, 
> 现在首要任务是确保我们所写的分页式内存管理可用

## 堆内存管理

堆内存是基于每个进程的专属区域内存管理方式, 是一种软件实现的内存管理方式。

堆内存管理会向用户程序提供两个函数, 用户程序将使用这两种函数合理安排自己所使用的内存。

* `void* malloc(size_t size);` 向OS申请分配一段指定大小的内存
* `void free(void* ptr);` 释放一块已分配的内存

> 什么? 你问我`realloc`函数怎么不在内, 哦这个其实可以你自己实现,并不需要通过操作系统给你提供API
