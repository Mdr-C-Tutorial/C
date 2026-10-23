# 1.7 SWD、JTAG 与片上调试

固件烧进去了，板载 LED 却不亮，串口也一个字没吐出来。用 [1.6 构建、烧录与调试](/教程/正文/嵌入式/1_平台与工具链/1_6_构建_烧录与调试) 里的串口 ISP 再烧一遍，结果一样——那条路只能把二进制写进 Flash，写进去之后程序跑到哪一步、某个全局对象变成了什么值，它一概看不见。想看见，就得让芯片把内核停下来，把寄存器和内存交给外部工具读。这就是片上调试。

本节使用 ST-LINK/V2 调试探针 (debug probe) 和 OpenOCD、GDB 这一套开源工具，先看清调试信号从 GDB 一路走到内核的哪几个寄存器，再用仓库里的 `debug_demo` 跑一遍完整的下载、断点、单步、观察点，最后处理两类只有调试才会碰到的事：调试输出走哪条路，以及调试器连不上时怎么救回来。示例只碰内核和 SRAM，不碰外设，所以不需要额外接线。

## 1. 为什么需要片上调试

嵌入式程序出错时的现场往往只有一句话的描述：设备不工作了。没有崩溃对话框，没有 core dump，连 `printf` 都要先有可用的串口。能拿到的证据只有三种：

1. 串口日志。前提是串口已经调通、`printf` 重定向已经写好。程序如果在时钟初始化里就跑飞，这一条什么也给不出。
2. 逻辑分析仪。能看到引脚上的波形，看不到波形为什么不出现。
3. 调试探针。能暂停内核、读任意内存与外设寄存器、单步执行、在某个对象被改写时停下来。

第三种是唯一能在程序跑到一半时观察内部状态的。它和「打印调试」的关系不是替代：打印适合看长时间的趋势和偶发事件，调试器适合看某一刻的精确状态。两者都用，排查效率最高。

片上调试还有一个前提常被忽略：芯片的调试接口必须在复位后没有被关掉，程序也没有把 CPU 送进连调试器都唤不醒的低功耗模式。这两件事都可能由固件自己造成，第 8 节处理。

## 2. JTAG 与 SWD

### 2.1 两套信号

JTAG (Joint Test Action Group) 原本是为芯片测试设计的标准，后来成了通用的调试接口。它用四根（有时五根）信号：

| 信号 | 方向 | 作用 |
| --- | --- | --- |
| TCK | 输入 | 时钟，由调试器提供 |
| TMS | 输入 | 状态机的控制线，每个时钟沿采样 |
| TDI | 输入 | 数据输入 |
| TDO | 输出 | 数据输出 |
| nTRST | 输入 | 复位调试逻辑，可选 |

串行线调试 (Serial Wire Debug, SWD) 是 Arm 为微控制器设计的精简协议，只用两根线：

| 信号 | 方向 | 作用 |
| --- | --- | --- |
| SWCLK | 输入 | 时钟，由调试器提供 |
| SWDIO | 双向 | 命令与数据复用的一根线 |

SWD 另外可以引出一根串行线输出 (Serial Wire Output, SWO)，用来从芯片单向输出跟踪数据，第 7 节会用到。它不属于调试通道本身，不接也能调试。

### 2.2 F103 上的引脚

STM32F103 的调试端口叫 SWJ-DP，同一个硬件既能说 JTAG 也能说 SWD，两种协议分时复用同一组引脚：

| 引脚 | JTAG 功能 | SWD 功能 | 复位后 |
| --- | --- | --- | --- |
| PA13 | JTMS | SWDIO | 调试接口 |
| PA14 | JTCK | SWCLK | 调试接口 |
| PA15 | JTDI | — | 调试接口 |
| PB3 | JTDO | TRACESWO | 调试接口 |
| PB4 | NJTRST | — | 调试接口 |

这五个脚复位后归调试接口所有，不能直接当 GPIO 用。要用其中几个作普通 I/O，必须改写 AFIO_MAPR 的 SWJ_CFG 位，规则见 [1.3 芯片手册、开发板资料与引脚复用](/教程/正文/嵌入式/1_平台与工具链/1_3_芯片手册_开发板资料与引脚复用) 的表格。

最小系统板上普遍用 SWD 而不是 JTAG，原因很实际：两根线与五根线相比，排针更小、走线更少，而且 SWD 的速度对调试微控制器完全够用。JTAG 的优势在别处：多颗芯片串成菊花链一起调试，以及边界扫描测试电路板焊接。日常开发用不上。

### 2.3 接线

ST-LINK/V2 与 Blue Pill 之间接四根线：

| ST-LINK/V2 20 芯接口的引脚 | 开发板 | 说明 |
| --- | --- | --- |
| 1（VAPP） | 3.3 V | 电平参考；也可以用它对板子供电，电流有限 |
| 4、6、8、10 等（GND） | GND | 必须共地，任选一个 |
| 7（TMS/SWDIO） | PA13 | 双向数据线 |
| 9（TCK/SWCLK） | PA14 | 时钟 |
| 15（nRESET） | NRST | 可选，接上才能做硬件复位 |

各版本 ST-LINK 的引脚编号排布不同，以探头外壳上印的丝印为准。四根线里最容易被漏掉的是 GND：只接三根信号线时，调试器有时也能连上，但时序余量很小，改一下杜邦线的位置就可能连不上。

::: tip 先接 GND 再接信号线

插拔杜邦线时先接 GND、后接信号线，拆的时候反过来。板上电时单独插 SWDIO 一根线，瞬间的电位差有可能通过引脚的保护二极管灌进电流。

:::

## 3. 从 GDB 到内核

从键盘上的 `continue` 到内核真正恢复执行，中间隔了四个程序：

```mermaid
flowchart LR
    G["arm-none-eabi-gdb<br/>主机上的调试器"] <-->|"TCP 3333<br/>GDB 远程协议"| O["OpenOCD<br/>调试服务端"]
    O <-->|"USB<br/>ST-LINK 驱动"| S["ST-LINK/V2<br/>调试探针"]
    S <-->|"SWD<br/>SWCLK + SWDIO"| D["SWJ-DP<br/>调试端口"]
    D --> A["AHB-AP<br/>访问端口"]
    A --> M["Flash / SRAM<br/>外设寄存器"]
    A --> C["内核调试寄存器<br/>DHCSR、FPB、DWT"]
```

分工是这样：GDB 负责界面与逻辑，知道断点设在哪一行、单步之后该停在哪；OpenOCD 负责把 GDB 的抽象请求翻译成具体的总线读写，并管理探针；ST-LINK 负责 USB 与 SWD 之间的电气转换；芯片里的 SWJ-DP 是进入芯片的门，门后分成两条路——一条经 AHB-AP 直接读写存储器与外设，一条通到内核的调试寄存器。

