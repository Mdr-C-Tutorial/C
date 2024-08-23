# 中断系统

中断, 如名字所示, 是强制性执行流的转移 也就是不管你处于哪个环境。
只要有中断和异常,都会强制执行的。换句话来讲就是从当前流程强制转到另一个流程

中断是OS中一个比较重要的概念, 可以说中断驱动了OS的各种行为. \
如: I/O设备响应, 异常处理, 进程调度等

## 一些开发前要了解的知识

### 中断描述符表 (Interrupt Description Table)

* IDT中存储的段描述符都是系统段描述符
* IDT中的第一个元素不是NULL（不为空）
* IDT可以包含三种门描述符：①任务门描述符；②中断门描述符；③陷阱门描述符

IDT表可以驻留在线性地址空间的任何地方，处理器使用IDTR寄存器来定位IDT表的位置。
这个寄存器中含有IDT表32位的基地址和16位的长度（限长）值。
IDT表基地址应该对齐在8字节边界上以提高处理器的访问效率。
限长值是以字节为单位的IDT表的长度。

IDT有特殊的标志位规定某些中断是否可以由用户程序使用 `int $xx` 指令来触发

#### 中断向量表 (Interrupt Vector Table: IVT)

在x86实模式承担IDT的功能, 其作用与IDT基本相同,与IDT不同的是,IVT固定基址为内存0x00000000处开始,
相比IDT也没有那么多特权级限制

在PC/AT中，规定 内存 储器的最低1 KB用来存放中断向量 (共256个)，称这一片内存区为 中断向量表 ，地址范围是0~3FFH

### 异常中断

异常中断是非可屏蔽中断, 由CPU内部触发，用于向正在运行的内核发出需要其处理的一些异常。
在Intel规定中，前32个中断向量被留为异常

* 在中断流程里发生异常中断叫 `Double Fault`
* 在 `Doubel Fault` 里再次发生中断叫 `Triple Fault`

> 当发生 `Triple Fault` 类型中断时候, 计算机系统意识到发生了内核无法解决的严重错误,
> 这个时候计算机系统会越过ACPI等电源的设置迅速进行重启。

以下是一份IDT各种异常的对照表

![IDT对照表](/images/项目/MdrOS/idt_vector.png)

