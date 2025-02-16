# 初识 STM32

## 1. 简介

**STM32（STMicroelectronics Microcontroller）** 是由意法半导体（STMicroelectronics）公司开发的一系列 32 位微控制器。
这些微控制器广泛应用于各种嵌入式系统，包括消费电子、工业控制、汽车电子、医疗设备、物联网（IoT）等领域。

STM32 系列微控制器具有以下特点：

1. **高性能**：STM32 微控制器采用高性能的 ARM Cortex-M 内核，提供高处理速度和低功耗。
2. **丰富的外设**
   ：STM32 系列微控制器集成了多种外设，如 ADC（模数转换器）、DAC（数模转换器）、PWM（脉宽调制）、SPI（串行外设接口）、I2C（集成电路间总线）、UART（通用异步收发传输器）、USB（通用串行总线）、CAN（控制器局域网）、GPIO（通用输入输出）等，满足各种应用需求。
3. **低功耗**：STM32 系列微控制器具有多种低功耗模式，适用于电池供电的便携式设备。
4. **可扩展性**：STM32 系列微控制器支持多种存储器选项，包括闪存、SRAM、EEPROM 等，并且可以通过外部存储器扩展存储容量。
5. **开发工具**：意法半导体提供了丰富的开发工具，包括 STM32CubeMX（用于配置和初始化微控制器）、STM32CubeIDE（集成开发环境）、STM32
   HAL 库（硬件抽象层库）等，方便开发者进行软件开发。
6. **生态系统**：STM32 微控制器拥有庞大的生态系统，包括丰富的开发板、教程、示例代码、社区支持等，便于开发者进行学习和开发。

总之，STM32 微控制器凭借其高性能、丰富的外设、低功耗、可扩展性、开发工具和生态系统，成为嵌入式系统开发的热门选择。

## 2. STM32 选型

> 中国有句古话说的好：“工欲善其事，必先利其器”，所以，在开始我们的嵌入式旅程前，得先挑选一块趁手的开发板。

### 2.1 STM32 分类

<table>
    <tr>
        <td>CPU 位数</td>
        <td>内核</td>
        <td>系列</td>
        <td>描述</td>
    </tr>
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
        <td rowspan="5">6502</td>
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
</table>

### 2.2 STM32 命名方法

![STM32 命名方法图](/docs/images/嵌入式/STM32命名方法图.png)

> 注：摘自《STM32 选型手册》

## 3. 开发导引

### 3.1 分配原理图 IO

在画原理图之前，一般做法是先把引脚分类好，然后才开始画原理图，一般分类如下：

| 引脚分类    | 引脚说明                                             |
|---------|--------------------------------------------------|
| 电源      | VBAT、VDD、VSS、VDDA、VSSA、VREF+、VREF-等              |
| 晶振 IO   | 主晶振、RTC 晶振                                       |
| 下载 IO   | JTAG（JTMS、JTCK、JTDI、JTDO、NJRST）、SWD（SWDIO、SWCLK） |
| BOOT IO | BOOT0、BOOT1，设置板子启动方式                             |
| 复位 IO   | NRST，用于外部复位                                      |
| GPIO    | 专用总线（I2C、SPI、CAN、USB、FSMC、DMCI 等）、通用 IO          |

### 3.2 关于引脚

> 注：有关这一部分可以在 _Pinouts and pin descriptions_ 小节中找到

![STM32 引脚定义图](/docs/images/嵌入式/STM32引脚定义图.png)

对引脚的解读如下：

| 名称                       | 说明                                                                |
|--------------------------|-------------------------------------------------------------------|
| 引脚序号（Pins）               | 六种封装（具体要根据需求选择）                                                   |
| 引脚名称（Pin name）           | 指复位状态下引脚名称                                                        |
| 引脚类型（Type）               | S：电源<br/>I：输入<br/>I/O：输入/输出                                       |
| I/O 结构（I/O Level）        | FT：兼容 5V<br/>TTa：支持 3.3V、直接 ADC<br/>B：BOOT 引脚<br/>RST：复位引脚、内部带弱上拉 |
| 主功能（Main function）       | 引脚复位后的功能                                                          |
| 复用功能（Alternate function） | I/O 默认的复用功能                                                       |
| 重映射（Remap）               | I/O 除了默认的复用功能外，还能够通过重映射的方式映射到其他 I/O，增加了 I/O 口功能的灵活性和多样性           |