两条路带来两种操作。**后台访问**不打断 CPU：经 AHB-AP 读一个地址，CPU 照常跑，读出来的就是那一刻的值。**暂停与单步**则要动用内核调试寄存器：往 DHCSR 里写入调试使能位并置 halt 位，内核在当前指令之后停下；单步是置 step 位再放行一条指令。

断点和观察点也靠内核里的硬件单元实现，因为 Flash 里的代码没法像 RAM 那样临时替换成断点指令。Cortex-M3 为这件事准备了两组比较器：FPB (Flash Patch and Breakpoint) 的 6 个指令比较器在指定地址取指时产生断点，另有 2 个字面量比较器负责把常量加载重定向到别处，不用于断点；DWT (Data Watchpoint and Trace) 的 4 个比较器可以配置成数据观察点，地址匹配的读或写一发生就让内核停下。

::: concept emb-debug-hardware-comparators title="断点与观察点由硬件比较器实现"

Cortex-M3 的 FPB 有 6 个指令比较器用于代码断点，DWT 有 4 个比较器用于数据观察点。两者都是固定的硬件资源，数量不随 Flash 大小或调试器而变，超出时只能删掉已有的断点或改用条件断点。

:::

::: concept emb-debug-bus-access title="调试器经总线直接访问存储器和外设"

调试器通过 AHB-AP 直接读写目标芯片的地址空间，不经过 CPU 执行指令。读 SRAM 或 Flash 不会改变程序状态；读外设寄存器则可能触发硬件副作用，例如读 USART_DR 会清掉 RXNE 标志、读某些状态寄存器会清中断挂起位。在 GDB 里查看外设时，读到的是访问之后的值，不是访问之前的值。

:::

这条副作用不是理论问题。调试时想看看串口收到了什么，随手 `x/1wx 0x40013804`（USART1_DR），RXNE 就被清掉了，程序中原本会处理这个字节的中断再也等不到它。现象是「一接调试器就丢数据」，代码本身没有问题。

## 4. 用 OpenOCD 和 GDB 调试一个程序

### 4.1 示例程序

示例在 `code/嵌入式/1_平台与工具链/1_7_SWD_JTAG与片上调试/debug_demo/`。它只使用内核和 SRAM，不配置任何外设，因此不依赖芯片型号的具体引脚。启动文件与链接脚本用 `code/嵌入式/common/` 里的现成版本，原理在 [1.8 复位、启动与链接](/教程/正文/嵌入式/1_平台与工具链/1_8_复位_启动与链接)。

```c
volatile uint32_t g_mode = 1;
int32_t g_samples[4] = {10, 20, 30, 40};
volatile uint32_t g_tick_count = 0;
volatile int32_t g_last_sum = 0;

__attribute__((noinline)) int32_t sum_array(const int32_t *arr, uint32_t len) {
    int32_t total = 0;
    for (uint32_t i = 0; i < len; ++i) {
        total += arr[i];
    }
    return total;
}

__attribute__((noinline)) void record_sample(uint32_t step, int32_t val) {
    uint32_t idx = step % 5u;
    int32_t *p = g_samples + idx;
    *p = val;
}

int main(void) {
    for (;;) {
        ++g_tick_count;
        record_sample(g_tick_count, (int32_t)(g_tick_count * 10u));
        if (g_mode == 1u) {
            g_last_sum = sum_array(g_samples, 4);
        } else {
            g_last_sum = -1;
        }
    }
}
```

`g_samples` 只有 4 个元素，`record_sample` 却按 0～4 取模。`idx` 等于 4 时，`p` 指向数组之后的那一个字，把 `val` 写进去。这不是故意制造崩溃，而是不少固件里真实出现过的那类越界写：数组定义在别处，越界量恰好是 1，写入的值又不离谱，于是程序照常运行，只有一个全局对象悄悄变了。演示的目的就是用观察点把它抓出来。

### 4.2 构建

```text
debug_demo/
├── CMakeLists.txt
└── main.c
```

`CMakeLists.txt` 只有三行，把目标交给 `common/` 里的 `firmware.cmake`：

```cmake
cmake_minimum_required(VERSION 3.20)
project(debug_demo C)

include("${CMAKE_CURRENT_LIST_DIR}/../../../common/firmware.cmake")

stm32f103_firmware(debug_demo main.c)
```

::: terminal title="bash - 构建 debug_demo"

```text
$ cd code/嵌入式/1_平台与工具链/1_7_SWD_JTAG与片上调试/debug_demo
$ cmake -B build -G Ninja -DCMAKE_TOOLCHAIN_FILE=../../../common/arm-none-eabi.cmake
-- The C compiler identification is GNU 13.3.1
-- Detecting C compiler ABI info - done
-- Check for working C compiler: …/arm-none-eabi-gcc.exe - skipped
-- Configuring done (0.6s)
-- Generating done (0.0s)
$ cmake --build build
[1/3] Building C object emb_common/CMakeFiles/stm32f103_startup.dir/startup_stm32f103.c.obj
[2/3] Building C object CMakeFiles/debug_demo.dir/main.c.obj
[3/3] Linking C executable debug_demo.elf
   text	   data	    bss	    dec	    hex	filename
    500	     20	   1548	   2068	    814	debug_demo.elf
```

:::

500 字节 `text` 里包含向量表、启动代码和三个函数，20 字节 `data` 是 `g_samples` 的四个初值，1548 字节 `bss` 里除四个全局对象外还有 C 库预留的堆与栈。链接产物在同一目录：`debug_demo.elf`、`debug_demo.bin`、`debug_demo.hex`、`debug_demo.map`。调试用 `.elf`，因为它带着符号与行号信息。

`arm-none-eabi-gcc` 不在 `PATH` 里时，configure 时加 `-DARM_TOOLCHAIN_DIR=<工具链的 bin 目录>`。

### 4.3 启动 OpenOCD

OpenOCD 需要两份配置：一份描述探针，一份描述目标芯片。

::: terminal title="bash - 启动 OpenOCD"

```console
$ openocd -f interface/stlink.cfg -f target/stm32f1x.cfg
```

:::

启动成功后终端里会持续输出日志，关键信息有这么几项（具体行文随 OpenOCD 版本与探针固件变化）：

```text
Info : Listening on port 6666 for tcl connections
Info : Listening on port 4444 for telnet connections
Info : clock speed …
Info : STLINK …
Info : Target voltage: …
Info : [stm32f1x.cpu] Cortex-M3 r2p0 processor detected
Info : [stm32f1x.cpu] target has 6 breakpoints, 4 watchpoints
Info : starting gdb server for stm32f1x.cpu on 3333
Info : Listening on port 3333 for gdb connections
```

其中的有效信息是：探针型号、目标电压、内核型号与修订版、**这颗内核支持几个硬件断点和几个观察点**，以及 GDB 端口。`6 breakpoints, 4 watchpoints` 这两个数字固定不变，它们来自 FPB 与 DWT 的比较器数量。

OpenOCD 启动之后保持运行，另开一个终端跑 GDB。它自己占三个端口：3333 给 GDB，4444 是 telnet 命令行，6666 是 TCL 脚本接口。