[**OSDEV Exceptions**](https://wiki.osdev.org/Exceptions) OSDEV网站有对异常中断作详细的解释

### 中断请求 (IRQ) 或硬件中断

这种类型的中断由芯片组在外部产生，并通过锁定到相关 CPU 的 #INTR 引脚或等效信号来发出信号。属于可屏蔽中断的一种。

> 在最初的32系统开发中, 我们会接触到一个叫做8259A的可编程中断控制器

### 软中断

这类中断通常由用户程序触发, 一般用于 Syscall 系统调用等功能，属于可屏蔽中断的一种

## 如何配置IDT

以下会讲解我们如何用C和汇编来配置我们的中断系统，方便我们在内核中更好的处理它们

### C结构体声明

> 通常, 我们建议你将这些结构体定义在头文件中, 以方便我们在其他地方复用它们

1. 中断门描述符结构体

    ```c
    struct idt_entry_struct {
        uint16_t base_low;
        uint16_t sel;
        uint8_t always0;
        uint8_t flags;
        uint16_t base_high;
    } __attribute__((packed));

    typedef struct idt_entry_struct idt_entry_t;
    ```

2. IDT指针声明

    ```c
    struct idt_ptr_struct {
        uint16_t limit;
        uint32_t base;
    } __attribute__((packed));

    typedef struct idt_ptr_struct idt_ptr_t;
    ```

3. 以及一些函数的预定义

    ```c
    void idt_install(); //IDT初始化

    void idt_use_reg(uint8_t num,uint32_t base); //注册用户程序可触发的中断, 这个在我们以后写到syscall时候要用
    void idt_set_gate(uint8_t num, uint32_t base, uint16_t sel, uint8_t flags); //注册一般中断
    ```

4. 汇编定义的ISR处理函数

    ```c
    #define DECLARE_ISR(i) extern void isr##i();

    DECLARE_ISR(0)
    DECLARE_ISR(1)
    DECLARE_ISR(2)
    DECLARE_ISR(3)
    DECLARE_ISR(4)
    DECLARE_ISR(5)
    DECLARE_ISR(6)
    DECLARE_ISR(7)
    DECLARE_ISR(8)
    DECLARE_ISR(9)
    DECLARE_ISR(10)
    DECLARE_ISR(11)
    DECLARE_ISR(12)
    DECLARE_ISR(13)
    DECLARE_ISR(14)
    DECLARE_ISR(15)
    DECLARE_ISR(16)
    DECLARE_ISR(17)
    DECLARE_ISR(18)
    DECLARE_ISR(19)
    DECLARE_ISR(20)
    DECLARE_ISR(21)
    DECLARE_ISR(22)
    DECLARE_ISR(23)
    DECLARE_ISR(24)
    DECLARE_ISR(25)
    DECLARE_ISR(26)
    DECLARE_ISR(27)
    DECLARE_ISR(28)
    DECLARE_ISR(29)
    DECLARE_ISR(30)
    DECLARE_ISR(31)
    #undef DECLARE_ISR
    #define DECLARE_IRQ(i) extern void irq##i();
    DECLARE_IRQ(0)
    DECLARE_IRQ(1)
    DECLARE_IRQ(2)
    DECLARE_IRQ(3)
    DECLARE_IRQ(4)
    DECLARE_IRQ(5)
    DECLARE_IRQ(6)
    DECLARE_IRQ(7)
    DECLARE_IRQ(8)
    DECLARE_IRQ(9)
    DECLARE_IRQ(10)
    DECLARE_IRQ(11)
    DECLARE_IRQ(12)
    DECLARE_IRQ(13)
    DECLARE_IRQ(14)
    DECLARE_IRQ(15)

    DECLARE_IRQ(80)
    #undef DECLARE_IRQ
    ```

5. 中断触发前的CPU各种寄存器状态以及中断号等

    ```c
    typedef struct registers {
        uint32_t ds; // 我们自己弹入的，当前数据段
        uint32_t edi, esi, ebp, useless, ebx, edx, ecx, eax; // 按pusha顺序
        // 其中esp一项不会被popa还原至对应寄存器中，因为esp寄存器代表的是当前的栈而不是进入中断前的，因此用useless代替
        uint32_t int_no, err_code; // 中断代号是自己push的，err_code有的自带，有的不自带
        uint32_t eip, cs, eflags, esp, ss; // 进入中断时自动压入的
    } registers_t;
    ```

6. 方便内核注册中断处理函数以及一些其他定义

    ```c
    #define IRQ0 32
    #define IRQ1 33
    #define IRQ2 34
    #define IRQ3 35
    #define IRQ4 36
    #define IRQ5 37
    #define IRQ6 38
    #define IRQ7 39
    #define IRQ8 40
    #define IRQ9 41
    #define IRQ10 42
    #define IRQ11 43
    #define IRQ12 44
    #define IRQ13 45
    #define IRQ14 46
    #define IRQ15 47

    typedef void (*isr_t)(registers_t *);

    void register_interrupt_handler(uint8_t n, isr_t handler);
    ```

### C语言的函数实现

> 一般这里的代码我们可以在 `idt.c` 文件中编写

1. 一些全局变量和汇编函数导入

    ```c
    #include "description_table.h" //描述符表的头文件, 你可以自由命名, 但教程这里是将GDT与IDT的头文件合并到一块了
    #include "io.h" //我们之前编写的io.h文件, 用于定义各种IO操作函数
    static isr_t interrupt_handlers[256];
    idt_entry_t idt_entries[256]; // IDT有256个描述符
    idt_ptr_t idt_ptr;

    extern void idt_flush(uint32_t); //从汇编中导入的函数, 用于加载IDT
    ```

2. 函数实现

    ```c
    void idt_use_reg(uint8_t num,uint32_t base){
        idt_entries[num].base_low = base & 0xFFFF;
        idt_entries[num].base_high = (base >> 16) & 0xFFFF; // 拆成低位和高位

        idt_entries[num].sel = 0x08;
        idt_entries[num].always0 = 0;
        idt_entries[num].flags = 0x8E | 0x60;
    }

    void idt_set_gate(uint8_t num, uint32_t base, uint16_t sel, uint8_t flags) {
        idt_entries[num].base_low = base & 0xFFFF;
        idt_entries[num].base_high = (base >> 16) & 0xFFFF; // 拆成低位和高位

        idt_entries[num].sel = sel;
        idt_entries[num].always0 = 0;
        idt_entries[num].flags = flags;
        // flags | 0x60，即可被Ring3调用
    }

    void register_interrupt_handler(uint8_t n, isr_t handler) {
        interrupt_handlers[n] = handler;
    }

    void idt_install() {
        idt_ptr.limit = sizeof(idt_entry_t) * 256 - 1;
        idt_ptr.base = (uint32_t) & idt_entries;

        memset(&idt_entries, 0, sizeof(idt_entry_t) * 256);

        // 初始化PIC
        outb(0x20, 0x11);
        outb(0xA0, 0x11);
        outb(0x21, 0x20);
        outb(0xA1, 0x28);
        outb(0x21, 0x04);
        outb(0xA1, 0x02);
        outb(0x21, 0x01);
        outb(0xA1, 0x01);
        outb(0x21, 0x0);
        outb(0xA1, 0x0);

    #define REGISTER_ISR(id) idt_set_gate(id, (uint32_t) isr##id, 0x08, 0x8E)
        REGISTER_ISR(0);
        REGISTER_ISR(1);
        REGISTER_ISR(2);
        REGISTER_ISR(3);
        REGISTER_ISR(4);
        REGISTER_ISR(5);
        REGISTER_ISR(6);
        REGISTER_ISR(7);
        REGISTER_ISR(8);
        REGISTER_ISR(9);
        REGISTER_ISR(10);
        REGISTER_ISR(11);
        REGISTER_ISR(12);
        REGISTER_ISR(13);
        REGISTER_ISR(14);
        REGISTER_ISR(15);
        REGISTER_ISR(16);
        REGISTER_ISR(17);
        REGISTER_ISR(18);
        REGISTER_ISR(19);
        REGISTER_ISR(20);
        REGISTER_ISR(21);
        REGISTER_ISR(22);
        REGISTER_ISR(23);
        REGISTER_ISR(24);
        REGISTER_ISR(25);
        REGISTER_ISR(26);
        REGISTER_ISR(27);
        REGISTER_ISR(28);
        REGISTER_ISR(29);
        REGISTER_ISR(30);
    #undef REGISTER_ISR
        idt_use_reg(31, (uint32_t) isr31);
    #define REGISTER_IRQ(id, irq_id) idt_set_gate(id, (uint32_t) irq##irq_id, 0x08, 0x8E)
        REGISTER_IRQ(32, 0);
        REGISTER_IRQ(33, 1);
        REGISTER_IRQ(34, 2);
        REGISTER_IRQ(35, 3);
        REGISTER_IRQ(36, 4);
        REGISTER_IRQ(37, 5);
        REGISTER_IRQ(38, 6);
        REGISTER_IRQ(39, 7);
        REGISTER_IRQ(40, 8);
        REGISTER_IRQ(41, 9);
        REGISTER_IRQ(42, 10);
        REGISTER_IRQ(43, 11);
        REGISTER_IRQ(44, 12);
        REGISTER_IRQ(45, 13);
        REGISTER_IRQ(46, 14);
        REGISTER_IRQ(47, 15);
    #undef REGISTER_IRQ
        idt_flush((uint32_t) & idt_ptr);
    }
    ```

3. C语言中的中断预处理函数, 由汇编调用

    ```c
    void isr_handler(registers_t regs) {
        //printf("\n[Kernel]: received interrupt: %d\n",regs.int_no); 这里你可以输出一下异常中断号,也可以不输出

        if (interrupt_handlers[regs.int_no]) {
            isr_t handler = interrupt_handlers[regs.int_no]; // 有自定义处理程序，调用之
            handler(&regs); // 传入寄存器
        }
    }

    void irq_handler(registers_t regs) {
        if(regs.int_no != 0x2b) {
            if (regs.int_no >= 40) outb(0xA0, 0x20); // 中断号 >= 40，来自从片，发送EOI给从片
            outb(0x20, 0x20); // 发送EOI给主片
        }
        if (interrupt_handlers[regs.int_no]) {
            isr_t handler = interrupt_handlers[regs.int_no]; // 有自定义处理程序，调用之
            handler(&regs); // 传入寄存器
        }
    }
    ```

什么? 你问我汇编部分呢, 请看源码文件, 我们使用nasm语法编写

[**interrupt.asm**](/教程/示例代码/项目/mdrOS/interrupt.asm)

到这里, 我们内核就基本具备了对中断的处理能力, 以后编写程序驱动, 异常处理都可以通过我们已经编写好的函数去注册
