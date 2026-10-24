# 2.7 UART

在微控制器的开发与运行维护阶段，获取系统内部的执行轨迹与状态数据是最基础的调试诉求。与具备图形显示子系统的桌面计算机不同，微控制器通常不自带人机交互屏幕。在所有外设通信方式中，通用异步收发器（Universal Asynchronous Receiver/Transmitter, UART/USART）以其极简的引脚开销（仅需 TX、RX 与地线）和极高的通用性，成为嵌入式领域最普及的文本控制台与诊断通信通道。

利用 SWD/JTAG 硬件调试器固然能够断点暂停 CPU 并单步审查内存对象，但该方式必须依靠专用调试探针介入，且会破坏系统的连续运行时间线。相比之下，串口通信不占用调试协议通道，即便在设备完成组装封装并部署到工业现场后，依然能持续通过串行总线输出运行日志，供维护人员回溯系统运行健康度。

在 STM32F103 微控制器中，USART 硬件控制器挂接在不同的总线域：USART1 挂靠于高速的 APB2 总线，而 USART2 与 USART3 则挂接在低速的 APB1 总线上。理清异步串行帧的时序结构与波特率分频算法，掌握轮询与中断收发的寄存器配置，并理解标准库输出重定向（如 `printf` 接入）的底层实现机制，是构建坚固可测嵌入式应用的基础技能。

硬件接线沿用开发环境的标准配置：外部 USB 转串口模块（如 CH340）的 RXD 端连接微控制器的 PA9 引脚（USART1_TX），TXD 端连接 PA10 引脚（USART1_RX），且两块板卡必须严格共地（GND）。由于 PA9 负责输出微控制器发送的数据，因此必须交叉接入外部接收端的输入引脚；若连线发生同名相接（TX 接 TX、RX 接 RX），虽然不会损坏硬件电路，但终端将无法捕获任何字符信号。

## 1. 串口是 MCU 的控制台

在裸机固件开发中，基于串口的文本输出能够以直观的方式呈现程序当前的执行阶段与关键对象数值。相较于专用硬件探针，串行控制台具备鲜明的工程优势：

- **非侵入式连续记录**：调试器在读取内存时需要暂停内核，呈现的是被冻结的瞬态切片；串口输出则在程序全速运行中连续打点，能够真实反映事件触发的先后时序与状态迁移。
- **生命周期覆盖全面**：串口驱动与控制台输出可直接固化在产品固件中，配合日志过滤级别在出厂后持续充当系统诊断窗口，而无需连接昂贵的外部仿真器。
- **物理拓扑精简**：仅需两根信号线与一根参考地线即可建立全双工通信链路，易于经由面板防水航插或调试飞线引出设备机箱。

然而串行控制台并非毫无成本，其主要制约在于**时间延迟与总线占用**。在常见的 115200 bps 波特率下，传输单个 8 位字符（包含起始位与停止位）约需耗时 87 μs；若单次同步打印一行包含 40 个字符的调试信息，CPU 阻塞等待发送完成的总耗时将高达 3.5 ms 以上。对于周期为 1 ms 的高实时性电机控制或数字滤波环路而言，这种毫秒级的同步等待会直接击穿控制节拍。因此，同步串口打印通常严格限制在低频状态变更或系统初始化阶段；高频突发数据的输出则必须交由后台环形发送缓冲区或硬件 DMA 控制器处理。

## 2. 异步串行帧

异步串行通信没有时钟线，收发双方各自按约定的波特率计时。一帧从起始位开始，到停止位结束：

```mermaid
packet
0-0: "起始位"
1-8: "D0～D7（低位先发）"
9-9: "停止位"
```

图中每一格是一个位宽，按发送顺序从 0 开始编号。数据段的 8 位里，D0 在最左边，也就是最先发出去的那一位。

按发送顺序，一帧依次是：

1. 空闲时线路保持高电平。
2. 起始位，一个位宽的低电平，接收方用它把自己的位定时器对齐。
3. 数据位，5～9 位，低位先发。8 位数据时，先发 D0，最后发 D7。
4. 校验位，可选，奇校验或偶校验，也可以没有。
5. 停止位，1、1.5 或 2 个位宽的高电平。

“8N1”是配置的简写：8 个数据位、无校验 (None)、1 个停止位。加上起始位，一个字节在线上占 10 个位宽。115200 bps 时一位宽 1 / 115200 ≈ 8.68 μs，一个字节约 86.8 μs。

发送字符 `'A'`（0x41，二进制 0100 0001）时，数据位从低位开始：1、0、0、0、0、0、1、0。算上起始位（0）和停止位（1），线上依次是：

```
0 1 0 0 0 0 0 1 0 1
起 D0 D1 D2 D3 D4 D5 D6 D7 停
```

<Quiz
  type="single"
  :options="[
    { label: '线上最先出现的是 1', value: 'A' },
    { label: '线上最先出现的是 0，它是起始位', value: 'B' },
    { label: '线上最先出现的是 0，它是 0x41 的最高位', value: 'C' },
    { label: '取决于波特率', value: 'D' }
  ]"
  answer="B"
>

<template #question>

发送 0x41 时，线上最先出现的是 1 还是 0？

</template>

正确答案是 **B**。0x41 的最低位 D0 是 1，但数据位之前还有一个低电平的起始位。判断"最先出现的是哪一位"要从起始位开始数。

</Quiz>

两端各用自己的时钟计时，误差会累积。接收方在每一位的中间采样，帧越长累积的偏差越大，所以总误差必须控制在一个位宽的一小部分之内。RM0008 有一节专门讲接收器对时钟偏差的容限，按数据位长度分档，8 位数据时在 4% 上下；本教程用到的误差远小于这个数。晶振本身的精度通常好于 0.01%，误差主要来自波特率分频除不尽。下一节的计算会看到具体数值。