### 4.4 GDB 会话

::: terminal title="bash - 连接并下载"

```console
$ arm-none-eabi-gdb debug_demo.elf
(gdb) target extended-remote :3333
(gdb) monitor reset halt
(gdb) load
(gdb) break main
(gdb) continue
```

:::

五条命令各做一件事。`target extended-remote :3333` 连上 OpenOCD，之后所有调试请求都走这条连接。`monitor reset halt` 把后面的参数原样转发给 OpenOCD：复位芯片并停住，内核停在复位向量处，尚未执行任何一条指令。`load` 把 `.elf` 里的段写进 Flash 与 RAM。`break main` 在 `main` 的第一行设断点，`continue` 放行。

需要真实硬件才能看到的部分，预期现象如下：

- 执行 `monitor reset halt` 后，`info registers sp pc` 应显示 `sp = 0x20005000`、`pc = 0x080000f0`——也就是 `_estack` 与 `Reset_Handler` 的地址。`x/2wx 0x08000000` 读出的头两个字应当是 `0x20005000` 与 `0x080000f1`，后者比 `Reset_Handler` 的地址大 1，最低位是 Thumb 状态标志。
- 执行 `load` 后，OpenOCD 会打印写入的字节数，随后是若干行校验通过的信息。
- 执行 `continue` 后，GDB 在 `main` 的第一行停下并打印命中的断点编号与源代码位置，该行是 `main.c` 里的 `for (;;)`。

断点命中之后，单步与查看的命令与主机上调试没有区别：

::: terminal title="bash - 单步与查看"

```console
(gdb) next
(gdb) step
(gdb) finish
(gdb) info registers r0 r1 r2 r3
(gdb) print g_tick_count
(gdb) print/x g_mode
(gdb) x/4wx &g_samples
(gdb) bt
(gdb) monitor reset run
```

:::

`next` 越过一行（遇到函数调用不进去），`step` 进入函数，`finish` 执行到当前函数返回。`print` 按对象类型输出，`print/x` 用十六进制，`x/4wx` 按 4 字节十六进制看内存。`bt` 打印调用栈，在中断服务程序里也能用——Cortex-M3 的异常压栈让 GDB 能顺着栈回溯到被打断的代码。最后一条 `monitor reset run` 让芯片复位并全速运行，等于按了一次复位键。

### 4.5 不经过 GDB 直接烧录

只想把程序烧进去时，不必开 GDB。OpenOCD 支持一次性执行完就退出：

::: terminal title="bash - 一条命令完成下载与校验"

```console
$ openocd -f interface/stlink.cfg -f target/stm32f1x.cfg \
    -c "program debug_demo.elf verify reset exit"
```

:::

`program` 依次完成擦除、写入、读回校验，`reset` 复位芯片，`exit` 关闭 OpenOCD。这条命令适合放进构建脚本或持续集成，失败时返回非零退出码。

### 4.6 telnet 命令行

不用 GDB 也能看一点东西。OpenOCD 的 4444 端口提供一个精简的命令行：

::: terminal title="bash - telnet 4444"

```console
$ telnet localhost 4444
> halt
> mdw 0x20000000 4
> reg
> resume
```

:::

`halt` 暂停内核，`mdw` 按字读内存（上面读的是 `g_samples` 开头 4 个字），`reg` 列出通用寄存器，`resume` 继续执行。输出格式随 OpenOCD 版本变化，但内容就是这几样。排查「GDB 连不上但探针认得出芯片」时，先开 telnet 试 `halt`，能停下就说明连接是通的，问题在 GDB 一侧。

### 4.7 其他工具

ST 官方提供 STM32CubeProgrammer，命令行版本叫 `STM32_Programmer_CLI`，功能与 `openocd -c "program ..."` 重叠，支持 ST-LINK、UART、USB DFU 几种连接方式，也能读写选项字节。它的图形界面适合不习惯命令行的场合，脚本里也能调用。

pyOCD 是另一套开源实现，用 Python 写，支持 DAPLink 与 ST-LINK；probe-rs 用 Rust 写，主打与 `cargo` 集成。三者做的事一样：把 GDB 或自己的协议翻译成 SWD 时序。本教程只用 OpenOCD，因为它在 Windows、Linux、macOS 上表现一致，配置文件也最透明。

::: generate emb-debug-gdb-session kind=explanation concepts=emb-debug-bus-access

面向第一次用 GDB 调固件的读者，把上面那一节会话里的每条命令按顺序走一遍：`target extended-remote` 建立了什么连接、`monitor` 前缀为什么要转发给 OpenOCD、`reset halt` 与 `load` 的先后顺序为什么不能反、`break` 设下的断点落在 FPB 的哪个比较器里、`continue` 之后 CPU 是怎样从暂停状态恢复的。用「谁在做事」区分 GDB、OpenOCD、ST-LINK 与内核各自的职责，不要总结成命令清单。

:::

## 5. 断点与观察点

### 5.1 数量限制

主机上调试程序时，断点可以设几十个，因为调试器把目标指令的第一个字节改成一条陷阱指令。目标换成单片机的 Flash 就不行了：Flash 不能按字节改写，改一个字节要先擦掉一整页。于是 Cortex-M3 提供了 FPB 单元，用比较器在取指阶段比对地址，命中就向内核发断点请求。

比较器是硬件，数量固定。RM0008 第 31.12 节写得很清楚：FPB 有 2 个字面量比较器和 6 个指令比较器，后者的用途是「匹配从代码区取指，然后重映射到系统区或产生断点指令」。DWT 有 4 个比较器，可配置成硬件观察点、ETM 触发、PC 采样器或数据地址采样器（第 31.13 节）。OpenOCD 启动时打印的 `6 breakpoints, 4 watchpoints` 就是从这里来的。

GDB 并不知道这些限制，除非有人告诉它：

::: terminal title="bash - 设好上限"

```console
(gdb) set remote hardware-breakpoint-limit 6
(gdb) set remote hardware-watchpoint-limit 4
```

:::

连上 OpenOCD 之后，这两个值通常会被自动协商，但写进 `.gdbinit` 更省事。超过上限时 GDB 会报类似 `Cannot insert hardware breakpoint 7` 的错误，意思很直接：硬件没有第 7 个比较器，删掉一个或改用条件断点。

条件断点（`break main.c:34 if g_tick_count > 1000`）不额外占用比较器：它仍然只占一个比较器，每次命中后由 GDB 读对象判断条件，不满足就自动继续。命中频繁时这会明显拖慢程序，因为每命中一次都要停下来、通信、判断、再放行。

<Quiz
  type="single"
  :options="[
    { label: 'watch 会修改 g_mode 的值来检测写入', value: 'A' },
    { label: 'DWT 比较器匹配地址，命中后让内核暂停', value: 'B' },
    { label: 'GDB 会周期性轮询 g_mode，发现变化就中断', value: 'C' },
    { label: '只有串口打印才能发现这类改写', value: 'D' }
  ]"
  answer="B"
