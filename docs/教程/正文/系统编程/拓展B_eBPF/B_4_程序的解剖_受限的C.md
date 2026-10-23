# B.4 程序的解剖：受限的 C

[上一节](/教程/正文/系统编程/拓展B_eBPF/B_3_第一个eBPF程序)的 hello 程序只有十几行，编译、加载、运行一次就过去了。可是当你要改它的时候，问题会一个接一个地来：`SEC("tp/syscalls/sys_enter_write")` 这串字符串到底做了什么，为什么函数名不重要而节名重要；`my_pid` 只是一个普通的 `int`，用户态凭什么能写它；`bpf_printk` 后面为什么不是 `printf`；把一个 1 KB 的数组放到栈上，编译器为什么直接拒绝。

这些问题的答案都在同一个地方：`.bpf.c` 编译出来的目标文件。这一页把 hello.bpf.o 拆开看一遍——先看它由哪些节组成，再看 BPF 虚拟机长什么样、机器码怎么读，然后用一个一百多行的解释器亲手执行几条指令，最后把「C 语言在 BPF 里有哪些写法不能用」整理成可以直接查的表。读完这一页，你应该能拿到任意一个 `.bpf.o` 就大致看懂它做了什么，也能在写内核态程序时避开那些让编译器和验证器同时报错的写法。

本页用到的完整程序放在 `code/系统编程/拓展B_eBPF/B_4_程序的解剖_受限的C/`，同一目录下的 `mini_vm.c` 可以在本机直接用 gcc 编译运行。

## 1. 一个 .bpf.c 文件由什么组成

先把 hello 的源码完整贴出来。它和[上一节](/教程/正文/系统编程/拓展B_eBPF/B_3_第一个eBPF程序)的版本一致，只是把一个 `int` 改成了供本页解剖的对象。

```c
#include "vmlinux.h"
#include <bpf/bpf_helpers.h>

int my_pid = 0;

SEC("tp/syscalls/sys_enter_write")
int handle_write(void *ctx)
{
    __u64 pid_tgid = bpf_get_current_pid_tgid();
    int pid = pid_tgid >> 32;

    if (pid != my_pid) {
        return 0;
    }
    bpf_printk("BPF triggered from PID %d.\n", pid);
    return 0;
}

char LICENSE[] SEC("license") = "Dual BSD/GPL";
```

四个部分：两个 `#include`、一个全局对象 `my_pid`、一个带 `SEC` 标记的函数、一行许可证。下面逐个说清楚它们在目标文件里变成什么。

### 1.1 SEC 宏把函数放进一个以节名命名的段

`SEC` 是 `bpf_helpers.h` 里的宏，不是编译器关键字。它的定义在 libbpf 源码里长这样：

```c
#if __GNUC__ && !__clang__
#define SEC(name) __attribute__((section(name), used))
#else
#define SEC(name) \
	_Pragma("GCC diagnostic push")					    \
	_Pragma("GCC diagnostic ignored \"-Wignored-attributes\"")	    \
	__attribute__((section(name), used))				    \
	_Pragma("GCC diagnostic pop")					    \
#endif
```

两个分支的实际内容一样：`__attribute__((section(name), used))`。[`section` 属性](/教程/正文/语法和标准库/17_预处理与翻译/17_6_pragma)把对象或函数放进指定的段，`used` 阻止编译器因为「没人调用」而把它删掉。clang 分支多出来的两句 `_Pragma` 只是压掉 `-Wignored-attributes` 警告，因为往某些段里放东西时 clang 会抱怨属性被忽略。

所以 `SEC("tp/syscalls/sys_enter_write")` 展开之后，`handle_write` 这个函数就落在名为 `tp/syscalls/sys_enter_write` 的段里。函数名本身对 libbpf 没有意义——它在骨架里被用作程序名，仅此而已。决定程序类型和附加方式的是节名，规则见 [B.10](/教程/正文/系统编程/拓展B_eBPF/B_10_程序类型与附加类型)。

一个文件放多个程序，就是写多个带不同 `SEC` 的函数。每个函数各自落进自己的段，编译出来是一份目标文件里的多个可执行段：

```c
SEC("tp/syscalls/sys_enter_write")
int on_write(void *ctx) { return 0; }

SEC("tp/syscalls/sys_enter_read")
int on_read(void *ctx) { return 0; }
```

用 `llvm-objdump -h` 看，会同时出现 `tp/syscalls/sys_enter_write` 和 `tp/syscalls/sys_enter_read` 两段，长度各 0x10 字节，也就是两条指令。

### 1.2 license 节决定能用哪些辅助函数

`char LICENSE[] SEC("license") = "Dual BSD/GPL";` 这一行看起来是仪式性的，其实 libbpf 会真的去读它。在 libbpf 的源码里，初始化对象时按名字找节：

```c
if (strcmp(name, "license") == 0) {
	err = bpf_object__init_license(obj, data->d_buf, data->d_size);
```

读到的字符串随后在 `bpf_prog_load` 时交给内核。内核按加载内核模块同样的规则检查许可证：一部分辅助函数带 `GPL` 标记，只对 GPL 兼容的程序开放；不兼容的程序调用它们会被验证器拒绝（报错原文见 [B.9](/教程/正文/系统编程/拓展B_eBPF/B_9_验证器)）。`"Dual BSD/GPL"`、`"GPL"`、`"GPL v2"` 都算兼容；`"Proprietary"` 不算。

这里有一个容易踩的坑。**符号名不能和节名相同**：ELF 里每个节会占一个同名的局部符号，同名的全局对象再进来就撞车，clang 直接报错：

```text
$ clang -target bpf -O2 -g -c hello.bpf.c -o hello.bpf.o
error: symbol 'license' is already defined
error: license changed binding to STB_GLOBAL
error: symbol 'license' is already defined
3 errors generated.
```

所以要么写大写 `char LICENSE[] SEC("license")`，要么写 `char _license[] SEC("license")`。libbpf 只认节名 `license`，不认符号名，这两种写法效果相同。上游的 libbpf-bootstrap 和 libbpf-tools 都统一用 `char LICENSE[] SEC("license")`，本章照此写法。

### 1.3 全局对象落在 .bss、.data 或 .rodata

`my_pid` 没有加任何属性，看起来就是一个普通的全局对象，但它和用户态程序里的全局对象待遇不同：BPF 程序没有数据段可以随便读写，libbpf 会把这些对象所在的节整个变成一张 map，加载时把内容搬进去，运行时通过 map 访问。三种节对应三种存储：

| 源码写法 | 落在 | 初值来自 |
| --- | --- | --- |
| `int my_pid = 0;` | `.bss` | 全零，不占文件空间 |
| `int hit_count = 7;` | `.data` | 文件里的初值 |
| `const volatile int target = 42;` | `.rodata` | 文件里的初值，加载后只读 |

用四个全局对象编译一次，`llvm-readelf -S` 里能同时看到这三段：

```text
  [ 5] .bss              NOBITS          0000000000000000 0000a0 000004 00  WA  0   0  4
  [ 6] .data             PROGBITS        0000000000000000 0000a0 000004 00  WA  0   0  4
  [ 7] .rodata           PROGBITS        0000000000000000 0000a4 000004 00  A   0   0  4
  [ 8] license           PROGBITS        0000000000000000 0000a8 000004 00  WA  0   0  1
```

