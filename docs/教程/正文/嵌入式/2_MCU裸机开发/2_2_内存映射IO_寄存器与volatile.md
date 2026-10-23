# 2.2 内存映射 I/O、寄存器与 volatile

> 本节主要介绍操作 **寄存器** 来操作底层硬件

## 1. 寄存器（Register）概念

寄存器 (Register) 本来指 CPU 内部的高速存储单元，用于临时存放数据和指令，访问速度比内存快得多。例如 Cortex-M 内核有通用寄存器 R0～R12、栈指针 SP、链接寄存器 LR 和程序计数器 PC。这些内核寄存器由编译器分配和使用，C 程序不能直接访问。

MCU 开发中所说的“操作寄存器”，通常指另一类寄存器，即外设寄存器。GPIO、UART、定时器等片上外设各有一组寄存器，分别用于配置工作方式、读取状态和收发数据。

这些外设寄存器与 Flash、SRAM 编排在同一个 4 GB 的地址空间中，每个寄存器占据一个固定的地址，CPU 用普通的读写指令访问它们，这种方式称为内存映射 I/O (Memory-Mapped I/O, MMIO)。在 STM32F103 上，外设区从地址 `0x40000000` 开始；GPIOA 的寄存器组从 `0x40010800` 开始，`GPIOA_ODR` 位于偏移 `0x0C` 处，即地址 `0x4001080C`。每个寄存器的地址、各位的含义和复位值，都要查阅芯片的参考手册（STM32F103 对应的是 RM0008）。

## 2.

## 3. 习题

<Exercise id="62215" :d="1" :w="2">

下面五个寄存器都在 RM0008 里有明确的位置。按「外设基地址 + 寄存器偏移」算出绝对地址，并写出对应的一行 C 代码（用强制转换的 `volatile` 指针，不要用设备头文件）：

| 寄存器 | 基地址 | 偏移 |
| --- | --- | --- |
| GPIOC_ODR | 0x4001 1000 | 0x0C |
| GPIOB_CRL | 0x4001 0C00 | 0x00 |
| USART1_SR | 0x4001 3800 | 0x00 |
| RCC_APB2ENR | 0x4002 1000 | 0x18 |
| FLASH_ACR | 0x4002 2000 | 0x00 |

(1) 填出五个绝对地址。

(2) 写出把 PC13 对应的 ODR 位置 1、且不影响同端口其他引脚的两种写法：一种是读-改-写，一种是用 BSRR（偏移 0x10，低 16 位写 1 置位）。比较两者在「同一端口还有别的引脚被中断修改」时的表现。

(3) 算出 GPIOC_ODR 中第 13 位的清零掩码与置位掩码，写成十六进制。

(4) 这些寄存器全在 `0x4000_0000` 起的同一段地址里，CPU 访问它们用的指令与访问 SRAM 有什么不同？为什么说「内存映射」这四个字就是字面意思。

</Exercise>

<Exercise id="62216" :d="2" :w="3">

`volatile` 的作用在汇编层面看得最清楚。下面两个版本只差一个限定符：

```c
/* nv.c */
#include <stdint.h>
#define SR (*(uint32_t *)0x40013800u)

int wait_txe(void) {
    while ((SR & (1u << 7)) == 0) {
    }
    return 0;
}
```

```c
/* v.c */
#include <stdint.h>
#define SR (*(volatile uint32_t *)0x40013800u)

int wait_txe(void) {
    while ((SR & (1u << 7)) == 0) {
    }
    return 0;
}
```

(1) 用 `arm-none-eabi-gcc -std=c11 -mcpu=cortex-m3 -mthumb -O2 -S` 分别生成汇编，把 `wait_txe` 的函数体贴出来。说明不带 `volatile` 的版本为什么在第一次读到标志未置位后就再也读不到寄存器了。

(2) 再试一次「连续两次写同一个地址」：

```c
void send_ab(void) {
    *(uint32_t *)0x40013804u = 'A';
    *(uint32_t *)0x40013804u = 'B';
}
```

对比加不加 `volatile` 生成的指令条数，说明如果目标是串口数据寄存器，丢掉的那次写会造成什么后果。