>

<template #question>

`watch g_mode` 为什么能在 `g_mode` 被写入时立刻停住？

</template>

正确答案是 **B**。观察点由 DWT 的比较器实现：CPU 每执行一条访存指令，比较器就把地址与设定的值比对，命中即请求内核暂停，整个过程不占用指令周期以外的执行时间，也不需要 GDB 轮询。A 与 C 都会改变被观察对象或依赖通信周期，前者破坏程序，后者漏掉短暂的写入。

</Quiz>

::: tip 把常用设置写进 .gdbinit

在工程目录下放一个 `.gdbinit`，写入 `set remote hardware-breakpoint-limit 6`、`set remote hardware-watchpoint-limit 4`、`set pagination off` 和常用的 `target extended-remote :3333`，GDB 启动时自动执行。GDB 默认只在信任的目录里读这个文件，不在信任列表里时会提示，按提示在 `~/.gdbinit` 里加 `add-auto-load-safe-path` 即可。

:::

### 5.2 用观察点抓越界写

回到 `debug_demo`。`main` 的循环里，`record_sample` 每 5 次把 `idx` 算成 4，越过 `g_samples` 的末尾。`nm` 给出的地址说明了后果：

::: terminal title="bash - 全局对象在 RAM 里的位置"

```text
$ arm-none-eabi-nm -n debug_demo.elf | grep -E "g_samples|g_mode|g_last_sum|g_tick_count"
20000000 D g_samples
20000010 D g_mode
20000014 B g_last_sum
20000018 B g_tick_count
```

:::

`g_samples` 占 16 字节，从 0x2000_0000 到 0x2000_000F；紧跟着 0x2000_0010 就是 `g_mode`。`idx` 等于 4 时，`&g_samples[4]` 正好是 `g_mode` 的地址，写入的值覆盖了它。`g_mode` 从 1 变成别的数，`main` 里 `if (g_mode == 1u)` 的判断随之走另一条分支，`g_last_sum` 变成 −1。具体的地址随工具链版本和链接顺序可能不同，但同一份构建里两者的相邻关系来自源码中定义的先后顺序，`nm` 的这张表就是最直接的证据。

现象是某个全局对象偶尔莫名其妙地变了值。看代码看不出问题——读写 `g_samples` 的地方全都在下标范围内，越界发生在取模的那个 `5u` 上。这种时候观察点比断点有效：不需要知道是哪一行，只需要知道「谁写了这个地址」。

::: terminal title="bash - 在 GDB 里设观察点"

```console
(gdb) monitor reset halt
(gdb) load
(gdb) watch g_mode
(gdb) continue
```

:::

`watch` 监视的是值：`g_mode` 被写入且值发生变化时，程序立刻停下。GDB 会先后打印观察点的编号、旧值、新值，以及停下来的位置，形如 `record_sample (step=…, val=…) at main.c:28`，指向 `*p = val;` 这一行。`bt` 显示调用链 `record_sample` ← `main`。开了 `-O2` 之后部分局部对象可能已经被优化掉，`print p` 会提示没有这个符号；这时退回去看反汇编，或者用 `info registers` 看基址寄存器里的值。

这里要看清的是：`record_sample` 的源码里没有一处明显的错误，`idx = step % 5u` 本身也合法，只有把 `idx` 与数组长度放在一起看，才知道 4 越界。观察点不受这种「看上去没问题」的迷惑，它只在内存真的被改写时停下来。

观察点也可以用地址设置，`watch *(int32_t *)0x20000010` 与 `watch g_mode` 等价，在符号不可用时（比如只有 `.bin` 没有 `.elf`）只能这么写。

停下之后，如果想知道这个地址是被哪条指令写的，把 GDB 给出的 PC 交给 `addr2line`：

::: terminal title="bash - 从地址回到源代码行"

```text
$ arm-none-eabi-addr2line -e debug_demo.elf -f 0x0800016c
record_sample
…/code/嵌入式/1_平台与工具链/1_7_SWD_JTAG与片上调试/debug_demo/main.c:28
```

:::

0x0800_016c 是 `record_sample` 里那条 `str.w r1, [r4, r0, lsl #2]` 的地址，正好对应 `*p = val;`。这条对应关系来自编译时加的 `-g`：调试信息把每条指令的地址映射回源文件与行号，`firmware.cmake` 里的统一选项已经带上它。反过来，`info line` 可以查某个函数或某一行落在哪个地址：

::: terminal title="bash - 查函数地址"

```text
$ arm-none-eabi-gdb -batch -ex "info line main" -ex "info line record_sample" debug_demo.elf
Line 31 of "…/debug_demo/main.c" starts at address 0x800017c <main> and ends at 0x800017e <main+2>.
Line 26 of "…/debug_demo/main.c" starts at address 0x8000158 <record_sample> and ends at 0x800015a <record_sample+2>.
```

:::

这两条命令不需要连目标，读的是 `.elf` 里的调试信息，所以在没有硬件时也能用来核对地址。

<Quiz
  type="single"
  :options="[
    { label: '6 个，受 FPB 指令比较器数量限制', value: 'A' },
    { label: '4 个，受 DWT 比较器数量限制', value: 'B' },
    { label: '不限，GDB 会把断点指令写进 Flash', value: 'C' },
    { label: '取决于芯片的 Flash 容量', value: 'D' }
  ]"
  answer="A"
>

<template #question>

在 STM32F103（Cortex-M3）上同时能设几个硬件断点？

</template>

正确答案是 **A**。FPB 提供 6 个指令比较器用于代码断点，DWT 的 4 个比较器用于数据观察点，两者各管一类。选项 C 错在 Flash 不能按字节改写，断点无法用写入陷阱指令的方式实现。

</Quiz>

## 6. 查看外设寄存器

调试器能读整块地址空间，外设寄存器自然也能读。GPIOC 的一组寄存器从 0x4001_1000 开始，按 RM0008 的顺序依次是 CRL、CRH、IDR、ODR、BSRR、BRR、LCKR，前六个都是 32 位（BRR 只有低 16 位有效），LCKR 的偏移在 0x18：

::: terminal title="bash - 直接读 GPIOC 的寄存器组"

```console
(gdb) x/7wx 0x40011000
```

:::

按字读出的 7 个值依次对应 CRL、CRH、IDR、ODR、BSRR、BRR、LCKR，每个寄存器的偏移与复位值都在 RM0008 的 GPIO 寄存器一节里。`debug_demo` 不配置任何外设，也没开 GPIOC 的时钟，CRL 与 CRH 就读出复位状态 `0x4444_4444`——16 个引脚全是浮空输入。换成 [2.4 GPIO](/教程/正文/嵌入式/2_MCU裸机开发/2_4_GPIO) 里把 PC13 配成推挽输出、再拉低的程序，CRH 会变成 `0x4424_4444`：第 23:20 位是 `0010`，MODE13 = 10 表示输出、最大 2 MHz，CNF13 = 00 表示通用推挽；再看 ODR 的 bit 13 是 0，PC13 输出低电平。这两个数放在一起，就是「板载 LED 亮着」在寄存器层面的全部含义。