`const volatile` 里的 `volatile` 不是可有可无的修饰。没有它，编译器会把 42 当成常量直接写进指令，用户态再改也改不动；加上之后，每次读都真的从 `.rodata` 这张 map 里取。这个组合的用法（在 open 之后、load 之前写入）在 [B.6](/教程/正文/系统编程/拓展B_eBPF/B_6_Map与数据交换) 展开。

::: concept ebpf-global-data-maps title="全局对象就是单元素数组 map"

`.bss`、`.data`、`.rodata` 三个节里的对象，由 libbpf 在加载时各建成一张单元素数组 map，并映射到用户态。骨架里的 `skel->bss->my_pid` 就是这张 map 的第零个元素。所以内核态程序读写「全局对象」时，实际发生的是一次 map 访问。

:::

### 1.4 目标文件的节列表

把 hello.bpf.c 编译成目标文件，再用 `llvm-objdump -h` 列出各节。去掉调试相关的节，剩下的就是程序真正用到的部分：

::: terminal title="bash - hello.bpf.o 的节"

```text
$ clang -g -O2 -target bpf -mcpu=v3 -D__TARGET_ARCH_x86 -I. -c hello.bpf.c -o hello.bpf.o
$ llvm-objdump -h hello.bpf.o | grep -Ev "debug|addrsig|strtab|symtab|eh_frame"

hello.bpf.o:	file format elf64-bpf

Sections:
Idx Name                            Size     VMA              Type
  0                                 00000000 0000000000000000 
  2 .text                           00000000 0000000000000000 TEXT
  3 tp/syscalls/sys_enter_write     00000068 0000000000000000 TEXT
  4 .reltp/syscalls/sys_enter_write 00000020 0000000000000000 
  5 .bss                            00000004 0000000000000000 BSS
  6 .rodata                         0000001c 0000000000000000 DATA
  7 license                         0000000d 0000000000000000 DATA
 13 .BTF                            00000288 0000000000000000 
 14 .rel.BTF                        00000030 0000000000000000 
 15 .BTF.ext                        000000a0 0000000000000000 
 16 .rel.BTF.ext                    00000070 0000000000000000 
```

:::

逐条对上：

- `tp/syscalls/sys_enter_write` 是程序本体，0x68 字节。一共 11 条指令，其中两条是 64 位立即数加载（编号 2 和 6），各占两个 8 字节槽位，合起来 13 个槽位，正好 104 字节。
- 紧随其后的 `.reltp/syscalls/sys_enter_write` 是重定位表，记录指令里哪些地方要由加载程序改写。两条记录分别指向 `my_pid` 和 `.rodata`。
- `.bss` 4 字节，是 `my_pid`；`.rodata` 0x1c 字节，是那句格式串，长度 28 与源码里的字符串一致（27 个字符加结尾的 0）。
- `license` 13 字节，是 `"Dual BSD/GPL"` 加结尾 0（12 个字符加结尾 0）。
- `.BTF` 是类型信息，`.BTF.ext` 是函数与行号信息以及 CO-RE 重定位记录，`.rel.BTF`、`.rel.BTF.ext` 是前两者自己的重定位。原理见 [B.8](/教程/正文/系统编程/拓展B_eBPF/B_8_libbpf_BTF与CO_RE)。

`-g` 会顺带产生 `.debug_info`、`.debug_line` 等一串节，它们只帮助调试和打印源码行，不影响加载。某些编译器驱动还会额外生成 `.eh_frame` 和 `.llvm_addrsig`；这两个节 libbpf 不认识，用 `bpftool gen skeleton` 时会打印两行提示后跳过：

```text
libbpf: elf: skipping unrecognized data section(17) .eh_frame
libbpf: elf: skipping relo section(18) .rel.eh_frame for section(17) .eh_frame
```

这两行不是错误。`.eh_frame` 是编译器驱动附加的栈展开表，对 BPF 程序没有意义；发行版自带的 clang 通常不生成它，用某些驱动（例如 Zig 自带的 clang）时会多出来，忽略即可。

BTF 里能直接读出结构：`VAR 'my_pid'`、`VAR 'LICENSE'`、`DATASEC '.bss'`、`DATASEC '.rodata'`、`DATASEC 'license'` 和 `FUNC 'handle_write'`。用 `bpftool btf dump file hello.bpf.o` 可以看到，`.bss` 只有一项，偏移 0、长度 4，正是 `my_pid`；`license` 节里是长度为 13 的 `LICENSE`。字符串也在这里——`handle_write.____fmt` 是那个在函数体内定义的 `static const char` 数组，和 `my_pid` 一样是节里的一个对象。

## 2. BPF 虚拟机

BPF 程序不是直接编译成 x86-64 或 ARM 指令的。clang 先把 C 编译成 BPF 字节码，内核的验证器检查一遍，再由即时编译器（just-in-time compilation，JIT）翻译成本机指令。所以反汇编里看到的是一套独立的指令集，它比任何真实 CPU 都简单。

### 2.1 寄存器与栈

虚拟机有 11 个 64 位寄存器，编号 R0 到 R10，另有 32 位的子寄存器可以只操作低半部分（写作 `w0`、`w1` 这样）。寄存器分工和调用约定：

| 寄存器 | 用途 |
| --- | --- |
| R0 | 函数返回值；程序结束时是程序的返回值 |
| R1～R5 | 调用辅助函数或子程序时的参数，最多五个 |
| R6～R9 | 被调用者保存，跨调用保持不变 |
| R10 | 只读的帧指针，指向当前栈帧的顶部 |

程序入口处，R1 指向上下文（context，代码里写 ctx），不同程序类型的上下文结构不同，见 [B.5](/教程/正文/系统编程/拓展B_eBPF/B_5_辅助函数与上下文)。

<Quiz
  :options="[
    { label: '这次事件对应的上下文指针', value: 'A' },
    { label: '调用者的 PID', value: 'B' },
    { label: '程序自己的指令地址', value: 'C' },
    { label: 'map 的地址', value: 'D' }
  ]"
  answer="A"
>

<template #question>

BPF 程序开始执行时，R1 里放的是什么？

</template>

正确答案是 **A**。内核在触发程序时把上下文指针放进 R1；不同程序类型的上下文结构不同，tracepoint 是跟踪记录的原始数据，XDP 是 `xdp_md`。要别的信息得调用辅助函数去取。

</Quiz>

::: concept ebpf-vm-registers title="BPF 的寄存器与调用约定"

BPF 虚拟机有 11 个 64 位寄存器。R0 存放返回值；R1～R5 传递参数，调用返回后内容即失效；R6～R9 由被调用者保存；R10 是只读帧指针，程序不能写它。参数只通过寄存器传递，不通过栈。

:::

每个程序有自己的栈，大小 512 字节，栈向低地址增长，所以局部对象都写成 R10 减去一个偏移，例如 `*(u64 *)(r10 - 0x8)`。

::: concept ebpf-vm-stack title="BPF 栈的大小"

每个 BPF 程序（以及每个子程序）的栈帧是 512 字节。栈上放不下的大结构要移到 map 或者环形缓冲区的预留空间里。栈的大小在编译期就检查，超过时 clang 直接报错。

:::

### 2.2 指令格式

每条指令 8 字节，字段布局如下：

| 字段 | 宽度 | 含义 |
| --- | --- | --- |
| opcode | 8 位 | 操作码，最低 3 位决定指令类别 |
| dst_reg | 4 位 | 目的寄存器编号 |
| src_reg | 4 位 | 源寄存器编号，或者伪指令的子类型 |
| offset | 16 位 | 有符号偏移，用于跳转和访存 |
| imm | 32 位 | 有符号立即数 |