::: concept emb-uart-frame title="异步串行帧的位序"

UART 的一帧由起始位、数据位、可选校验位和停止位组成。起始位是一个位宽的低电平；数据位低位先发；停止位是高电平。8N1 表示 8 个数据位、无校验、1 个停止位，一个字节在线上占 10 个位宽。空闲状态是高电平，所以没有通信时线路保持高电平。

:::

电平标准也需要区分。MCU 引脚上是 TTL 电平：高电平接近 3.3 V，低电平接近 0 V。老式串口用 RS-232 电平，逻辑 1 是 −3～−15 V，逻辑 0 是 +3～+15 V，两者不能直接相连。USB 转串口模块内部把 USB 协议转成 TTL 串口，所以接到 MCU 上看到的是 TTL。工业现场的 RS-485 是差分信号，用一对双绞线传输，3.5 节讲 Modbus 时会用到。

## 3. 寄存器与波特率

USART 的寄存器不多，常用的六个：

| 偏移 | 名称 | 主要位 |
| --- | --- | --- |
| 0x00 | `SR` | TXE、TC、RXNE、IDLE、ORE、FE、PE |
| 0x04 | `DR` | 发送和接收共用一个数据寄存器 |
| 0x08 | `BRR` | 波特率分频，高 12 位整数、低 4 位小数 |
| 0x0C | `CR1` | UE、M、PCE、TE、RE、TXEIE、TCIE、RXNEIE、IDLEIE |
| 0x10 | `CR2` | STOP，停止位长度 |
| 0x14 | `CR3` | DMAT、DMAR，DMA 请求使能 |

`SR` 里最常用的四位：

::: details USART_SR 的完整位说明

| 位 | 名称 | 含义 |
| --- | --- | --- |
| 7 | TXE | 发送数据寄存器空，可以写下一个字节 |
| 6 | TC | 发送完成，最后一个字节的停止位也发完了 |
| 5 | RXNE | 接收数据寄存器非空，有字节可读 |
| 4 | IDLE | 检测到空闲线，一帧结束后线路持续高电平 |
| 3 | ORE | 溢出错误，新字节到达时旧数据还没读走 |
| 2 | NF | 噪声错误 |
| 1 | FE | 帧错误，没有在预期位置检测到停止位 |
| 0 | PE | 校验错误 |

:::

`BRR` 的算法是：

```
USARTDIV = f_CK / (16 × 波特率)
BRR = round(USARTDIV × 16)
```

`f_CK` 是外设时钟。USART1 用 PCLK2（72 MHz），USART2、USART3 用 PCLK1（36 MHz）。`BRR` 的 16 倍值直接写进寄存器：低 4 位是 `USARTDIV` 的小数部分乘以 16，高 12 位是整数部分。

72 MHz、115200 bps：

```
USARTDIV = 72 000 000 / (16 × 115 200) = 39.0625
BRR = 39.0625 × 16 = 625 = 0x271
```

625 的高 12 位是 39，低 4 位是 1，所以 `BRR = 0x0271`。这样算出来的波特率是 72 000 000 / 625 = 115 200，误差为零。

宿主机程序把几个常用波特率算了一遍：

::: terminal title="bash - BRR 与误差"

```text
$ gcc -std=c17 -Wall -Wextra brr_calc.c -o brr_calc
$ ./brr_calc
Bus     f_CK(Hz)   Target   USARTDIV   BRR (Mant, Frac)     Actual    Error
------ --------- -------- ---------- ------------------ ---------- --------
PCLK2   72000000     9600   468.7500   0x1D4C (468, 12)     9600.0   +0.00%
PCLK2   72000000   115200    39.0625   0x0271 ( 39,  1)   115200.0   +0.00%
PCLK2   72000000   921600     4.8828   0x004E (  4, 14)   923076.9   +0.16%
PCLK1   36000000     9600   234.3750   0x0EA6 (234,  6)     9600.0   +0.00%
PCLK1   36000000   115200    19.5313   0x0139 ( 19,  9)   115016.0   -0.16%
PCLK1   36000000   921600     2.4414   0x0027 (  2,  7)   923076.9   +0.16%
PCLK1*  36000000   115200    19.5313   0x0138 ( 19,  8)   115384.6   +0.16%
```

:::

用 PCLK1 的 USART2 在 115200 上除不尽，`USARTDIV` 算出来是 19.5313，只能取到 19.5 或 19.5625 这样的值，误差 0.16%，远小于接收容限，通信没有问题。RM0008 表 192 给 36 MHz 下 115200 的推荐值就是 19.5，对应 `BRR = 0x0138`；程序里用四舍五入算出 `0x0139`，误差方向相反、大小相同，两个都能用。

代码里不手算这个数，让编译器算：

```c
USART1->BRR = (pclk2_hz + baud / 2u) / baud;
```

`f_CK / baud` 正好等于 `USARTDIV × 16`。加上 `baud / 2` 再除是四舍五入。

::: concept emb-uart-brr title="BRR 由外设时钟与波特率决定"

`USART_BRR` 的值等于 `f_CK / 波特率` 的四舍五入结果，其中 `f_CK` 是 USART 所在总线的时钟：USART1 用 PCLK2，USART2 和 USART3 用 PCLK1。时钟频率改动后 BRR 必须重算，否则波特率按比例偏移。STM32F1 的 BRR 不区分 8 倍过采样模式，这个公式与 RM0008 的 USARTDIV 定义一致。

:::