用数字地址读寄存器需要先查手册，容易记错偏移。第 2 章起代码里会包含 ST 的设备头文件，那时可以直接按名字读：

```console
(gdb) print/x *GPIOC
(gdb) print/x GPIOC->ODR
(gdb) print/x GPIOC->CRH
```

`print/x *GPIOC` 把整个结构体按十六进制打印出来，成员名与手册一致。它依赖 `.elf` 里有 `GPIOC` 这个符号的调试信息，也就是编译时包含了设备头文件并且没有把它优化掉。

IDE 里还有更省事的办法。CMSIS-SVD 是一份 XML 文件，描述某颗芯片所有外设寄存器的地址、位域与访问类型。VS Code 的 Cortex-Debug 插件、CLion 的外设视图都读 SVD，把寄存器按名字列出来，可以直接勾选位、直接改值。ST 的 SVD 文件随 CubeMX 或 `cmsis_device_f1` 一起发布，文件名形如 `STM32F103.svd`。

## 7. 调试输出：半主机与 SWO

### 7.1 半主机

[1.2 MCU、MPU、SoC 与执行环境](/教程/正文/嵌入式/1_平台与工具链/1_2_MCU_MPU_SoC与执行环境) 里那个 `printf` 程序在芯片上没有输出，因为 `_write` 没有把字符送到任何地方。半主机 (semihosting) 提供了一条现成的路：目标程序执行一条特殊的 `BKPT` 指令，调试器截获它，按约定从目标的寄存器或内存里取出参数，代替目标完成打开文件、写字符这些操作。字符最终出现在 OpenOCD 的控制台上，不占用任何引脚。

启用需要三件事：链接时用 `--specs=rdimon.specs` 选带半主机支持的 C 库，程序开头调用 `initialise_monitor_handles()` 建立标准输入输出的句柄，OpenOCD 一侧打开半主机。拿 [1.2 MCU、MPU、SoC 与执行环境](/教程/正文/嵌入式/1_平台与工具链/1_2_MCU_MPU_SoC与执行环境) 里那个 `hello.c` 换一套链接选项，就能看出 rdimon 的代价：

::: terminal title="bash - 同一个 hello.c 的两种链接方式"

```text
$ arm-none-eabi-gcc -mcpu=cortex-m3 -mthumb -O2 hello.c     --specs=nano.specs --specs=nosys.specs -o hello_nosys.elf
$ arm-none-eabi-gcc -mcpu=cortex-m3 -mthumb -O2 hello.c     --specs=rdimon.specs --specs=nano.specs -o hello_semihost.elf
$ arm-none-eabi-size hello_nosys.elf hello_semihost.elf
   text	   data	    bss	    dec	    hex	filename
   5824	    108	    432	   6364	   18dc	hello_nosys.elf
   7704	    400	    604	   8708	   2204	hello_semihost.elf
```

:::

同一份程序，换成 rdimon 之后 `text` 多了 1880 字节，`data` 与 `bss` 也各涨了一截：半主机库把打开文件、读目录、取时间这些操作都实现了，哪怕程序只调用 `printf`，它们的代码也都在库里跟着进来。GDB 一侧的用法是在连接之后打开半主机支持，再让程序跑起来：

```console
(gdb) monitor arm semihosting enable
(gdb) continue
```

目标执行到半主机调用时会停下来等调试器处理，调试器处理完再放行。如果没有连接调试器，同一条 `BKPT` 指令没有接收者，CPU 会把它当成一条无法处理的指令，进入 HardFault。

::: warning 半主机程序离开调试器就跑不了

启用半主机的程序，一旦执行到 `BKPT` 而没有调试器接管，就会进入 HardFault。发布版本必须去掉 `rdimon.specs`，改成经串口或 SWO 输出。

:::

### 7.2 SWO 与 ITM

半主机要求调试器与目标保持会话，字符走调试通道。另一条路是从芯片单独引出一根线，把日志单向送出去，这就是 SWO。F103 上这根线复用 PB3 的 `TRACESWO` 功能。

数据从 ITM (Instrumentation Trace Macrocell) 发出。CMSIS 提供了一行写字符的函数 `ITM_SendChar`，它的实现很朴素：检查 ITM 是否使能、0 号端口是否打开，然后往端口寄存器里写一个字节。程序侧只需要先打开跟踪：

```c
#include "stm32f1xx.h"

static void trace_init(void) {
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    ITM->LAR = 0xC5ACCE55u;
    ITM->TCR = ITM_TCR_ITMENA_Msk;
    ITM->TER = 0x1u;
}

static void trace_puts(const char *s) {
    while (*s != '\0') {
        ITM_SendChar((uint32_t)*s);
        ++s;
    }
}
```

`DEMCR` 的 TRCENA 位是跟踪单元的总开关；LAR 是 ITM 的锁访问寄存器，写入解锁键值；TCR 打开 ITM 本身；TER 打开 0 号端口。把 `trace_init`、`trace_puts` 和一个只调用它们的 `main` 配上 `common/` 的启动文件与链接脚本编成固件，`arm-none-eabi-size` 给出的 `text` 是 496 字节，`bss` 1536 字节（其中大部分是栈的预留）。半主机版本光是库就有 7704 字节 `text`，两者不在一个数量级。

主机一侧要配置 TPIU，把 ITM 的数据从 SWO 引脚按异步串行格式送出来。OpenOCD 用 `tpiu config` 指定输出的去向、协议与位时钟，用 `itm ports on` 打开 ITM 的端口。位时钟取内核时钟：复位后是 HSI 的 8 MHz，[2.3 时钟与复位](/教程/正文/嵌入式/2_MCU裸机开发/2_3_时钟与复位) 把 SYSCLK 提到 72 MHz 之后，这里也要跟着改成 72 MHz，否则主机收到的字节全是错位的。这两条命令的参数顺序随 OpenOCD 版本有过调整，写进自己的 `openocd.cfg` 之前照当前版本的官方手册核对一遍。

作者的建议是：常规日志走串口（[2.7 UART](/教程/正文/嵌入式/2_MCU裸机开发/2_7_UART)），SWO 留给串口被占用、或者日志量大到串口带宽不够的场合。SWO 的优点是只占一根线、不打断程序；缺点是配置依赖调试器与 OpenOCD 版本，换一台机器可能要重新调。

::: tip 调试看门狗程序时先停住看门狗

程序里开了独立看门狗时，CPU 停在断点上的时间里看门狗照常计数，超时就复位，调试根本进行不下去。把 DBGMCU_CR（0xE004_2004）的 `DBG_IWDG_STOP` 位置 1，内核暂停时看门狗也暂停。窗口看门狗对应 `DBG_WWDG_STOP`。2.10 节会用到这一条。