8 字节正好是一个指令槽。有少数指令要带 64 位立即数，占两个槽位（16 字节），第二个槽位里放立即数的高 32 位。反汇编里这种指令显示为 `r1 = 0x0 ll`，后面的 `ll` 就是「long long」，提示它占了两个槽位——所以紧接着的指令编号会跳过一个数字。

opcode 的最低 3 位是类别：

| 类别 | 值 | 作用 |
| --- | --- | --- |
| LD | 0x0 | 特殊加载，包括 64 位立即数和 map 地址 |
| LDX | 0x1 | 从内存读到寄存器 |
| ST | 0x2 | 把立即数写入内存 |
| STX | 0x3 | 把寄存器的值写入内存 |
| ALU | 0x4 | 32 位算术运算 |
| JMP | 0x5 | 64 位跳转与调用 |
| JMP32 | 0x6 | 32 位跳转 |
| ALU64 | 0x7 | 64 位算术运算 |

`call` 属于 JMP 类，opcode 0x85。它的 `src_reg` 区分三种调用：0 表示按编号调用辅助函数（编号在 imm 里），1 表示调用同一目标文件里的子程序（imm 是相对偏移），2 表示按 BTF 编号调用。

### 2.3 逐条读 hello 的反汇编

```text
$ llvm-objdump -dSr hello.bpf.o

0000000000000000 <handle_write>:
;     __u64 pid_tgid = bpf_get_current_pid_tgid();
       0:	85 00 00 00 0e 00 00 00	call 0xe
;     int pid = pid_tgid >> 32;
       1:	77 00 00 00 20 00 00 00	r0 >>= 0x20
;     if (pid != my_pid) {
       2:	18 01 00 00 00 00 00 00 00 00 00 00 00 00 00 00	r1 = 0x0 ll
		0000000000000010:  R_BPF_64_64	my_pid
       4:	61 11 00 00 00 00 00 00	r1 = *(u32 *)(r1 + 0x0)
       5:	5e 01 05 00 00 00 00 00	if w1 != w0 goto +0x5 <handle_write+0x58>
;     bpf_printk("BPF triggered from PID %d.\n", pid);
       6:	18 01 00 00 00 00 00 00 00 00 00 00 00 00 00 00	r1 = 0x0 ll
		0000000000000030:  R_BPF_64_64	.rodata
       8:	b4 02 00 00 1c 00 00 00	w2 = 0x1c
       9:	bc 03 00 00 00 00 00 00	w3 = w0
      10:	85 00 00 00 06 00 00 00	call 0x6
; }
      11:	b4 00 00 00 00 00 00 00	w0 = 0x0
      12:	95 00 00 00 00 00 00 00	exit
```

指令 0 的字节是 `85 00 00 00 0e 00 00 00`：opcode 0x85 是 `call`，`regs` 字节 0x00 说明 `src_reg` 为 0，也就是按编号调用辅助函数；imm 是 0x0e，即 14。14 号辅助函数是 `bpf_get_current_pid_tgid`，可以在 `bpf_helper_defs.h` 里查到：

```c
static __u64 (* const bpf_get_current_pid_tgid)(void) = (void *) 14;
```

这条声明说明了一件重要的事：辅助函数在 C 里看起来像函数指针，在字节码里就是一个立即数编号。调用约定就是寄存器那五个。

指令 1 把 R0 右移 32 位，拿到高 32 位的 tgid，也就是用户态说的 PID。

指令 2 是 `r1 = 0x0 ll`，后面跟着一行重定位记录 `R_BPF_64_64 my_pid`。目标文件里立即数是 0，因为编译器不知道这张 map 在加载后会落在哪个地址；libbpf 加载时创建 map，把它的地址填进这两条指令的 64 位立即数字段。指令 3 被这条 16 字节的指令占掉了，所以编号从 2 直接跳到 4。

指令 4 从 R1 指向的地址读 4 字节，这就是读 `my_pid`：`.bss` 被建成单元素数组 map，读它的第一个元素就是「解引用 R1 加偏移 0」。

指令 5 比较 `w1` 和 `w0`——32 位的比较，因为两边都是 `int`。不等就跳过 5 条指令，跳到 11，也就是那条 `return 0`。

指令 6 到 10 是 `bpf_printk`。宏展开后是 `bpf_trace_printk(fmt, sizeof(fmt), pid)`：R1 指向格式串（另一条 `R_BPF_64_64` 重定位，指向 `.rodata`），R2 是 0x1c 也就是 28，正好是那句格式串的长度；R3 是 pid；然后 `call 0x6`，6 号辅助函数就是 `bpf_trace_printk`。

指令 11 和 12 把 R0 置零并退出。R0 是返回值，跟踪类程序的返回值通常没人看，但写上 0 是个好习惯。

`bpf_printk` 的参数超过三个时，宏会改调 `bpf_trace_vprintk`（177 号），把参数先摆到栈上再整块传过去。这就是为什么参数一多，反汇编里会突然出现一串 `*(u64 *)(r10 - 0x8)` 这样的写栈指令——那不是程序自己在用栈，是宏展开的副产物。[B.5](/教程/正文/系统编程/拓展B_eBPF/B_5_辅助函数与上下文) 会用到这个事实。

::: generate ebpf-decode-hello-disasm kind=explanation concepts=ebpf-vm-registers

面向已经会读 x86 汇编、但没接触过 BPF 指令集的读者，按指令编号逐条解释 hello.bpf.o 的反汇编：先讲 8 字节指令的五个字段怎么从十六进制字节里读出来，再讲 `call 0xe` 和 `call 0x6` 里的立即数就是辅助函数编号，最后讲两条 `ll` 指令为什么占两个槽位、重定位记录在加载时被改写成什么。不要重复正文已经写过的寄存器表，重点放在「怎么从字节读到含义」。

:::

## 3. 用 C 执行几条 BPF 指令

反汇编读懂了，但「虚拟机」三个字还是抽象的。写一个袖珍解释器，把 clang 为下面这个函数生成的字节码跑一遍，指令集就不再是纸上的东西。

```c
long calc(long x)
{
    return x > 10 ? x * 3 : x + 1;
}
```

用 `-O2` 编译，反汇编只有六条指令：

```text
$ llvm-objdump -d calc.bpf.o

0000000000000000 <calc>:
       0:	bf 10 00 00 00 00 00 00	r0 = r1
       1:	27 00 00 00 03 00 00 00	r0 *= 0x3
       2:	65 01 02 00 0a 00 00 00	if r1 s> 0xa goto +0x2 <calc+0x28>
       3:	07 01 00 00 01 00 00 00	r1 += 0x1
       4:	bf 10 00 00 00 00 00 00	r0 = r1
       5:	95 00 00 00 00 00 00 00	exit
```

`mini_vm.c` 只实现这几条指令用到的操作，加一个解码函数和主循环。核心部分是解码和分派：

```c
struct insn {
    uint8_t opcode;
    uint8_t regs;
    int16_t off;
    int32_t imm;
};

static struct insn decode(const uint8_t *p) {
    struct insn in;

    in.opcode = p[0];
    in.regs = p[1];
    in.off = (int16_t)((uint16_t)p[2] | ((uint16_t)p[3] << 8));
    in.imm = (int32_t)((uint32_t)p[4] | ((uint32_t)p[5] << 8) |
                       ((uint32_t)p[6] << 16) | ((uint32_t)p[7] << 24));
    return in;
}
```