<Quiz
  type="single"
  :options="[
    { label: 'USART1 用 PCLK2，所以 0x0271 照样正确', value: 'A' },
    { label: '0x0271 对应 PCLK2 的 72 MHz，在 PCLK1 的 36 MHz 上波特率只有一半', value: 'B' },
    { label: 'BRR 是固定值，与时钟无关', value: 'C' },
    { label: '应当把 BRR 改成 0x04E2，即原来的两倍', value: 'D' }
  ]"
  answer="B"
>

<template #question>

把同样的 USART 配置代码从 USART1 复制到 USART2，`BRR` 仍写 `0x0271`，通信会怎样？

</template>

正确答案是 **B**。`BRR` 是时钟与波特率的比值再乘 16，USART1 的 0x0271 是在 72 MHz 下算出来的 39.0625；USART2 的时钟是 36 MHz，同样的分频值只能得到 57600，只有目标的一半。这时应当重新按 36 MHz 算，得到 0x0139（或按 RM0008 表 192 取 0x0138）。

</Quiz>

## 4. 轮询收发

初始化三步：开时钟、配引脚、写寄存器。PA9 要配成复用推挽输出，因为 USART 要接管它；PA10 配成浮空输入（或上拉输入），USART 从它读电平。复用推挽是 CNF = 10、MODE = 11，即 `0xB`；浮空输入是 `0x4`。

```c
void console_init(uint32_t pclk2_hz, uint32_t baud) {
    /* 1. 开启 GPIOA 与 USART1 时钟 */
    RCC->APB2ENR |= RCC_APB2ENR_IOPAEN | RCC_APB2ENR_USART1EN;

    /* 2. PA9（CRH[7:4]）：50 MHz 复用推挽输出（0xB）
     *    PA10（CRH[11:8]）：浮空输入（0x4）
     */
    GPIOA->CRH = (GPIOA->CRH & ~(0xFFu << 4)) | (0x4Bu << 4);

    /* 3. 配置波特率 */
    USART1->BRR = (pclk2_hz + baud / 2u) / baud;

    /* 4. 8 数据位、无校验、1 停止位，使能收发与 USART */
    USART1->CR2 = 0;
    USART1->CR3 = 0;
    USART1->CR1 = USART_CR1_TE | USART_CR1_RE | USART_CR1_UE;
}
```

`CR1` 的 M 位为 0 表示 8 数据位，PCE 为 0 表示无校验，`CR2` 的 STOP 为 00 表示 1 个停止位。这三处都是复位值，显式写 0 是为了让配置一眼可读。UE（USART enable）置位后外设开始工作，初始化时把 TE、RE、UE 三位一次写入即可。

发送和接收各等一个标志：

```c
void uart_putc(char c) {
    while ((USART1->SR & USART_SR_TXE) == 0) {
    }
    USART1->DR = (uint8_t)c;
}

char uart_getc(void) {
    while ((USART1->SR & USART_SR_RXNE) == 0) {
    }
    return (char)(USART1->DR & 0xFFu);
}
```

在底层寄存器机制上，向 `DR` 写入一个字节会自动导致 `TXE`（发送数据寄存器空）标志被硬件清零，外设硬件随后将该字节从数据缓冲区并行加载至内部移位寄存器，一旦缓冲脱空，`TXE` 会被硬件重新置 1；反之，从 `DR` 读取已接收到的字节会自动触发 `RXNE`（接收数据寄存器非空）标志的硬件复位。这两个关键状态位完全反映了硬件状态机的当前物理阶段，通常无需软件手工介入清除。

在编写驱动时，`TXE` 与 `TC`（发送完成）标志位的物理内涵极易产生混淆：

- **TXE（Transmit Data Register Empty）**：表明片内发送数据缓冲寄存器已空，软件已可安全写入下一个待发字节。然而在此时刻，先前写入的前一个字节可能依然在片内移位寄存器中由波特率发生器逐位向外部物理引脚串行吐出。
- **TC（Transmission Complete）**：表明不仅数据缓冲寄存器为空，而且移位寄存器内的最后一个比特（包括停止位）也已完整离开芯片管脚，物理总线已彻底回归高电平空闲态。

在执行连续流式发送时，程序仅需轮询等待 `TXE` 即可，这使得前后字节之间能够紧密衔接，避免波特率时隙产生不必要的物理空隙。唯有当系统需要“在当前报文彻底脱离总线后立即切换硬件工作模式”时，才必须严格轮询等待 `TC` 标志。例如：在通过控制 GPIO 驱动 RS-485 收发芯片（如 MAX485）的 DE/RE 差分方向使能端时，若在仅检测到 `TXE` 时就匆忙拉低引脚切回接收模式，移位寄存器中正在发送的报文尾部字节（或最后一个停止位）将被硬件强行截断，导致对端通信节点报出帧校验错误。类似地，在进入 Stop 低功耗休眠模式或关闭外设总线时钟前，调用 `uart_flush()` 等待 `TC` 置位同样是确保最后一帧报文完整交付的必要防护措施：

```c
void uart_flush(void) {
    while ((USART1->SR & USART_SR_TC) == 0) {
    }
}
```

<Quiz
  type="single"
  :options="[
    { label: 'TXE，因为它表示发送完成', value: 'A' },
    { label: 'TC，因为它表示最后一个字节的停止位也发完了', value: 'B' },
    { label: 'RXNE，因为它表示数据已经被对方接收', value: 'C' },
    { label: '两个都不行，只能加一个固定延时', value: 'D' }
  ]"
  answer="B"
>

<template #question>

写 RS-485 的程序发完最后一个字节要立刻拉低 DE 引脚释放总线。这时应该等哪个标志？

</template>