:::

## 8. 连不上调试器时

OpenOCD 在启动时就报错，或者能连上但一 `halt` 就失败，常见原因有三个。

**SWD 引脚被改作他用，或 SWJ_CFG 被写成了 100。** [1.3 芯片手册、开发板资料与引脚复用](/教程/正文/嵌入式/1_平台与工具链/1_3_芯片手册_开发板资料与引脚复用) 讲过，写 100 会同时关闭 JTAG 与 SWD。此时芯片照常运行固件，只是调试端口不再响应。

**程序进入了 Stop 或 Standby 模式。** 这两种低功耗模式会停掉调试接口的时钟，调试器发过去的请求没有响应。唤醒之后能恢复，但自动连接的过程往往等不到唤醒。

**芯片处于读保护状态。** 读保护会禁止调试器读取 Flash 内容，连接时报 `target not examined` 一类错误。

针对前两种原因，有两条恢复路径，彼此独立，任选一条即可。

第一条是让 OpenOCD 在复位的状态下连接。接上 NRST 线，然后在配置里加一行：

```text
reset_config srst_only srst_nogate connect_assert_srst
```

它的意思是用硬件复位线（`srst_only`）、复位期间不屏蔽其他复位源（`srst_nogate`）、在连接之前先拉住复位（`connect_assert_srst`）。这样调试器连接时芯片一直处于复位状态，引脚回到默认功能，调试端口必然可用；连上之后再释放复位。

第二条不需要复位线：把 BOOT0 拨到 1，从系统存储器启动。芯片上电后执行 ST 出厂固化的自举程序，而不是 Flash 里的固件，调试端口自然没有被改过。这时用 OpenOCD 连上去，把 Flash 擦掉或重新烧一份正确的固件，再把 BOOT0 拨回 0。BOOT 引脚的组合见 [1.6 构建、烧录与调试](/教程/正文/嵌入式/1_平台与工具链/1_6_构建_烧录与调试) 的表。

两种方法的共同点是绕开「已经跑起来的固件」。固件把调试口关掉之后，从软件内部没有办法再打开它，只能从复位状态或自举程序介入。读保护属于第三类，需要先解除读保护再连，选项字节的操作见 [6.7 安全启动、映像校验与密钥](/教程/正文/嵌入式/6_质量_安全与交付/6_7_安全启动_映像校验与密钥)。

## 9. 习题

### 9.1 巩固

<Exercise id="61130" :d="1" :w="1">

不接硬件，只用 `.elf` 里的信息回答：

(1) `code/嵌入式/1_平台与工具链/1_7_SWD_JTAG与片上调试/debug_demo/` 里构建出的 `debug_demo.elf`，其 `.bin` 的头 8 个字节是什么？分别对应哪两个符号？

(2) 用 `arm-none-eabi-gdb -batch -ex "info symbol 0x0800016c" debug_demo.elf` 查这个地址落在哪个函数、偏移多少。这个地址在 5.2 节里对应 `main.c` 的哪一行？

(3) 用 `info line main` 与 `info line record_sample` 查出两个函数各自的起始地址。

(4) 用 `x/4wx &g_samples` 打印数组内容，再用 `print *g_samples@4` 打印一次。两条命令的输出形式有何不同？

(5) 本页 4.4 节说 `monitor reset halt` 之后 `pc` 应为 `0x080000f0`，而向量表第 2 个字是 `0x080000f1`。为什么两者差 1？

</Exercise>

<Exercise id="61131" :d="2" :w="2">

本页 2.2 节的引脚表与 1.3 节的 SWJ_CFG 表要合起来看才能回答。

(1) 把 PA13、PA14、PA15、PB3、PB4 五个脚复位后的功能，与 SWJ_CFG 取 `000`、`010`、`100` 时各脚是否被释放，整理成一张 5 行 4 列的表。

(2) 想让 PB3 与 PB4 作普通 GPIO，同时保留 SWD 调试能力，SWJ_CFG 应写什么？写完之后 PA15 能不能用？

(3) 如果 SWJ_CFG 已经写成 `100`，用 ST-LINK 还能不能连上？按本页第 8 节，给出两条恢复路径，并说明各自要求硬件上有什么。

(4) 本页 2.3 节的接线表里，ST-LINK 的第 15 脚接 NRST 是可选的。结合第 8 节的 `reset_config srst_only srst_nogate connect_assert_srst`，说明不接这根线时哪一条恢复路径用不了。

</Exercise>

<Exercise id="61132" :d="2" :w="2">

GDB 的命令分两类：一类只读本地 `.elf`，没有目标也能用；一类必须连上调试器。对下面每条命令，先判断它属于哪一类，再对 `debug_demo.elf` 实际执行一次（不接硬件），把结果填进表里：

| 命令 | 需要目标 | 实际输出 |
| --- | --- | --- |
| `info line main` | | |
| `break main` | | |
| `info registers sp pc` | | |
| `print g_mode` | | |
| `watch g_mode` | | |
| `monitor reset halt` | | |
| `x/4wx &g_samples` | | |
| `bt` | | |
| `load` | | |

(1) 哪几条在没有目标时给出错误信息？把错误原文抄下来。

(2) `break main` 在没有目标时也能成功，说明断点记录在哪里？真正写进 FPB 比较器发生在什么时候？

(3) `watch g_mode` 在没有目标时也报告了 `Hardware watchpoint 1`，这与 (2) 是同一回事吗？

(4) `x/4wx &g_samples` 在没有目标时读出了正确的初值 10、20、30、40。它读的是芯片的 SRAM，还是 `.elf` 里的内容？如果换成 `x/4wx 0x40011000`（GPIOC_CRL），结果会怎样？

</Exercise>

<Exercise id="61133" :d="2" :w="2">

本页 7.1 节说半主机的代价是「库代码全跟着进来」。用 `code/嵌入式/1_平台与工具链/1_2_MCU_MPU_SoC与执行环境/hello.c` 亲手确认一遍：

```text
arm-none-eabi-gcc -mcpu=cortex-m3 -mthumb -O2 hello.c --specs=nano.specs --specs=nosys.specs -o hello_nosys.elf
arm-none-eabi-gcc -mcpu=cortex-m3 -mthumb -O2 hello.c --specs=rdimon.specs --specs=nano.specs -o hello_semihost.elf
arm-none-eabi-size hello_nosys.elf hello_semihost.elf
```

(1) 贴出 `size` 的输出，算出两份的 `text` 差多少字节。

(2) 对两份各运行 `arm-none-eabi-objdump -d hello_semihost.elf | grep -c bkpt`，结果分别是多少？为什么不是 1？把这些 `bkpt` 所在的函数列出来（`objdump -d` 里往上找最近的 `<...>:` 行），说明它们与 7.1 节讲的「目标执行一条特殊的 `BKPT` 指令，调试器截获它」是什么关系。

(3) `hello_semihost.elf` 的 `data` 与 `bss` 也比 `nosys` 版大。这两个段里多出来的是什么？