分派部分按 opcode 取值，`dst` 和 `src` 从 `regs` 字节的两半拆出来：

```c
unsigned dst = in.regs & 0x0f;
unsigned src = in.regs >> 4;

switch (in.opcode) {
case OP_MOV_REG:            /* 0xbf */
    regs[dst] = regs[src];
    pc++;
    break;
case OP_MUL_IMM:            /* 0x27 */
    regs[dst] *= (uint64_t)(int64_t)in.imm;
    pc++;
    break;
case OP_JSGT_IMM:           /* 0x65 */
    if ((int64_t)regs[dst] > (int64_t)in.imm) {
        pc = (size_t)((int64_t)pc + 1 + in.off);
    } else {
        pc++;
    }
    break;
case OP_EXIT:               /* 0x95 */
    *result = regs[0];
    return 0;
}
```

跳转的语义要特别注意：`off` 是**相对于下一条指令**的偏移，所以算新位置时要先 `pc + 1` 再加 `off`。这和 x86 的相对跳转一样。

指令表直接照抄 objdump 的输出，一条一条写成字节数组：

```c
static const uint8_t code[] = {
    0xbf, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* r0 = r1 */
    0x27, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, /* r0 *= 3 */
    0x65, 0x01, 0x02, 0x00, 0x0a, 0x00, 0x00, 0x00, /* if r1 s> 10 */
    0x07, 0x01, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, /* r1 += 1 */
    0xbf, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* r0 = r1 */
    0x95, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* exit */
};
```

完整程序见 `code/系统编程/拓展B_eBPF/B_4_程序的解剖_受限的C/mini_vm.c`，用 `gcc -std=c17 -Wall -Wextra mini_vm.c -o mini_vm` 编译。它做的事是把同一组输入分别交给解释器和本机编译的 C 函数，比较结果：

::: terminal title="bash - mini_vm"

```text
$ gcc -std=c17 -Wall -Wextra -O2 mini_vm.c -o mini_vm
$ ./mini_vm
x =   0   BPF:   1   C:   1
x =   1   BPF:   2   C:   2
x =   5   BPF:   6   C:   6
x =  10   BPF:  11   C:  11
x =  11   BPF:  33   C:  33
x =  20   BPF:  60   C:  60
x = 100   BPF: 300   C: 300

x = 20 的执行过程：
   0: opcode 0xbf dst r0 src r1 off +0 imm 0
      r0 = 20  r1 = 20
   1: opcode 0x27 dst r0 src r0 off +0 imm 3
      r0 = 60  r1 = 20
   2: opcode 0x65 dst r1 src r0 off +2 imm 10
      r0 = 60  r1 = 20
   5: opcode 0x95 dst r0 src r0 off +0 imm 0
      r0 = 60  r1 = 20

  结果 60
```

:::

走一遍 x = 20 的过程：R0 先复制 R1（20），乘 3 得到 60；然后比较 R1 和 10，20 大于 10，跳转生效，从指令 3 跳到 5，直接退出。如果 x 是 5，比较不成立，落到指令 3 把 R1 加 1，指令 4 再把它搬回 R0，结果是 6。

这个解释器只用来理解指令格式。真正的内核还有验证器（保证程序不会越界、一定会结束）和 JIT（把字节码翻译成本机指令，之后不再逐条解释）。这两部分分别在 [B.9](/教程/正文/系统编程/拓展B_eBPF/B_9_验证器) 和本章开头讲过。

## 4. C 语言的限制

写 `.bpf.c` 用的是 C 的语法，但目标平台不是完整的操作系统进程。没有 libc，没有浮点运算的硬件支持，栈只有 512 字节，不能随便解引用指针。这些限制大多来自同一个原因：程序要在内核里、在任意上下文（包括中断）中安全地跑完，而且加载之前必须能被静态证明。

### 4.1 没有 libc，也没有浮点

`printf`、`malloc`、`strcpy` 这些函数一个都没有。想输出用辅助函数 `bpf_printk` 或 `bpf_trace_printk`，想读内存用 `bpf_probe_read_*`，想操作字符串用 `bpf_probe_read_kernel_str` 之类，具体清单见 [B.5](/教程/正文/系统编程/拓展B_eBPF/B_5_辅助函数与上下文)。

编译器内置的 `__builtin_memcpy` 和 `__builtin_memset` 可以用，前提是长度在编译期是常量——这样 clang 会把它展开成几条访存指令，不产生函数调用：

```c
char buf[16];
int n = bpf_get_smp_processor_id();

__builtin_memset(buf, 0, sizeof(buf));
buf[n & 15] = 1;
return buf[0] + n;
```

清 16 字节被展开成两条 8 字节写入：

```text
       1:	b7 01 00 00 00 00 00 00	r1 = 0x0
       2:	7b 1a f0 ff 00 00 00 00	*(u64 *)(r10 - 0x10) = r1
       3:	7b 1a f8 ff 00 00 00 00	*(u64 *)(r10 - 0x8) = r1
```

没有 `call`。长度要是运行期才知道，编译器就没法展开，直接报错：

```c
int f(void *ctx, int n) {
    char buf[16];

    __builtin_memset(buf, 0, n);
    return buf[0];
}
```

```text
$ clang -target bpf -O2 -g -c memset_rt.c -o memset_rt.o
memset_rt.c:4:5: error: A call to built-in function 'memset' is not supported.
    4 |     __builtin_memset(buf, 0, n);
      |     ^
1 error generated.
```

浮点更直接。BPF 指令集里没有浮点运算，`double` 的加减乘除都要靠软件模拟，而软件模拟的例程（`__adddf3` 之类）不在 BPF 目标里：

```text
$ clang -target bpf -O2 -g -c float.bpf.c -o float.bpf.o
float.bpf.c:10:11: error: A call to built-in function '__adddf3' is not supported.
   10 |     x = x * 2.0;
      |           ^
float.bpf.c:11:12: error: A call to built-in function '__fixdfsi' is not supported.
   11 |     return (int)x;
      |            ^
2 errors generated.
```

内核里的时间、计数、比例全是整数，需要小数时自己定好单位（纳秒、千分比之类）。

### 4.2 栈只有 512 字节

栈上放不下大对象。下面的函数在栈上开了一个 1 KB 的数组，而且真的用了它，clang 在编译期就拒绝：

```c
SEC("tp/syscalls/sys_enter_write")
int big_stack(void *ctx)
{
    char buf[1024];
    int i;

    for (i = 0; i < 1024; i++) {
        buf[i] = i;
    }
    bpf_printk("last byte %d\n", buf[1023]);
    return 0;
}
```

::: terminal title="bash - 栈超限"

```text
$ clang -g -O2 -target bpf -mcpu=v3 -I. -c stack_too_big.bpf.c -o stack_too_big.bpf.o
stack_too_big.bpf.c:5:5: error: Looks like the BPF stack limit is exceeded. Please move large on stack variables into BPF per-cpu array map. For non-kernel uses, the stack can be increased using -mllvm -bpf-stack-size.
    5 | int big_stack(void *ctx)
      |     ^
stack_too_big.bpf.c:5:5: note: could not determine the original source location for stack_too_big.bpf.c:0:0
1 error generated.
```

:::

错误信息本身就把出路写出来了：大对象移到每 CPU 数组 map 里（[B.6](/教程/正文/系统编程/拓展B_eBPF/B_6_Map与数据交换)），或者用环形缓冲区的预留空间。注意报错位置指向函数定义而不是数组声明，这是编译器在函数退栈时才发现总用量超了。