正确答案是 **B**。TXE 只说明发送数据寄存器可以接收下一个字节，此时上一个字节还在移位寄存器里往外发，提前切换方向会把最后几个位截断。TC 在停止位发完时才置位，等它才安全。这也是 [3.5 CAN 与 Modbus](/教程/正文/嵌入式/3_驱动_通信与中间件/3_5_CAN与Modbus) 里 RS-485 驱动的固定写法。

</Quiz>

回显程序把收到的字节原样发出去：

```c
int main(void) {
    clock_init_72mhz();
    console_init(72000000u, 115200u);

    uart_puts("USART1 echo ready (115200 8N1)\n");

    for (;;) {
        char c = uart_getc();
        if (c == '\r') {
            uart_putc('\r');
            uart_putc('\n');
        } else {
            uart_putc(c);
        }
    }
}
```

电脑端用任意串口终端都能看到。PuTTY 选 Serial、COM 口号、115200、8N1；有 Python 和 pyserial 时也可以直接用它的命令行终端：

::: terminal title="PowerShell - 打开串口终端"

```text
PS> python -m serial.tools.miniterm COM5 115200
```

:::

打开后应当先看到程序启动时打印的 `USART1 echo ready (115200 8N1)`，随后敲进去的每个字符都会原样回显；按回车，光标换到下一行开头，因为程序把 `\r` 补成了 `\r\n`。如果终端里一个字都没有，先查 COM 口号和 TX/RX 是否交叉，再查波特率，具体见第 7 节。

构建结果：

::: terminal title="bash - 构建 echo"

```text
$ arm-none-eabi-gcc -std=c11 -mcpu=cortex-m3 -mthumb -O2 -g -Wall -Wextra \
    -ffunction-sections -fdata-sections -DSTM32F103xB -I. \
    -nostartfiles --specs=nano.specs --specs=nosys.specs -Wl,--gc-sections \
    -T stm32f103c8.ld clock.c console.c echo/main.c startup_stm32f103.c \
    -o echo.elf
$ arm-none-eabi-size echo.elf
   text	   data	    bss	    dec	    hex	filename
    728	      4	   1540	   2272	    8e0	echo.elf
```

:::

728 字节里向量表占 236 字节，启动代码 76 字节，时钟初始化 112 字节，`console_init`、`uart_putc`、`uart_getc`、`uart_puts` 四个函数加起来 160 字节。没有 `printf` 时整套串口输出不到 200 字节。

## 5. 把 printf 接到串口

newlib 的 `printf` 最终把格式化好的字符通过 `_write` 交给"系统"输出。裸机上没有系统，`_write` 是一个空实现（`--specs=nosys.specs` 提供的），返回 −1，什么也不发。自己实现 `_write`，把它接到 `uart_putc` 上，`printf` 就有输出了：

```c
#include "console.h"

int _write(int fd, const char *buf, int len) {
    (void)fd;
    for (int i = 0; i < len; ++i) {
        if (buf[i] == '\n') {
            uart_putc('\r');
        }
        uart_putc(buf[i]);
    }
    return len;
}
```

`fd` 是文件描述符，`printf` 走的是 1（标准输出），这里不区分。换行要补一个 `\r`，否则终端上光标只往下移不回到行首，输出会呈阶梯状。

不写 `_write`、也不加 `--specs=nosys.specs` 时，链接器会报一长串未定义引用：

::: terminal title="bash - 缺少系统调用实现"

```text
$ arm-none-eabi-gcc -std=c11 -mcpu=cortex-m3 -mthumb -O2 -g -Wall -Wextra \
    -ffunction-sections -fdata-sections -DSTM32F103xB -I. \
    -nostartfiles --specs=nano.specs -Wl,--gc-sections \
    -T stm32f103c8.ld clock.c console.c printf_demo/main.c startup_stm32f103.c \
    -o printf_demo.elf
closer.c:(.text._close_r+0xc): undefined reference to `_close'
lseekr.c:(.text._lseek_r+0x10): undefined reference to `_lseek'
readr.c:(.text._read_r+0x10): undefined reference to `_read'
writer.c:(.text._write_r+0x10): undefined reference to `_write'
fstatr.c:(.text._fstat_r+0xe): undefined reference to `_fstat'
isattyr.c:(.text._isatty_r+0xc): undefined reference to `_isatty'
sbrkr.c:(.text._sbrk_r+0xc): undefined reference to `_sbrk'
collect2.exe: error: ld returned 1 exit status
```

:::

`_sbrk` 是堆管理，`printf` 里的某些路径会调用 `malloc`。只想重定向输出时，`--specs=nosys.specs` 给其余几个提供空实现、自己只写 `_write` 是最省事的组合。

加入 `printf` 的代价可以量化：

::: terminal title="bash - printf 前后的体积"

```text
$ arm-none-eabi-size echo.elf printf_demo.elf
   text	   data	    bss	    dec	    hex	filename
    728	      4	   1540	   2272	    8e0	echo.elf
   4472	     96	   1872	   6440	   1928	printf_demo.elf
```

:::

`text` 从 728 涨到 4472，多了约 3.7 KB。`printf_demo` 里用了两处 `printf`，包含 `%lu` 和 `%ld`，整数格式化已经占了大头。

浮点更贵。newlib-nano 的 `printf` 默认把浮点转换整个裁掉，写 `%.2f` 时链接能通过，运行时那个位置不会出现数值。把浮点格式化拉回来要加链接选项 `-u _printf_float`，`-u` 强制把符号当作未定义引用，链接器就会去库里找它。用同一个只打印一行 `printf("temp = %.2f C\n", 25.375)` 的程序，只改链接选项，两份固件的大小差了一个数量级：

::: terminal title="bash - 浮点格式化的体积"

```text
$ arm-none-eabi-size pf_noflag.elf pf_flag.elf
   text	   data	    bss	    dec	    hex	filename
   4428	     96	   1872	   6396	   18fc	pf_noflag.elf
  14460	    460	   1876	  16796	   419c	pf_flag.elf