(4) 把 `rdimon` 版烧进板子但不连调试器，程序执行到第一个 `BKPT` 会怎样？为什么？

(5) 用 `arm-none-eabi-nm hello_semihost.elf | grep -E "initialise|_write$|_read$"` 找出半主机版的 `_write`、`_read` 与 `initialise_monitor_handles`。三者分别是什么符号类型（`T`、`W` 还是 `B`）？`W` 表示什么，为什么它与 `nosys` 版的同名函数抢同一个位置？

</Exercise>

### 9.2 编程

<Exercise id="61134" :d="3" :w="3">

本页 5.1 节说条件断点「仍然只占一个比较器，每次命中后由 GDB 读对象判断条件」。这道题把「每次命中的代价」用能在主机上测的方式表达出来。

1. 把 `debug_demo/main.c` 复制一份到 `code/嵌入式/1_平台与工具链/1_7_SWD_JTAG与片上调试/exercises/`，不改逻辑。
2. 在 `main` 的循环里加一个函数 `uint32_t loop_once(void)`，把一次循环的三条语句包进去，返回 `g_tick_count`。
3. 用 DWT 的 `CYCCNT` 测量「连续调用 `loop_once` 一万次」的周期数，除以一万得到一次循环的基价。写出测量函数 `measure_cycles(uint32_t n)`，读 `DWT->CYCCNT` 前后各一次，注意计数器回绕时用无符号减法。
4. 估算条件断点的代价：每命中一次要停内核、经 SWD 把对象值传到主机、判断、再放行。设一次往返约 100 μs，1000 次就是多少秒？把这个数与本页 4.1 节示例里 `g_tick_count` 的增长速度放在一起，说明为什么条件断点只能偶尔命中，命中频繁时程序会慢到看不出实时行为。
5. 写一份 `.gdbinit`，内容为 `set remote hardware-breakpoint-limit 6`、`set remote hardware-watchpoint-limit 4`、`set pagination off`、`target extended-remote :3333`。用 `arm-none-eabi-gdb -batch -ex "show remote hardware-breakpoint-limit" -x .gdbinit debug_demo.elf` 验证前两条被读到。
6. 有硬件时，先在 `loop_once`、`sum_array`、`record_sample` 三处设满 6 个断点，再在 `main` 里加第 7 个，记录报错原文。没有硬件时说明限制由哪一侧检查（提示：`set remote hardware-breakpoint-limit` 设的值在 GDB 插入断点时被比对），并给出这条限制的硬件来源。

验收：第 3 问给出一次循环的周期数；第 4 问给出 1000 次命中的秒数，并说明它与循环周期相差几个数量级；第 6 问报错形如 `Cannot insert hardware breakpoint 7`。

</Exercise>

<Exercise id="61135" :d="3" :w="3">

本页 6 节讲了如何用 `x/7wx 0x40011000` 直接读 GPIOC，也讲了用 `print/x GPIOC->ODR` 按名字读。写一个工具把两种方式对上。

1. 写 `regdump.c`，在主机上运行，按 RM0008 的偏移打印 GPIOC 七个寄存器的名字与偏移：

```text
$ ./regdump
CRL   0x00  reset 0x44444444
CRH   0x04  reset 0x44444444
IDR   0x08  reset 0x00000000
ODR   0x0C  reset 0x00000000
BSRR  0x10  reset 0x00000000
BRR   0x14  reset 0x00000000
LCKR  0x18  reset 0x00000000
```

2. 6 节给的 GPIOC 基地址是 `0x40011000`。让程序接受一个参数 `--base 0x40011000`，按基地址加偏移算出每个寄存器的绝对地址并打印成 `CRL 0x40011000` 这样的形式。
3. 把 `PC13` 配成推挽输出、输出低电平之后，6 节说 `CRH` 会变成 `0x4424_4444`。写一个函数 `explain_crh(uint32_t crh)`，对第 13 号引脚的 MODE13 与 CNF13 四个位做解码，打印 `MODE13=10 (output, 2 MHz)`、`CNF13=00 (push-pull)`。用 `0x44244444` 与 `0x44444444` 两组输入各测一次。
4. 用 `arm-none-eabi-gdb -batch -ex "x/7wx 0x40011000"` 与 `-ex "print/x *GPIOC"` 分别验证第 1、2 问的地址与偏移是否与 RM0008 一致。没有设备头文件时后者会失败，说明原因（提示：见 6 节最后一段）。

验收：`regdump --base 0x40011000` 输出的七个绝对地址与 6 节 `x/7wx` 一条命令读的七个字一一对应；`explain_crh` 对 `0x44244444` 打印出的第 13 号引脚是输出、推挽。

</Exercise>

<Exercise id="61136" :d="3" :w="2">

本页 3 节的 `emb-debug-bus-access` 讲的是读外设寄存器的副作用。写一个主机程序把这类副作用建模出来。

1. 定义一个结构体描述「一个寄存器 + 读时的副作用」：

```c
struct reg {
    const char *name;
    uint32_t offset;
    uint32_t value;
    uint32_t (*on_read)(struct reg *r);
    void (*on_write)(struct reg *r, uint32_t v);
};
```

2. 用五个寄存器搭一个假的 USART：`SR`（含 RXNE 与 TXE）、`DR`、`BRR`、`CR1`、`CR2`。`DR` 的读操作把 `SR` 的 RXNE 清掉，并把读到的字节返回；`DR` 的写操作把 `SR` 的 TXE 置 0 并在两次写之后置回。
3. 写三个函数：`debug_read(regs, name)` 模拟调试器读，`program_isr_read(regs)` 模拟中断服务程序读，`program_send(regs, byte)` 模拟发送。
4. 写一个测试：程序正在等 `RXNE`，调试器先读了一次 `DR`，然后中断到来。打印中断服务程序读到的 `SR` 与 `DR`，说明它为什么再也不会处理那个字节。
5. 回答：这条副作用对「读 `x/7wx 0x40011000` 读 GPIOC」成立吗？对「读 `0x40013804` 读 USART1_DR」呢？两者的区别是什么？

验收：测试程序打印出「调试器读走 RXNE 之后，ISR 看到的 SR 里 RXNE 为 0」，并在第 5 问里指出区别在于寄存器本身有没有读清标志的语义。

</Exercise>

### 9.3 综合与挑战

<Exercise id="61137" :d="4" :w="3">

本页 7.2 节的 `trace_init`、`trace_puts` 只用到 ITM 的 0 号端口。把它做成一个能在真实目标上用的最小日志模块，而且调试器不在时不能把程序卡死。

1. 写 `itm_log.h` 与 `itm_log.c`，对外只暴露三个函数：

```c
void itm_log_init(uint32_t core_hz);
void itm_log_puts(const char *s);
void itm_log_printf(const char *fmt, ...);
```