有个细节值得留意：如果把 `buf` 声明了却只用 `buf[0]`，clang 的 `-O2` 会把整个数组优化掉，连报错都没有——反汇编里只剩两条指令：

```text
0000000000000000 <big_stack>:
       0:	b4 00 00 00 01 00 00 00	w0 = 0x1
       1:	95 00 00 00 00 00 00 00	exit
```

所以「编译通过」不等于「栈够用」，写内核态程序时要用 `llvm-objdump -d` 确认大对象真的没被优化成栈上的东西。

<Quiz
  :options="[
    { label: '在函数里声明 char buf[1024] 并逐个写入', value: 'A' },
    { label: '在 64 位寄存器上做整数乘法', value: 'B' },
    { label: '在 kprobe 程序的入口读 ctx 参数', value: 'C' },
    { label: '在循环里累加一个整数 64 次', value: 'D' }
  ]"
  answer="A"
>

<template #question>

下列写法中，哪一种在编译期就会失败？

</template>

正确答案是 **A**。栈只有 512 字节，1 KB 的数组直接用满，clang 报 `Looks like the BPF stack limit is exceeded`。整数乘法和有界循环都是 BPF 的基本能力；读 `ctx` 受程序类型允许，kprobe 的上下文本就是一个寄存器快照。浮点也会在编译期失败，但那属于上一节的内容，不在这道题的选项里。

</Quiz>

### 4.3 不能递归，调用深度有限

BPF 到 BPF 的函数调用是支持的，但调用栈的深度上限是 8 层。递归会保留成真正的 `call` 指令，加载时不可能通过：

```text
$ llvm-objdump -dr rec3.bpf.o
0000000000000000 <is_even>:
       0:	b4 00 00 00 01 00 00 00	w0 = 0x1
       1:	16 01 02 00 00 00 00 00	if w1 == 0x0 goto +0x2 <is_even+0x20>
       2:	04 01 00 00 ff ff ff ff	w1 += -0x1
       3:	85 10 00 00 01 00 00 00	call 0x1
       4:	95 00 00 00 00 00 00 00	exit
```

`call 0x1` 里的 `src_reg` 是 1，表示调用同一目标文件里的子程序，`imm` 是相对偏移。两个函数互相调用，调用图是一个环，环里的路径长度没有上界，验证器算不出调用深度，加载时被拒。

直接递归（函数调自己）的情况更值得注意：clang 19 在 `-O2` 下会把 `fact(n)` 改写成循环，编译干净通过，反汇编里根本没有 `call`。但这不代表递归在 BPF 里可用——换一种写法、换一个编译器版本，改写就可能不发生，那时同样过不了验证器。需要重复计算时写成有界的循环，不要指望编译器的这一手。

### 4.4 循环必须能证明会结束

内核 5.3 之后循环是允许的，但验证器要能算出上界。`for (int i = 0; i < 64; i++)` 里的 64 是常量，验证器把循环体展开 64 次来检查，这是最省事的写法。上界如果来自运行期的输入，就要么先用条件把范围收窄，要么改用 `bpf_loop`（内核 5.17 起）或 `bpf_for` 这类开放编码迭代器（内核 6.4 起）。验证器的具体判定在 [B.9](/教程/正文/系统编程/拓展B_eBPF/B_9_验证器)。

顺带一个编译期的观察：`-O2` 会把能算出来的循环整个展开。求和结果编译期就确定的循环，反汇编只剩 `w0 = 0x7e0`（0～63 的和）和 `exit`：

```c
int sum = 0;

for (int i = 0; i < 64; i++) {
    sum += i;
}
return sum;
```

而循环体依赖运行期的值时，循环才会真的出现在字节码里：

```text
$ llvm-objdump -d looptest.bpf.o
0000000000000000 <bounded>:
       0:	b4 06 00 00 00 00 00 00	w6 = 0x0
       1:	85 00 00 00 08 00 00 00	call 0x8
       2:	b4 02 00 00 00 00 00 00	w2 = 0x0
       3:	bc 61 00 00 00 00 00 00	w1 = w6
       4:	54 01 00 00 01 00 00 00	w1 &= 0x1
       5:	84 01 00 00 00 00 00 00	w1 = -w1
       6:	5c 01 00 00 00 00 00 00	w1 &= w0
       7:	0c 21 00 00 00 00 00 00	w1 += w2
       8:	04 06 00 00 01 00 00 00	w6 += 0x1
       9:	bc 12 00 00 00 00 00 00	w2 = w1
      10:	16 06 01 00 40 00 00 00	if w6 == 0x40 goto +0x1 <bounded+0x60>
      11:	05 00 f7 ff 00 00 00 00	goto -0x9 <bounded+0x18>
      12:	bc 10 00 00 00 00 00 00	w0 = w1
      13:	95 00 00 00 00 00 00 00	exit
```

指令 10 和 11 构成循环的出口和回边：`w6` 是计数器，和 0x40（64）比较，不等就跳回指令 3。上界写在指令里，验证器才认得出这是个有界循环。

### 4.5 指针不能随意解引用

上下文和栈可以直接读，map 里的值在判空之后可以直接读，除此之外的内存都要通过辅助函数：内核内存用 `bpf_probe_read_kernel`，用户空间内存用 `bpf_probe_read_user`。原因不在语法，在于验证器需要知道每次访问的目标是不是安全的——直接解引用一个来路不明的地址，验证器无法判断，只能拒绝。

CO-RE 的 `BPF_CORE_READ` 系列宏是这类调用的简写，展开后仍然是一串 `bpf_probe_read_kernel`。三种写法的对比和原理在 [B.5](/教程/正文/系统编程/拓展B_eBPF/B_5_辅助函数与上下文) 与 [B.8](/教程/正文/系统编程/拓展B_eBPF/B_8_libbpf_BTF与CO_RE)。

### 4.6 为什么常写 static __always_inline

头文件里对两个属性的定义很直白：

```c
#define __always_inline inline __attribute__((always_inline))
#define __noinline __attribute__((noinline))
```

对一个小函数加上 `__always_inline`，它会展开进每个调用点，字节码里只有一条直线；加 `__noinline`，它就留在 `.text` 段，成为一个独立的子程序，调用点是一条 `call -0x1` 加重定位：

```text
$ llvm-objdump -dr inl.bpf.o

Disassembly of section .text:

0000000000000000 <thrice>:
       0:	bc 10 00 00 00 00 00 00	w0 = w1
       1:	24 00 00 00 03 00 00 00	w0 *= 0x3
       2:	95 00 00 00 00 00 00 00	exit

Disassembly of section tp/syscalls/sys_enter_write:

0000000000000000 <f>:
       0:	85 00 00 00 08 00 00 00	call 0x8
       1:	bc 06 00 00 00 00 00 00	w6 = w0
       2:	64 06 00 00 01 00 00 00	w6 <<= 0x1
       3:	bc 01 00 00 00 00 00 00	w1 = w0
       4:	85 10 00 00 ff ff ff ff	call -0x1
		0000000000000020:  R_BPF_64_32	.text
       5:	0c 06 00 00 00 00 00 00	w6 += w0
       6:	bc 60 00 00 00 00 00 00	w0 = w6
       7:	95 00 00 00 00 00 00 00	exit
```

`thrice` 没有展开，`f` 里用 `call -0x1` 加一条指向 `.text` 的重定位来调它；`twice` 加了 `__always_inline`，只剩指令 2 的 `w6 <<= 1` 一条，看不到任何调用痕迹。