```

:::

`text` 段从 4428 激增到 14460，净增近 10 KB，同时 `data` 段也增加了 364 字节，用于保存浮点运算与转换的常量表。对于片上 Flash 仅有 64 KB 的微控制器而言，为单行调试打印付出如此代价显然是不经济的。

在片上存储资源紧凑的工程实践中，应当审慎评估是否引入标准库完整的 `printf`。手写一个轻量级、仅支持 `%d`、`%x`、`%s`、`%u` 的字符格式化函数，通常仅需一两百字节的 Flash 空间，却足以覆盖调试与协议输出的绝大部分场景。若必须输出带有小数精度的传感器物理量，工程上普遍采用定点化方案——在业务层将数值统一放大存储为整数（例如以毫摄氏度 `temp_mdeg` 为单位），在输出时利用整数除法与取模分别输出整数与小数部分，中间手动插入小数点，示例程序 `printf_demo` 便是采用了这一成熟做法。

## 6. 中断接收与命令行

轮询接收的问题和 [2.5 中断与 NVIC](/教程/正文/嵌入式/2_MCU裸机开发/2_5_中断与NVIC) 里的按键一样：主循环在忙别的事时，串口来的字节没人读，`DR` 里的数据被下一个字节覆盖，触发溢出错误 ORE。接收应当交给中断。

打开 `RXNEIE` 之后，每收到一个字节就进一次 `USART1_IRQHandler`。中断里把字符收进一行缓冲，遇到回车就置一个"行就绪"标志：

```c
void USART1_IRQHandler(void) {
    uint32_t sr = USART1->SR;
    if ((sr & (USART_SR_RXNE | USART_SR_ORE)) != 0) {
        char c = (char)(USART1->DR & 0xFFu); /* 读 SR 后读 DR 同时清除 RXNE 与 ORE */
        if (g_line_ready != 0) {
            return; /* 上一行尚未由主循环处理完，暂丢弃新字符 */
        }
        if (c == '\r' || c == '\n') {
            g_line_buf[g_line_len] = '\0';
            g_line_len = 0;
            g_line_ready = 1;
        } else if (g_line_len + 1u < LINE_BUF_SIZE) {
            g_line_buf[g_line_len++] = c;
        }
    }
}
```

先读 `SR` 再读 `DR` 是一个必须遵守的顺序。只读 `DR` 能清 `RXNE`，但清不掉 `ORE`；`ORE` 不清，后续字节会继续被丢。先读 `SR` 再读 `DR` 把两个都清掉。

主循环看到 `g_line_ready` 后解析这一行：

```c
for (;;) {
    if (g_line_ready != 0) {
        cli_handle_line(g_line_buf, &st, reply, sizeof reply);
        led_set(st.led_on);
        g_line_ready = 0;
        if (reply[0] != '\0') {
            uart_puts(reply);
        }
        uart_puts("> ");
    }
}
```

解析函数不碰硬件，单独放在 `cli_parse.c` 里：

```c
void cli_handle_line(const char *line, cli_state *st, char *reply, size_t reply_cap) {
    while (*line == ' ' || *line == '\t') {
        ++line;
    }
    if (*line == '\0') {
        reply[0] = '\0';
        return;
    }

    st->rx_lines++;

    if (strcmp(line, "led on") == 0) {
        st->led_on = 1;
        snprintf(reply, reply_cap, "OK: LED is ON\n");
    } else if (strcmp(line, "led off") == 0) {
        st->led_on = 0;
        snprintf(reply, reply_cap, "OK: LED is OFF\n");
    } else if (strcmp(line, "status") == 0) {
        snprintf(reply, reply_cap, "LED=%s, lines=%lu\n",
                 st->led_on ? "ON" : "OFF",
                 (unsigned long)st->rx_lines);
    } else {
        snprintf(reply, reply_cap, "ERR: unknown command '%s'\n", line);
    }
}
```

把解析和硬件分开的好处是它可以在 PC 上测试。`host/cli_parse_test.c` 喂进几条命令行，打印回复：

::: terminal title="bash - 在 PC 上测试命令解析"

```text
$ gcc -std=c17 -Wall -Wextra cli_parse_test.c cli_parse.c -o cli_test
$ ./cli_test
> led on
OK: LED is ON
> status
LED=ON, lines=2
> led off
OK: LED is OFF
> status
LED=OFF, lines=4
> reboot
ERR: unknown command 'reboot'
```

:::

去掉串口和 LED 之后，这段逻辑只依赖标准库，用 MinGW 编译运行就能验证。串口终端里的实际交互需要硬件。

命令行程序的体积：

::: terminal title="bash - 构建 cli"

```text
$ arm-none-eabi-size cli.elf
   text	   data	    bss	    dec	    hex	filename
   3656	     84	   1628	   5368	   14f8	cli.elf
