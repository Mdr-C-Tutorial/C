# 2.1 MCU、地址空间与外设

## 1. 简介

STM32 是意法半导体 (STMicroelectronics) 推出的 32 位微控制器系列，采用 Arm Cortex-M 内核，广泛用于消费电子、工业控制、汽车电子、医疗设备和物联网 (Internet of Things, IoT) 等领域。

STM32 系列微控制器有以下特点：

- 集成了多种外设，如模数转换器 (Analog-to-Digital Converter, ADC)、数模转换器 (Digital-to-Analog Converter, DAC)、脉宽调制 (Pulse-Width Modulation, PWM)、串行外设接口 (Serial Peripheral Interface, SPI)、集成电路间总线 (Inter-Integrated Circuit, I2C)、通用异步收发器 (Universal Asynchronous Receiver/Transmitter, UART)、通用串行总线 (Universal Serial Bus, USB)、控制器局域网 (Controller Area Network, CAN) 和通用输入输出 (General-Purpose Input/Output, GPIO) 等。
- 有多种低功耗模式，适合电池供电的便携式设备。
- 支持闪存、SRAM、EEPROM 等多种存储器，也可以外接存储器扩展容量。
- 意法半导体提供 STM32CubeMX（配置和初始化微控制器）、STM32CubeIDE（集成开发环境）、STM32 HAL 库（硬件抽象层库）等开发工具，开发板、教程、示例代码和社区支持也很丰富。

## 2. STM32 选型

> 中国有句古话说得好：“工欲善其事，必先利其器”，所以，在开始我们的嵌入式旅程前，得先挑选一块趁手的开发板。

### 2.1 STM32 分类

<table>
    <thead>
        <tr>
            <th scope="col">CPU 位数</th>
            <th scope="col">内核</th>
            <th scope="col">系列</th>
            <th scope="col">描述</th>
        </tr>
    </thead>
    <tbody>
        <tr>
            <td rowspan="9">32</td>
            <td rowspan="2">Cortex-M0</td>
            <td>STM32-F0</td>
            <td>入门级</td>
        </tr>
        <tr>
            <td>STM32-L0</td>
            <td>低功耗</td>
        </tr>
        <tr>
            <td rowspan="3">Cortex-M3</td>
            <td>STM32-F1</td>
            <td>基础型，主频 72MHz</td>
        </tr>
        <tr>
            <td>STM32-F2</td>
            <td>高性能</td>
        </tr>
        <tr>
            <td>STM32-L1</td>
            <td>低功耗</td>
        </tr>
        <tr>
            <td rowspan="3">Cortex-M4</td>
            <td>STM32-F3</td>
            <td>混合信号</td>
        </tr>
        <tr>
            <td>STM32-F4</td>
            <td>高性能，主频 180MHz</td>
        </tr>
        <tr>
            <td>STM32-L4</td>
            <td>低功耗</td>
        </tr>
        <tr>
            <td>Cortex-M7</td>
            <td>STM32-F7</td>
            <td>高性能</td>
        </tr>
        <tr>
            <td rowspan="5">8</td>
            <td rowspan="5">STM8</td>
        </tr>
        <tr>
            <td>STM8s</td>
            <td>标准系列</td>
        </tr>
        <tr>
            <td>STM8AF</td>
            <td>标准车载</td>
        </tr>
        <tr>
            <td>STM8AL</td>
            <td>低功耗车载</td>
        </tr>
        <tr>
            <td>STM8L</td>
            <td>低功耗</td>
        </tr>
    </tbody>
</table>

### 2.2 STM32 命名方法

![STM32 命名方法图](/images/嵌入式/STM32命名方法图.png)

> 注：摘自《STM32 选型手册》

## 3. 开发导引

### 3.1 分配原理图 IO

在画原理图之前，一般做法是先把引脚分类好，然后才开始画原理图，一般分类如下：