选择哪种写法的理由在于验证：每个 BPF 函数单独验证，内联之后验证器看到的是调用者上下文里的一段直线代码，能利用调用点已知的约束；不内联则子程序要按最一般的情况证明自己安全。所以工具库里的短小辅助函数几乎都写成 `static __always_inline`。代价是代码体积——重复展开会让程序变大，而程序大小有上限。

## 5. 旧资料里的限制

网上流传的不少 BPF 教程写于内核 4.x 时期，里面的一些「不能」今天是错的。对照表如下，版本号只在这一节出现。

| 旧说法 | 今天的情况 | 何时放宽 |
| --- | --- | --- |
| 不能有循环 | 有界循环可以，验证器会展开检查 | 内核 5.3 |
| 不能有全局对象 | `.bss`、`.data`、`.rodata` 里的全局对象正常使用 | 内核 5.2（libbpf 同时支持） |
| 不能有常量字符串 | 字符串放进 `.rodata`，加载时重定位 | 内核 5.2 |
| 一切都要内联，不能有函数调用 | BPF 到 BPF 的调用可用，深度上限 8 | 内核 4.16、LLVM 6.0 |
| 程序最多 4096 条指令 | 特权程序的上限提高到 100 万条指令；非特权程序仍是 4096 条 | 内核 5.2 |
| 栈只有 512 字节 | 仍然如此 | 未变 |
| 不能用浮点 | 仍然如此 | 未变 |

作者的经验是：看到「BPF 不能做某某」这样的断言，先看文章是哪一年写的。2019 年之前的材料里，上表前五行基本都成立，之后陆续失效；栈和浮点这两条从 BPF 诞生至今没变过。另有三条说法与年代无关，一直就不对：

| 常见说法 | 实际情况 |
| --- | --- |
| 一个 BPF 程序只能有一个入口，多个程序要分文件 | 一个 `.bpf.c` 里可以写任意多个带 `SEC` 的函数，各自是一个程序 |
| `SEC` 后面的字符串只是注释 | 它是 ELF 节名，libbpf 靠它判断程序类型和附加方式 |
| 许可证随便写，只影响法律问题 | 不写或写不兼容的值，调用 GPL 专用辅助函数会被验证器拒绝 |

<Quiz
  :options="[
    { label: '栈只有 512 字节', value: 'A' },
    { label: '不能有全局对象', value: 'B' },
    { label: '循环必须能证明会结束', value: 'C' },
    { label: '不能用浮点运算', value: 'D' },
    { label: '不能调用 libc 函数', value: 'E' }
  ]"
  :answer="['A', 'C', 'D', 'E']"
>

<template #question>

在这一章的环境下（内核 6.12、clang 19、libbpf 1.5），下列哪些限制仍然有效？

</template>

正确答案是 **A、C、D、E**。B 是已经放宽的旧限制——`.bss`、`.data`、`.rodata` 里的全局对象从内核 5.2 起就能正常使用，libbpf 把它们建成 map。栈的大小、浮点运算、libc 的缺席从 BPF 诞生至今没有变过；循环则从「完全禁止」变成「必须有可证明的上界」。

</Quiz>

::: tip 判断一份资料的年代

看它用不用 `SEC("kprobe/...")` 之外的旧式节名，看有没有 `struct bpf_map_def` 这种 BTF 之前的 map 声明，看有没有 `bcc` 的 C 代码内嵌字符串。三者出现任意一个，基本可以确定是 2019 年以前的写法。

:::

## 6. 编译选项

内核态程序的编译命令在[环境准备](/教程/正文/系统编程/拓展B_eBPF/B_2_环境准备与工具链)里给过，这里解释每个选项为什么必须有：

::: terminal title="bash - 内核态程序的编译"

```text
$ clang -g -O2 -target bpf -mcpu=v3 -D__TARGET_ARCH_x86 -I. -c hello.bpf.c -o hello.bpf.o
```

:::

- `-target bpf` 选目标架构。不写的话 clang 按本机架构编译，产出的目标文件 libbpf 根本不认。
- `-g` 生成 BTF。没有 BTF，CO-RE 重定位（[B.8](/教程/正文/系统编程/拓展B_eBPF/B_8_libbpf_BTF与CO_RE)）无法进行，`bpftool gen skeleton` 也拿不到类型信息。对比 1.4 节的节列表，加 `-g` 之前没有 `.BTF` 和 `.BTF.ext`。
- `-O2` 不是可选项。`-O0` 的代码把每个局部对象都摆到栈上，读写来回倒腾，指令数通常是 `-O2` 的两三倍，验证器的复杂度预算很快用完。更要紧的是间接调用：C 里通过函数指针调用时，`-O2` 能把目标确定下来并直接内联，`-O0` 会保留成 `callx`（opcode 0x8d），而内核验证器不支持这条指令。内核文档对此的原话是：使用 `-O0` 时 clang 会生成 `BPF_CALL | BPF_X | BPF_JMP`（0x8d）指令，而 Linux 内核验证器不支持该指令。
- `-mcpu=v3` 选指令集版本。不写的话，LLVM 20 之前默认是 v1，20 起默认是 v3。写出来是为了让不同版本的 clang 生成相同的指令，别人的机器上 `llvm-objdump` 出来的结果才和书上对得上。
- `-D__TARGET_ARCH_x86` 是 `bpf_tracing.h` 的要求。用 `BPF_KPROBE` 这类宏而不定义它，编译直接失败：

```text
trace.bpf.c:6:5: error: Must specify a BPF target arch via __TARGET_ARCH_xxx
```

`-mcpu` 的差别不是纸面上的。同一个 hello 程序，用 v1 和 v3 编译出来字节码不同：

::: terminal title="bash - -mcpu=v1 与 -mcpu=v3"

```text
$ clang -target bpf -mcpu=v1 -O2 -g -c hello.bpf.c -o hello_v1.o
$ clang -target bpf -mcpu=v3 -O2 -g -c hello.bpf.c -o hello.bpf.o
$ diff <(llvm-objdump -d hello_v1.o | tail -n +3) \
       <(llvm-objdump -d hello.bpf.o | tail -n +3)
9c9
<        5:	5d 01 05 00 00 00 00 00	if r1 != r0 goto +0x5 <handle_write+0x58>
---
>        5:	5e 01 05 00 00 00 00 00	if w1 != w0 goto +0x5 <handle_write+0x58>
11,12c11,12
<        8:	b7 02 00 00 1c 00 00 00	r2 = 0x1c
<        9:	bf 03 00 00 00 00 00 00	r3 = r0
---
>        8:	b4 02 00 00 1c 00 00 00	w2 = 0x1c
>        9:	bc 03 00 00 00 00 00 00	w3 = w0
14c14
<       11:	b7 00 00 00 00 00 00 00	r0 = 0x0
---
>       11:	b4 00 00 00 00 00 00 00	w0 = 0x0
```

:::

v1 用 64 位寄存器做比较和搬运（`if r1 != r0`、`r2 = 0x1c`、`r0 = 0x0`），v3 用 32 位子寄存器（`if w1 != w0`、`w2 = 0x1c`、`w0 = 0x0`），指令的 opcode 也随之不同。这就是 v3 的意义：`w` 前缀表示只操作低 32 位，JIT 之后的本机代码更短。