2. `itm_log_puts` 每写一个字节前先读一次 ITM 的 0 号端口寄存器：FIFO 有空间时读回 0，可以写入；读回非 0 说明上一次写的还没被取走，此时重试有限次后放弃这一字节，保证没有调试器时函数仍能返回。`itm_log_init` 把 `core_hz` 记下来备用。
3. 在 `main` 里初始化后每 100 ms 打印一行计数，用 1.8 节的 `common/` 启动文件与链接脚本构建。
4. 用 `arm-none-eabi-size` 记录 `text` 大小，与本页 7.2 节那个 496 字节的版本对照（自己的数字会随编译选项与循环写法出入几十字节）。多出来的部分是谁带来的？把 `itm_log_printf` 换成 `itm_log_puts` 再测一次，差值说明 `vsnprintf` 占多少。
5. 写一份 `openocd.cfg`，把 `tpiu config internal :2345 uart off 8000000` 与 `itm ports on` 写进去，并在这个文件里用中文注释说明每条命令的作用。说明波特率参数为什么在 `main` 里改时钟之后要跟着改。

验收：`itm_log_printf` 版本构建成功，`size` 的 `text` 明显超过 496 字节；`openocd.cfg` 能被 `openocd -f` 读入而不报语法错误（没有探针时只验证到启动配置解析阶段）。

</Exercise>

<Exercise id="61138" :d="5" :w="4">

本页 5.2 节用 `watch g_mode` 抓到了 `record_sample` 的越界写。把这件事做成一次完整的缺陷定位演练，不依赖观察点也能得到同样的结论。

1. 复制 `debug_demo` 到 `exercises/watch_lab/`，保留越界缺陷。构建两份：`-O2 -g3` 与 `-O2 -g0`。
2. 对两份分别用 `addr2line` 把 `record_sample` 里那条 `str.w` 的地址映射回源码行。`-g0` 那份能得到行号吗？
3. 用 `arm-none-eabi-nm -n -S` 打印 `g_samples`、`g_mode`、`g_last_sum`、`g_tick_count` 四个符号的地址与大小，验证 `&g_samples[4]` 恰好等于 `g_mode` 的地址。给出计算过程。
4. 只改 `record_sample` 一处（不加快越界检查代码），让缺陷消失但不改动数组大小与函数签名。用 `nm` 与 `objdump -d` 证明改完之后 `*p` 的地址范围被限制在数组内。给出两种改法并比较代价。
5. 写一个主机程序 `replay.c`，把 `g_samples` 与 `g_mode` 放在同一个结构体里、按同样的顺序排列，用同样的 `% 5` 逻辑跑 20 次，打印每次写入的地址偏移与 `g_mode` 的变化。说明为什么这种方式在主机上能复现，而在芯片上必须靠观察点。

验收：第 3 问的等式成立；第 4 问给出两种改法（改取模、加边界钳位），并说明它们各自的运行时开销；`replay` 的输出里能看到下标 4 的那一次把 `g_mode` 改掉。

</Exercise>

<Exercise id="61139" :d="6" :w="4" scope="*">

本页讲了 `openocd -c "program ... verify reset exit"` 与 GDB 两条路径。把烧录这件事做成一个可以在 CI 里跑的检查工具。

1. 写 `flash_check.sh`（或 `.ps1`），接受一个 `.elf` 路径，做四件事：`arm-none-eabi-size` 记录占用；用 `readelf -h` 确认 `Machine: ARM`；用 `objdump -h` 确认 `.isr_vector` 的 VMA 是 `0x08000000` 且大小非零；用 `nm` 确认 `Reset_Handler` 在 Flash 区。
2. 再加一条：`readelf -S` 里 `.data` 的 VMA 在 `0x2000_xxxx`、LMA 在 `0x0800_xxxx`，两者不相等。
3. 对 `debug_demo.elf`、1.8 节去掉 `KEEP` 的产物、把 RAM 写成 `64K` 的产物各跑一次，把输出贴出来，指出每项检查抓住了哪种错误、哪一种错误四项都抓不住。
4. 把脚本接进 CMake：在 `stm32f103_firmware` 生成的 `.elf` 之后加一个 `POST_BUILD` 步骤调用它。写出 `add_custom_command` 的完整片段，并说明为什么不能用 `add_custom_target` 加 `ALL` 来做这件事（提示：目标之间的依赖顺序）。
5. 有硬件时，把第 1 步通过的文件用 `openocd -c "program ... verify reset exit"` 烧进去，记录退出码；故意传一个 `.bin` 而非 `.elf`，记录 OpenOCD 的报错。

验收：三份产物各给出四行检查结论；第 4 问的片段能加进 `firmware.cmake` 并让构建在检查失败时中止。

</Exercise>

## 10. 参考资料

1. OpenOCD，[*GDB and OpenOCD*](https://openocd.org/doc/html/GDB-and-OpenOCD.html)：`target extended-remote`、`monitor` 命令与 GDB 远程协议。
2. OpenOCD，[*Flash Programming*](https://openocd.org/doc/html/Flash-Programming.html)：`program ... verify reset exit` 的用法。
3. OpenOCD，[*Reset Configuration*](https://openocd.org/doc/html/Reset-Configuration.html)：`reset_config` 的各个标志，`connect_assert_srst` 的含义。
4. OpenOCD，[*Architecture and Core Commands*](https://openocd.org/doc/html/Architecture-and-Core-Commands.html)：`arm semihosting`、`tpiu config`、`itm ports` 与 Cortex-M 的断点、观察点命令。
5. GDB，[*Debugging with GDB*](https://sourceware.org/gdb/current/onlinedocs/gdb.html/)：`watch`、`info line`、`x` 命令与远程调试设置。
6. Arm，[*ADIv5: Arm Debug Interface Architecture Specification*](https://developer.arm.com/documentation/ihi0031/latest)：DP、AP 与 AHB-AP 的结构。
7. STMicroelectronics，[*PM0056 Programming manual*, Rev 7](https://www.st.com/resource/en/programming_manual/pm0056-stm32f10xxx20xxx21xxxl1xxxx-cortexm3-programming-manual-stmicroelectronics.pdf)：内核调试寄存器与异常模型。
8. STMicroelectronics，[*RM0008 Reference manual*, Rev 21](https://www.st.com/resource/en/reference_manual/rm0008-stm32f101xx-stm32f102xx-stm32f103xx-stm32f105xx-and-stm32f107xx-advanced-armbased-32bit-mcus-stmicroelectronics.pdf)：第 31 章调试支持（SWJ-DP、FPB、DWT、ITM、DBGMCU），第 27 章 USART（读 DR 清 RXNE）。
9. Arm，[*CMSIS-SVD*](https://arm-software.github.io/CMSIS_5/SVD/html/index.html)：调试器外设视图使用的寄存器描述文件格式。
10. STMicroelectronics，[*UM2237 STM32CubeProgrammer user manual*](https://www.st.com/resource/en/user_manual/um2237-stm32cubeprogrammer-software-description-stmicroelectronics.pdf)：命令行下载与选项字节操作。