| 引脚分类 | 引脚说明                                                   |
| -------- | ---------------------------------------------------------- |
| 电源     | VBAT、VDD、VSS、VDDA、VSSA、VREF+、VREF-等                 |
| 晶振 IO  | 主晶振、RTC 晶振                                           |
| 下载 IO  | JTAG（JTMS、JTCK、JTDI、JTDO、NJRST）、SWD（SWDIO、SWCLK） |
| BOOT IO  | BOOT0、BOOT1，设置板子启动方式                             |
| 复位 IO  | NRST，用于外部复位                                         |
| GPIO     | 专用总线（I2C、SPI、CAN、USB、FSMC、DCMI 等）、通用 IO     |

### 3.2 关于引脚

> 注：有关这一部分可以在 _Pinouts and pin descriptions_ 小节中找到

![STM32 引脚定义图](/images/嵌入式/STM32引脚定义图.png)

对引脚的解读如下：

| 名称                           | 说明                                                                                              |
| ------------------------------ | ------------------------------------------------------------------------------------------------- |
| 引脚序号（Pins）               | 六种封装（具体要根据需求选择）                                                                    |
| 引脚名称（Pin name）           | 指复位状态下引脚名称                                                                              |
| 引脚类型（Type）               | S：电源<br/>I：输入<br/>I/O：输入/输出                                                            |
| I/O 结构（I/O Level）          | FT：兼容 5V<br/>TTa：支持 3.3V、直接 ADC<br/>B：BOOT 引脚<br/>RST：复位引脚、内部带弱上拉         |
| 主功能（Main function）        | 引脚复位后的功能                                                                                  |
| 复用功能（Alternate function） | I/O 默认的复用功能                                                                                |
| 重映射（Remap）                | I/O 除了默认的复用功能外，还能够通过重映射的方式映射到其他 I/O，增加了 I/O 口功能的灵活性和多样性 |

## 4. 习题

<Exercise id="62211" :d="1" :w="2">

对照本页 2.2 小节的命名方法图，写出下面四个型号的家族、系列、引脚数、Flash 容量、封装与温度范围：

1. `STM32F103C8T6`
2. `STM32F103CBT6`
3. `STM32F407VGT6`
4. `STM32L051K8U6`

然后回答：

(1) 前两个型号只差一个字符，Flash 容量差多少？这个差别会影响链接脚本 `MEMORY` 里的哪一个参数？

(2) `STM32F103C8T6` 的引脚数是 48，但 Blue Pill 板上引出的排针并不是 48 根。说明「引脚数」与「可用 GPIO 数」为什么不是一回事，至少举出三个不能当普通 GPIO 用的引脚。

(3) 命名图里「特定功能」一栏写的是三位数字，`103` 与 `051` 分别对应什么定位？如果一个项目需要硬件浮点运算，这两个系列能不能选？

</Exercise>

<Exercise id="62212" :d="2" :w="3">

写一个主机端程序 `part_decode.c`，从命令行接收若干 STM32 型号，逐行输出解码结果：

```text
$ gcc -std=c17 -Wall -Wextra part_decode.c -o part_decode
$ ./part_decode STM32F103C8T6 STM32F103CBT6 STM32F407VGT6 STM32L051K8U6
STM32F103C8T6: 系列 103, 48 引脚, 64 KB Flash, LQFP, -40~+85
STM32F103CBT6: 系列 103, 48 引脚, 128 KB Flash, LQFP, -40~+85
STM32F407VGT6: 系列 407, 100 引脚, 1024 KB Flash, LQFP, -40~+85
STM32L051K8U6: 系列 051, 32 引脚, 64 KB Flash, UFQFPN, -40~+85
```

要求：

(1) 用一个函数 `int decode(const char *part, char *out, size_t n)` 完成解析，型号长度不是 13 或前缀不是 `STM32` 时返回 0，主函数打印 `无法解析: 型号` 并继续处理下一个参数。

(2) 引脚数、Flash 容量、封装、温度的编码各写成一张静态表或一个 `switch`，不要在解析函数里堆一长串 `if`。

(3) 自行补两个型号验证：`STM32F103RCT6`（64 引脚、256 KB）与 `STM32F103C8T7`（温度范围不同），贴出运行结果。

</Exercise>

<Exercise id="62213" :d="3" :w="3">

按 3.1 小节的分类整理一份引脚清单，写程序检查冲突。清单如下：