不过 v1 只是没有 32 位指令，并不是不能用 `int`。它会把 32 位运算用 64 位指令模拟出来，代价是多几条指令。拿一个 `int` 参数的函数对比，差别在条数上就能看出来：

```text
$ clang -target bpf -mcpu=v1 -O2 -c calc_int.bpf.c -o ci_v1.o
$ llvm-objdump -d ci_v1.o
0000000000000000 <calc>:
       0:	bf 10 00 00 00 00 00 00	r0 = r1
       1:	27 00 00 00 03 00 00 00	r0 *= 0x3
       2:	bf 12 00 00 00 00 00 00	r2 = r1
       3:	67 02 00 00 20 00 00 00	r2 <<= 0x20
       4:	c7 02 00 00 20 00 00 00	r2 s>>= 0x20
       5:	65 02 02 00 0a 00 00 00	if r2 s> 0xa goto +0x2 <calc+0x40>
       6:	07 01 00 00 01 00 00 00	r1 += 0x1
       7:	bf 10 00 00 00 00 00 00	r0 = r1
       8:	95 00 00 00 00 00 00 00	exit
$ clang -target bpf -mcpu=v3 -O2 -c calc_int.bpf.c -o ci_v3.o
$ llvm-objdump -d ci_v3.o
0000000000000000 <calc>:
       0:	bc 10 00 00 00 00 00 00	w0 = w1
       1:	24 00 00 00 03 00 00 00	w0 *= 0x3
       2:	66 01 02 00 0a 00 00 00	if w1 s> 0xa goto +0x2 <calc+0x28>
       3:	04 01 00 00 01 00 00 00	w1 += 0x1
       4:	bc 10 00 00 00 00 00 00	w0 = w1
       5:	95 00 00 00 00 00 00 00	exit
```

v1 版本 9 条指令，其中指令 2 到 4 是把 32 位值左移后算术右移，靠 64 位指令补出符号扩展；v3 版本 6 条，直接比 `w1` 和 0xa。如果函数参数本来就是 `long`，两版生成的字节码完全相同——32 位指令省不掉任何东西。这也是为什么编译命令里统一写 `-mcpu=v3`：对宽整数没有损失，对窄整数则能省下指令。

`-O0` 与 `-O2` 的差距用同一个函数对比最直观。左边是 `-O0`，右边是 `-O2`：

::: compare vertical left-title="-O0" right-title="-O2"

```text
0000000000000000 <calc>:
       0:	7b 1a f8 ff 00 00 00 00	*(u64 *)(r10 - 0x8) = r1
       1:	79 a1 f8 ff 00 00 00 00	r1 = *(u64 *)(r10 - 0x8)
       2:	c5 01 05 00 0b 00 00 00	if r1 s< 0xb goto +0x5 <calc+0x40>
       3:	05 00 00 00 00 00 00 00	goto +0x0 <calc+0x20>
       4:	79 a1 f8 ff 00 00 00 00	r1 = *(u64 *)(r10 - 0x8)
       5:	27 01 00 00 03 00 00 00	r1 *= 0x3
       6:	7b 1a f0 ff 00 00 00 00	*(u64 *)(r10 - 0x10) = r1
       7:	05 00 04 00 00 00 00 00	goto +0x4 <calc+0x60>
       8:	79 a1 f8 ff 00 00 00 00	r1 = *(u64 *)(r10 - 0x8)
       9:	07 01 00 00 01 00 00 00	r1 += 0x1
      10:	7b 1a f0 ff 00 00 00 00	*(u64 *)(r10 - 0x10) = r1
      11:	05 00 00 00 00 00 00 00	goto +0x0 <calc+0x60>
      12:	79 a0 f0 ff 00 00 00 00	r0 = *(u64 *)(r10 - 0x10)
      13:	95 00 00 00 00 00 00 00	exit
```

---

```text
0000000000000000 <calc>:
       0:	bf 10 00 00 00 00 00 00	r0 = r1
       1:	27 00 00 00 03 00 00 00	r0 *= 0x3
       2:	65 01 02 00 0a 00 00 00	if r1 s> 0xa goto +0x2 <calc+0x28>
       3:	07 01 00 00 01 00 00 00	r1 += 0x1
       4:	bf 10 00 00 00 00 00 00	r0 = r1
       5:	95 00 00 00 00 00 00 00	exit
```

:::

`-O0` 版本 14 条指令，参数先写栈、用到时再读回来、结果也写栈，最后才读进 R0，一共七次 `r10` 相对访问；`-O2` 版本 6 条，全部在寄存器里完成，一次栈访问都没有。验证器对每个分支都要重算寄存器状态，前者要处理的分支和栈状态都多得多。

Windows 上用 Zig 自带的 clang 做本页的编译检查时，还要多一个 `-fno-sanitize=all`：Zig 的驱动默认打开一组未定义行为检查，而 BPF 目标没有运行库来报告它们。发行版自带的 clang 不需要这个选项。

## 7. 写错了会怎样

前几节的错误散布在各处，这里集中列一遍，都是编译期就能发现的：

| 写法 | 结果 |
| --- | --- |
| `char license[] SEC("license")` | `error: symbol 'license' is already defined`，符号名与节名相同 |
| 栈上开 1 KB 数组并实际使用 | `error: Looks like the BPF stack limit is exceeded` |
| 用 `double` 做加减乘 | `error: A call to built-in function '__adddf3' is not supported` |
| 调用 `printf`、`malloc` 等 libc 函数 | 编译能过，留下未定义的符号；`bpftool gen skeleton` 报 `failed to find BTF for extern 'printf'` |
| 用 `BPF_KPROBE` 却没定义 `__TARGET_ARCH_x86` | `error: Must specify a BPF target arch via __TARGET_ARCH_xxx` |
| 未启用优化时写了函数指针调用 | 字节码里出现 `callx`（0x8d），内核验证器不认 |

第三行和第四行的区别值得记住：浮点在编译期就断了，因为编译器找不到模拟例程；libc 函数要到打包阶段才断，因为目标文件里只是留了一个外部符号，libbpf 处理 extern 时才需要对应的 BTF 信息。后者更容易让人困惑，因为 `clang` 一声不响地成功了。

`callx` 那一行值得单独看一眼。C 里用函数指针时，`-O0` 生成的是真正的间接调用：

```text
      22:	79 a2 e8 ff 00 00 00 00	r2 = *(u64 *)(r10 - 0x18)
      23:	79 a1 f8 ff 00 00 00 00	r1 = *(u64 *)(r10 - 0x8)
      24:	8d 02 00 00 00 00 00 00	callx r2
```

`0x8d` 是 `callx`，寄存器里放调用目标。同一段代码在 `-O2` 下 clang 能确定两个候选函数并各自内联，`callx` 不会出现。所以「`-O0` 能不能编译过」取决于代码里有没有函数指针，而不是 `-O0` 本身——但函数指针在 C 里太常见，规矩写成「必须优化」更省事。

还有一类错误编译器管不了：编译干净、符号齐全，但加载时被验证器拒绝。典型的例子是 map 查找之后没有判空就去写、环形缓冲区 reserve 之后有条路径没有提交。这些属于验证器的工作范围，报错怎么读、怎么改，在 [B.9](/教程/正文/系统编程/拓展B_eBPF/B_9_验证器)。

## 8. 习题

### 8.1 巩固

<Exercise id="47401" :d="2" :w="1">

下面这段程序编译后，`count`、`limit`、`table` 三个对象分别落在哪个节？用 `llvm-readelf -S` 验证你的答案。