```

:::

比 `echo` 大，因为用了 `snprintf`，链接器把整数格式化的那部分拉了进来。

这个命令行只够演示。行缓冲固定 64 字节，溢出时新字符直接丢弃；没有回显和退格，敲错只能重来；只支持整行命令，不支持参数解析和转义。更完整的做法是把接收放进环形缓冲区，键处理放进一个状态机，那正是 [3.4 数据帧、环形缓冲区与状态机](/教程/正文/嵌入式/3_驱动_通信与中间件/3_4_数据帧_环形缓冲区与状态机) 的内容。

## 7. 写错了会怎样

**波特率算错**。程序里调用 `clock_init_72mhz()` 之后忘记改 `console_init` 的参数，仍然按 8 MHz 传入，或者反过来，时钟还是 HSI 的 8 MHz 却按 72 MHz 算 `BRR`。后一种情况下 `BRR` 停在 0x0271（625），实际波特率变成 8 000 000 / 625 = 12 800，从 115200 掉到不足九分之一，终端收到的全是乱码。判断方法很简单：把终端也设成 12800 再看，如果能读出正常文字，问题就出在时钟上。

**TX/RX 接反或没共地**。MCU 的 TX 输出接对方的 TX，两边都在发，没人收；或者只接了 TX/RX 没接 GND，电平没有共同的参考点，收到的字节会周期性出错。这两种情况下终端里完全没有输出，和"程序没跑起来"很难区分，先量一下 TX 引脚在启动时有没有电平变化。

**开了中断却没写处理函数**。置了 `RXNEIE`，NVIC 也打开了，但函数名写成 `USART1_IRQhandler` 或者干脆没写，链接一切正常，芯片上按第一个字符就停住。这和 [2.5 中断与 NVIC](/教程/正文/嵌入式/2_MCU裸机开发/2_5_中断与NVIC) 里处理函数名拼错是同一个问题：向量表里的 `USART1_IRQHandler` 还是 `Default_Handler`，中断一进就死循环。`arm-none-eabi-nm` 查一下这个符号的地址与 `Default_Handler` 是否相同就能确认。

::: compare left-title="只读 DR 清 RXNE" right-title="读 SR 再读 DR 清 RXNE 和 ORE"

```c
uint8_t c = USART1->DR;
/* ORE 仍然置位，后续字节被丢弃 */
```

---

```c
uint32_t sr = USART1->SR;
if ((sr & (USART_SR_RXNE | USART_SR_ORE)) != 0) {
    uint8_t c = USART1->DR;
}
```

:::

左栏在读走一个字节之后 `ORE` 还挂着，下一个字节到达时硬件看到接收寄存器已经空、但错误标志没清，行为取决于具体时序；右栏先读状态再读数据，两个标志一并清掉，这是 RM0008 规定的清除序列。

## 8. HAL 对照

HAL 把串口封装成句柄：

```c
UART_HandleTypeDef huart1;
huart1.Instance = USART1;
huart1.Init.BaudRate = 115200;
huart1.Init.WordLength = UART_WORDLENGTH_8B;
huart1.Init.StopBits = UART_STOPBITS_1;
huart1.Init.Parity = UART_PARITY_NONE;
huart1.Init.Mode = UART_MODE_TX_RX;
HAL_UART_Init(&huart1);

HAL_UART_Transmit(&huart1, (uint8_t *)"hello\n", 6, 100);
HAL_UART_Receive_IT(&huart1, &rx_byte, 1);
```

`HAL_UART_Transmit` 的最后一个参数是超时毫秒数，内部轮询 `TXE`，超时返回 `HAL_TIMEOUT`。`HAL_UART_Receive_IT` 只启动一次接收，收到一个字节后调用 `HAL_UART_RxCpltCallback`，回调里要再调一次 `HAL_UART_Receive_IT` 才能继续收。

| 寄存器写法 | HAL 写法 |
| --- | --- |
| `USART1->BRR = f / baud` | `huart1.Init.BaudRate`，`HAL_UART_Init` 内部算 |
| `CR1 \|= TE \| RE \| UE` | `huart1.Init.Mode` |
| 轮询等 TXE | `HAL_UART_Transmit` |
| 轮询等 RXNE | `HAL_UART_Receive` |
| `RXNEIE` + `USART1_IRQHandler` | `HAL_UART_Receive_IT` + `HAL_UART_RxCpltCallback` |
| 自己的 `_write` | CubeMX 生成的 `syscalls.c` 里的 `__io_putchar` |

CubeMX 生成的 `syscalls.c` 通过 `__io_putchar` 重定向：`_write` 逐字节调用它，用户只需要实现这一个函数。这和本节直接写 `_write` 是一回事，只是中间多一层。

## 9. 习题

### 9.1 巩固

<Exercise id="60258" :d="1" :w="2">

8N1、无校验、115200 bps，线上依次传送三个字节 `0x00`、`0xAA`、`0x0D`。画出每一个字节的位序列（起始位、8 个数据位、停止位，低位先发），并回答：

1. 每个字节占几个位宽？传输三个字节一共多少微秒？
2. 发送 `0xAA` 时，线上连续出现的低电平最长是多少个位宽？它会不会被接收方误认成起始位？
3. 把校验位改成偶校验，`0xAA` 的校验位应该是 0 还是 1？`0x0D` 呢？

</Exercise>

<Exercise id="60259" :d="2" :w="2">

某工程把 `clock_init_72mhz()` 换成了 HSI 8 MHz 的初始化，但 `console_init` 的调用仍是 `console_init(72000000u, 115200u)`。

1. 算出这时实际的波特率，终端设成多少才能读出正常文字。
2. 反过来：时钟仍是 72 MHz，但参数误传成 `console_init(36000000u, 115200u)`，也就是照抄了 USART2 的时钟。算出写入的 `BRR`、实际波特率和它相对 115200 的倍数，说明终端上会看到什么。
3. 用 `brr_calc.c` 的写法，算出 72 MHz 下 250000 bps、500000 bps 的 `BRR` 和相对误差，说明这两个速率在 USART1 上能否达到。
4. USART1 用 921600 bps，`BRR` 的高 12 位和低 4 位各是多少？实际波特率误差多大？

</Exercise>

<Exercise id="60260" :d="2" :w="2">

下面是四个发送/接收片段，判断各自的问题并改正：

```c
/* A */
void putc_a(char c) {
    USART1->DR = (uint8_t)c;
    while ((USART1->SR & USART_SR_TXE) == 0) {
    }
}