```c
static const struct pin_use g_uses[] = {
    {"VDD", "VDD", "电源"}, {"VSS", "VSS", "电源"},
    {"OSC_IN", "PD0", "晶振"}, {"OSC_OUT", "PD1", "晶振"},
    {"SWDIO", "PA13", "下载"}, {"SWCLK", "PA14", "下载"},
    {"BOOT0", "BOOT0", "BOOT"}, {"BOOT1", "PB2", "BOOT"},
    {"NRST", "NRST", "复位"},
    {"LED", "PC13", "GPIO"}, {"KEY", "PA0", "GPIO"},
    {"USART1_TX", "PA9", "GPIO"}, {"USART1_RX", "PA10", "GPIO"},
    {"USB_DM", "PA11", "GPIO"}, {"USB_DP", "PA12", "GPIO"},
    {"CAN_RX", "PA11", "GPIO"}, {"CAN_TX", "PA12", "GPIO"},
};
```

写 `pin_plan.c`，统计每个类别用了几个引脚，再两两比较引脚名，把重复的引脚打印出来。期望输出：

```out
电源 2
晶振 2
下载 2
BOOT 2
复位 1
GPIO 8
合计 17
冲突: PA11 (USB_DM, CAN_RX)
冲突: PA12 (USB_DP, CAN_TX)
```

(1) 这两个冲突对应硬件上的什么约束？查阅 RM0008 第 23 章，说明 USB 与 CAN 能否同时工作，以及计划里为什么要把 CAN 改到重映射引脚。

(2) 给程序加一条检查：属于「下载」类别的引脚如果同时出现在 GPIO 类别里，输出 `警告: 引脚 被复用`。把 PA13 再加一条 `{"LED2", "PA13", "GPIO"}` 验证这条检查。

(3) 冲突检查为什么只能按引脚名比较，不能按类别比较？举一个清单里不冲突但同类的例子。

</Exercise>

<Exercise id="62214" :d="4" :w="4">

第 2.1 节的正文还没有展开存储器映射。用 RM0008 第 3 章的存储器映射表与各外设章的基地址，写一个查询工具 `memmap.c`：从标准输入逐行读一个十六进制地址，输出它落在哪个区域、哪个外设，以及相对外设基地址的偏移。

要求至少覆盖：别名区、Flash、系统存储器、选项字节、SRAM、外设区、Cortex-M3 内核外设区；外设区里至少覆盖 TIM2、TIM3、TIM4、SPI2、USART2、I2C1、PWR、AFIO、GPIOA、GPIOB、GPIOC、ADC1、SPI1、USART1、RCC、FLASH。

用下面九个地址验证，输出必须一致：

```text
$ printf '08000000\n20000000\n4001080C\n40021018\n40013800\n40022000\nE000E010\n1FFFF000\n40021800\n' | ./memmap
0x08000000  Flash
0x20000000  SRAM
0x4001080C  外设  APB2  GPIOA   偏移 0x00C
0x40021018  外设  AHB   RCC     偏移 0x018
0x40013800  外设  APB2  USART1  偏移 0x000
0x40022000  外设  AHB   FLASH   偏移 0x000
0xE000E010  内核外设  NVIC/SCB/SysTick
0x1FFFF000  系统存储器
0x40021800  外设  未知外设
```

(1) `0x40021800` 落在外设区里但不在你列出的任何外设内。查 RM0008 第 3 章的映射表，说明它位于哪两个外设之间、为什么是保留地址，以及往这里写会触发哪一类故障（第 2.10 节会讲）。

(2) 把 GPIOC 的基地址写成 `0x40011100`（比真正的 `0x40011000` 多加了 0x100）。分别查询真正的 GPIOC_ODR 地址 `0x4001100C` 和错误的 `0x4001110C`，写出工具的输出。说明基地址抄错一个数字时，工具报出的「偏移 0x00C」为什么反而是错的，这类错误编译期为什么发现不了、只能靠什么发现。

(3) `0xE000E010` 是 SysTick 的 CTRL 寄存器，它在手册里由 PM0056 而非 RM0008 描述。查 PM0056 给出这个寄存器的三个位名，说明查询工具为什么要把内核外设区单独列一类。

</Exercise>