```c
int count = 0;
const volatile int limit = 100;
int table[4] = {1, 2, 3, 4};

SEC("tp/syscalls/sys_enter_write")
int f(void *ctx) { return count + limit + table[0]; }
```

再回答一个问题：为什么 `limit` 必须写成 `const volatile`，只写 `const` 会有什么后果？

</Exercise>

<Exercise id="47402" :d="2" :w="2">

下面是从 hello.bpf.o 里截出来的四条指令：

```text
       0:	85 00 00 00 0e 00 00 00	call 0xe
       2:	18 01 00 00 00 00 00 00 00 00 00 00 00 00 00 00	r1 = 0x0 ll
       4:	61 11 00 00 00 00 00 00	r1 = *(u32 *)(r1 + 0x0)
       5:	5e 01 05 00 00 00 00 00	if w1 != w0 goto +0x5
```

(1) 第 0 条调用的是哪个辅助函数？根据是什么？
(2) 为什么编号从 2 直接跳到 4？
(3) 第 5 条跳转生效时，下一条执行的指令编号是多少？
(4) `61 11` 这两个字节里的两个 `11` 分别是什么含义？

</Exercise>

### 8.2 编程

<Exercise id="47403" :d="3" :w="3">

`code/系统编程/拓展B_eBPF/B_4_程序的解剖_受限的C/stack_too_big.bpf.c` 里有两个错误：一个是栈超限，另一个在 license 那一行。分别改掉它们，让文件通过编译，并说明两个错误分别在编译的哪个阶段被发现。

改好之后用 `llvm-objdump -d` 确认：大数组确实不在栈上了。提示：可以把数组改成每 CPU 数组 map（[B.6](/教程/正文/系统编程/拓展B_eBPF/B_6_Map与数据交换) 有写法），也可以先把它缩小到 512 字节以内看现象。

</Exercise>

<Exercise id="47404" :d="3" :w="3">

写一个 `twoprog.bpf.c`，满足以下要求，编译后用 `llvm-objdump -h` 和 `llvm-readelf -S` 检查：

- 一个文件里放两个程序，分别挂在 `tp/syscalls/sys_enter_write` 和 `tp/syscalls/sys_enter_read` 上；
- 一个初始化为非零的 `int` 全局对象，落在 `.data`；
- 一个 `const volatile int` 全局对象，落在 `.rodata`；
- license 用 `"GPL"`。

验收标准：节列表里同时出现两个 `tp/` 开头的节、`.data` 和 `.rodata`，且 `.data` 的大小与你声明的对象一致。再用 `bpftool btf dump file twoprog.bpf.o` 找到两个 `FUNC` 记录和两个 `DATASEC` 记录，把输出贴出来。

</Exercise>

<Exercise id="47405" :d="4" :w="3">

取本页的 `calc.bpf.c`，分别用 `-mcpu=v1` 和 `-mcpu=v3` 编译，把两份反汇编都保存下来做 diff：

(1) 逐条说明差异出在哪里。两版的指令条数一样，那么 v3 省在了什么地方？
(2) 把函数改成 `int calc(int x)`，再对比一次。这次两版的指令条数分别是多少？
(3) 用 `-mcpu=v1` 编译的字节码，能在只支持 v1 的内核上加载；反过来，用 `-mcpu=v3` 编译、在只支持 v1 的内核上加载，会怎样？第二问可以结合 `llc -march bpf -mcpu=help` 的输出和 `-mcpu=probe` 的用途回答。

</Exercise>

<Exercise id="47406" :d="5" :w="4">

本页的 `mini_vm.c` 只能跑 `-O2` 生成的 calc 字节码。把它扩展到能跑 `clang -O0 -c calc.bpf.c` 生成的那一份，需要补上四类指令：

- `0x7b`：把 64 位寄存器写到 `r10` 减偏移的栈位置（STX，双字）；
- `0x79`：从栈位置读回 64 位寄存器（LDX，双字）；
- `0xc5`：有符号小于立即数时跳转（JMP32，`if r1 s< 0xb`）；
- `0x05`：无条件跳转（JA）。

自己用 `clang -O0 -c calc.bpf.c` 生成字节码，用 `llvm-objdump -d` 抄成字节数组，然后验证七组输入的运行结果与 `-O2` 版本一致。

要求：栈用 `uint8_t stack[512]` 表示，R10 初始化为 512，访存按小端解释。做完之后回答：为什么 `-O0` 的字节码里会出现 `r10 - 0x8` 这样的地址，而 `-O2` 版本一条都没有？

</Exercise>

### 8.3 综合与挑战

<Exercise id="47407" :d="6" :w="5" scope="*">

给 `mini_vm.c` 加一个最小的静态检查器，在真正执行之前拒绝明显非法的字节码。至少检查这四项，每项都要给出可读的报错信息：

(1) opcode 是否在支持的集合里；
(2) 每条指令的 `dst_reg` 和 `src_reg` 是否都不大于 10；
(3) 所有跳转的目标是否落在指令范围内；
(4) 最后一条指令是不是 `exit`，以及从第 0 条出发是否所有路径都能到达 `exit`。

用三类输入测试：正常的 calc 字节码、故意把跳转偏移改大的字节码、把最后一条 `exit` 改成 `w0 = 0` 的字节码。第三类需要你实现一个可达性检查，可以用简单的深度优先搜索。

做完之后读一段真实验证器的日志（[B.9](/教程/正文/系统编程/拓展B_eBPF/B_9_验证器) 里有），对比你的检查器和它在检查深度上的差别。

</Exercise>

## 9. 参考资料

- [BPF Instruction Set Specification, v1.7](https://docs.kernel.org/bpf/standardization/instruction-set.html)：指令编码、指令类别、`call` 的三种形式。
- [RFC 9669: BPF Instruction Set Architecture (ISA)](https://www.rfc-editor.org/rfc/rfc9669)：指令集的标准化文档。
- [Classic BPF vs eBPF](https://docs.kernel.org/bpf/classic_vs_extended.html)：寄存器数量与宽度、调用约定。
- [eBPF Instruction Set Architecture (ISA)](https://docs.ebpf.io/linux/instruction-set/)：寄存器分工与栈的说明。
- [BPF verifier](https://docs.ebpf.io/linux/concepts/verifier/)：有界循环的引入版本、复杂度上限。
- [BPF functions and calling convention](https://docs.ebpf.io/linux/concepts/functions/)：调用深度 8、子程序的栈帧。
- [BPF licensing](https://docs.kernel.org/bpf/bpf_licensing.html)：GPL 兼容性检查。
- [Clang implementation notes](https://docs.kernel.org/bpf/clang-notes.html)：`-O0` 与 `callx`、`-mcpu=v3` 与 alu32。
- [BPF LLVM relocations](https://docs.kernel.org/bpf/llvm_reloc.html)：`R_BPF_64_64` 与 `R_BPF_64_32` 的含义。
- [BPF design Q&A](https://docs.kernel.org/bpf/bpf_design_QA.html)：4096 条指令上限与 100 万条的验证复杂度上限。
- [BPF toolchain requirements](https://docs.cilium.io/en/latest/reference-guides/bpf/toolchain/)：旧资料中各项限制的来源。
- [libbpf-bootstrap](https://github.com/libbpf/libbpf-bootstrap)：hello 示例与 `LICENSE` 的写法。
- **_Learning eBPF_**（Liz Rice，O'Reilly，2023）：第 3 章「Anatomy of an eBPF Program」的讲解顺序。