(3) 把 (1) 里的等待改成带超时的形式：最多循环 100000 次，超时返回 -1。用 `volatile` 的版本写出函数，并说明计数对象本身要不要加 `volatile`。

(4) `volatile` 能不能保证 `SR` 的读取是原子的？结合 Cortex-M3 对 32 位对齐访问的处理回答。

</Exercise>

<Exercise id="62217" :d="3" :w="3">

设备头文件用结构体描述一组寄存器。自己写一份最小的 GPIO 结构体，不包含 ST 的头文件：

```c
typedef struct {
    volatile uint32_t CRL;
    volatile uint32_t CRH;
    volatile uint32_t IDR;
    volatile uint32_t ODR;
    volatile uint32_t BSRR;
    volatile uint32_t BRR;
    volatile uint32_t LCKR;
} gpio_regs_t;

#define GPIOA ((gpio_regs_t *)0x40010800u)
```

(1) 用 C11 的 `_Static_assert` 与 `offsetof` 检查七个成员的偏移：CRL 0x00、CRH 0x04、IDR 0x08、ODR 0x0C、BSRR 0x10、BRR 0x14、LCKR 0x18；再加一条 `sizeof(gpio_regs_t) == 0x1C`。编译一次确认全部成立。

(2) 写两个函数，一个用 `GPIOA->ODR |= 1u << 5;`，一个用 `*(volatile uint32_t *)0x4001080Cu |= 1u << 5;`。用 `arm-none-eabi-gcc -O2 -S` 编译并比较两者的指令序列，说明结构体成员访问最终变成了哪几条指令。

(3) 把 `IDR` 与 `ODR` 两个成员的位置写反，重新编译。贴出 `_Static_assert` 的报错信息，解释编译器为什么能发现，以及如果只写了 `sizeof` 那一条检查、能不能发现。

(4) 如果把 `volatile` 从结构体成员上去掉、只保留在指针上（`#define GPIOA ((gpio_regs_t * volatile)0x40010800u)`），编译能否通过？写一个 `while ((GPIOA->IDR & 1u) == 0) {}` 的等待循环，看反汇编里那条 `ldr` 在不在循环内，说明限定符加在指针上与加在成员上的区别。

</Exercise>

<Exercise id="62218" :d="4" :w="4">

主循环与中断同时修改 `GPIOC->ODR` 时，读-改-写会丢位。下面是一个能在主机上复现的模型：

```c
#include <stdint.h>
#include <stdio.h>

static uint32_t g_odr;

static void isr_sets_bit3(void) { g_odr |= 1u << 3; }

int main(void) {
    uint32_t tmp = g_odr;
    isr_sets_bit3();   /* 模拟中断恰好发生在读与写之间 */
    tmp |= 1u << 5;
    g_odr = tmp;
    printf("ODR = 0x%08X, bit3 %s\n", g_odr,
           (g_odr & (1u << 3)) != 0 ? "kept" : "lost");
    return 0;
}
```

(1) 编译运行，写出输出，并画出「主循环读、中断写、主循环写」三个时刻的时间线，标出 bit 3 在哪一步被覆盖。

(2) 换成「写 1 置位」的寄存器模型：C 代码写 `g_odr |= (1u << 5);`，但硬件语义是「只有写 1 的位被置位，写 0 的位不变」。在主机上把 `g_odr` 换成一个结构体，用 `write_w1s(uint32_t value)` 模拟这一行为，让同样的时序不再丢 bit 3，输出 `kept`。

(3) 如果没有这种寄存器，另一个办法是在读-改-写外面关中断。写出用 CMSIS 的 `__disable_irq()` / `__enable_irq()` 包住的版本（这两个函数在 2.5 节讲，这里直接调用即可；也可以写成 `__asm volatile ("cpsid i")`），说明它保护的范围应该从哪里到哪里，为什么不能只包住写的那一句。

(4) 三种做法（W1S 寄存器、关中断、先把要写的值攒起来再一次性写）各适合什么场合？给出你在「主循环与中断都要改同一个端口」时的选择与理由。

</Exercise>