/* B */
void send_str_b(const char *s) {
    while (*s != '\0') {
        while ((USART1->SR & USART_SR_TXE) == 0) {
        }
        USART1->DR = (uint8_t)*s++;
    }
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_8, GPIO_PIN_RESET); /* RS-485 切回接收 */
}

/* C */
char getc_c(void) {
    return (char)USART1->DR;
}

/* D */
void USART1_IRQHandler(void) {
    char c = (char)USART1->DR; /* 先把数据读走 */
    if ((USART1->SR & USART_SR_RXNE) != 0) {
        g_line_buf[g_line_len++] = c;
    }
}
```

1. A 错在哪里？写出正确的等待顺序。
2. B 在什么情况下会截断最后一个字节？应该换成哪个标志？
3. C 在什么情况下返回的是上一次的旧数据？
4. D 的判断为什么永远不成立？把读 `DR` 移到判断之后，`ORE` 又该怎么清？

</Exercise>

### 9.2 编程

<Exercise id="60261" :d="2" :w="2">

`console.h` 声明了 `uart_puts`、`uart_write` 和 `uart_flush`，但正文只给出了 `uart_putc`、`uart_getc` 与 `uart_flush` 的实现。先不要看 `code/嵌入式/2_MCU裸机开发/2_7_UART/console.c` 里的现成版本，自己写一遍再对照。

1. 实现 `void uart_puts(const char *s)` 和 `void uart_write(const void *buf, size_t len)`，都基于 `uart_putc`。前者遇到 `\n` 要先补一个 `\r`，与第 5 节 `_write` 的做法一致；后者按字节原样写出。`uart_puts` 能不能也走 `uart_write`，说明各自的取舍。
2. `uart_flush` 等的是 TC 而不是 TXE，写出它在 `uart_write` 之后单独调用的场景：关闭 USART、进入低功耗、切换 RS-485 方向。各举一行代码。
3. 为这三个函数写一份宿主机测试：把 `uart_putc` 改成往内存缓冲区里写，验证 `uart_puts("AB")` 后紧接 `uart_puts("CD")` 得到的字节序列是 `ABCD`，且 `uart_write(buf, 0)` 不写任何字节。
4. 用 HAL 写同一组功能：阻塞发送用 `HAL_UART_Transmit`，中断发送用 `HAL_UART_Transmit_IT`。HAL 没有 `uart_flush` 这样的函数，写出轮询 `huart1.gState` 直到回到 `HAL_UART_STATE_READY` 的版本，说明它等价于等哪个硬件标志。

验收：测试程序用 `gcc -std=c17 -Wall -Wextra` 无警告编译并输出 `ABCD`。

</Exercise>

<Exercise id="60262" :d="3" :w="3">

基于 `code/嵌入式/2_MCU裸机开发/2_7_UART/exercises/line_edit_starter.c`，补全 `line_editor_feed`，让命令行支持退格：

1. 收到 `\r` 或 `\n` 时收尾，返回 1；此时 `buf` 以 `\0` 结尾。
2. 收到 `\b` 或 `0x7F`（DEL）时删掉最后一个字符，并往 `echo_out` 写 `"\b \b"`；缓冲区已经为空时什么也不删，`echo_out` 写空串。
3. 普通可打印字符追加到缓冲区并把该字符写进 `echo_out`；缓冲区满时丢弃，`echo_out` 写空串。
4. `echo_out` 要保证以 `\0` 结尾，且不超过 `echo_cap`。

程序里的测试流是 `"led o\x7fn\rstatus\r"`：先打 `led o`，按一次退格删掉 `o`，再打 `n`，回车。因此第一行应当是 `LINE: "led n"`，第二行是 `LINE: "status"`。验证删掉的确实是最后输入的字符，而不是从头重来。再把流换成 `"led o\x7f \x7fon\r"`（退格删 `o`、补空格、再退格删空格、补 `on`），确认最终得到 `LINE: "led on"`，空格也被正确删掉了。最后用 `"\x7f\x7f"` 验证空缓冲区上连按退格不会越界，用 `-fsanitize=address` 跑一遍。

</Exercise>

<Exercise id="60263" :d="3" :w="3">

给命令行加参数解析。现在 `cli_handle_line` 只认整行固定字符串，要支持：

```text
led on
led off
led blink 100
led blink 1000
status
```

1. 在 `cli_parse.c` 里用 `strtok` 或手写扫描拆出最多三个词，`strcmp` 比较命令词。`cli_state` 增加一个 `uint32_t period_ms` 字段，初始 500。
2. `led blink N` 把 LED 翻转周期设成 N 毫秒，`N` 取 50～5000，超出范围回 `ERR: period out of range`。非数字回 `ERR: bad period`。
3. 回复格式统一，`status` 增加一行当前周期，例如 `LED=ON, period=500 ms, lines=7`。
4. 扩展 `host/cli_parse_test.c`，覆盖正常、越界、非数字、参数缺失（`led blink`）、未知命令五种情况，把输出贴出来。

验收：宿主机上 `gcc -std=c17 -Wall -Wextra cli_parse_test.c cli_parse.c -o cli_test` 无警告，五组输出的首行与预期一致。

</Exercise>

<Exercise id="60264" :d="3" :w="3">

把接收改成环形缓冲区，去掉“上一行未处理完就丢字符”的限制。

1. 在 `USART1_IRQHandler` 里把收到的字节写进一个 64 字节的环形缓冲区，头尾各用一个 `volatile uint8_t` 或 `uint16_t` 下标，中断只写头、主循环只读尾。
2. 队列满时丢弃新字节并置位一个 `g_rx_overrun` 计数；主循环在回复里带上这个计数，方便定位丢字。
3. 主循环从队列里逐字节取出，用 `line_editor` 或状态数组拼行，遇到 `\r` 时交给 `cli_handle_line`。
4. 在宿主机上写测试：向队列灌 80 个字节，验证前 64 个被保留、`g_rx_overrun` 等于 16，再取空后队列恢复可用。

说明为什么这个队列在单生产者（中断）单消费者（主循环）的模型下不需要关中断。

</Exercise>

### 9.3 综合与挑战

<Exercise id="60265" :d="4" :w="4">

写一个不依赖 `printf` 的轻量格式化函数，覆盖调试输出九成的需求：

```c
int mini_format(char *out, size_t cap, const char *fmt, ...);
```

支持 `%d`、`%u`、`%x`、`%s`、`%c`、`%%`，可选宽度（`%4d`、`%-6s`）和 `%04x` 补零，返回写入的字符数（不含 `\0`）。

1. 实现整数转字符串的辅助函数，处理 `INT_MIN`（`-2147483648` 取负会溢出）。
2. 缓冲区不够时截断并保证 `\0` 结尾，返回值为“本该写入的长度”，与 `snprintf` 一致。
3. 用同一组格式串把 `mini_format` 与 `snprintf` 的输出逐字节比较，至少覆盖 20 组用例：负数、0、`UINT_MAX`、宽度与左对齐、补零、空字符串。
4. 用 `arm-none-eabi-size` 比较把它接到 `uart_puts` 后与 `printf_demo` 的 `.text` 大小，说明省了多少字节。

验收：20 组用例全部一致；固件 `.text` 比用了 `printf` 的版本小 3 KB 以上。

</Exercise>

<Exercise id="60266" :d="4" :w="4">

做一个串口协议解析器，帧格式为 `0xAA`、长度（1 字节）、数据（1～32 字节）、校验和（前面所有字节之和的低 8 位）。

1. 写 `frame_feed(uint8_t byte)`，用状态机逐字节接收，收到完整且校验正确的一帧时返回 1，通过出参给出数据指针与长度。
2. 帧头出现在数据中时解析器不能失步。给出你的处理办法（长度字段约束、超时重置或转义），并说明在什么情况下这个办法也会失效。
3. 写一个灌数据的宿主机测试，输入流里包含：正常帧、校验错误的帧、长度字段为 0 的帧、一帧中间插入 0xAA 的帧。验证解析器给出正确的帧数和错误的帧数。
4. 把它接到第 6 节的命令行上：`led on` 等命令改为装进帧传输，PC 端用 Python 或另一个 C 程序生成帧。

验收：四类输入的解析结果与预期一致；正常帧 100% 被识别。

</Exercise>

<Exercise id="60267" :d="5" :w="4" scope="*">

写一个串口日志模块，要求主循环里调用 `log_printf` 不阻塞在波特率上。

1. 提供 `void log_printf(const char *fmt, ...)`，内部格式化后放进一个 256 字节的环形发送队列，立即返回。
2. 打开 `TXEIE`，在 `USART1_IRQHandler` 里从队列取一个字节写 `DR`，队列空时关掉 `TXEIE`，避免中断空转。用 `mini_format` 或 `snprintf`。
3. 队列满时调用方有两种选择：丢弃新日志，或者等待。给出一个 `log_drop_count` 统计并说明各自的适用场景。
4. 在 115200 bps 下测：连续打印 100 行、每行 40 字符，比较同步 `uart_puts` 与队列版对主循环一轮时间的影响（用 DWT 计时）。主循环里跑一个 1 kHz 的控制任务，比较两种情况下任务的抖动。
5. 说明为什么不能用 `TC` 中断代替 `TXE` 中断驱动队列。

验收：队列版主循环单轮时间不随日志长度增长；抖动从毫秒级降到几十微秒。

</Exercise>

## 10. 参考资料

1. STMicroelectronics, [*RM0008 Reference manual: STM32F101xx, STM32F102xx, STM32F103xx, STM32F105xx and STM32F107xx advanced Arm-based 32-bit MCUs*, Rev 21](https://www.st.com/resource/en/reference_manual/rm0008-stm32f101xx-stm32f102xx-stm32f103xx-stm32f105xx-and-stm32f107xx-advanced-armbased-32bit-mcus-stmicroelectronics.pdf)：第 27 章 Universal synchronous asynchronous receiver transmitter（27.3.4 波特率生成、表 192、27.3.5 接收容限）。
2. newlib, [*Stubs*](https://sourceware.org/newlib/libc.html#Stubs)：`_write`、`_sbrk`、`_read` 等系统调用的用途与实现约定。
3. cpq, [*Bare-metal programming guide*](https://github.com/cpq/bare-metal-programming-guide)：UART 与 `printf` 重定向一节。
4. Analog Devices, [*UART: A Hardware Communication Protocol*](https://www.analog.com/en/resources/analog-dialogue/articles/uart-a-hardware-communication-protocol.html)：异步串行帧、起始位与停止位、波特率误差。
5. STMicroelectronics, [*STM32F1 HAL driver*](https://github.com/STMicroelectronics/stm32f1xx-hal-driver)：`stm32f1xx_hal_uart.c`。
6. pyserial, [*miniterm*](https://pyserial.readthedocs.io/en/latest/tools.html#module-serial.tools.miniterm)：命令行串口终端。
