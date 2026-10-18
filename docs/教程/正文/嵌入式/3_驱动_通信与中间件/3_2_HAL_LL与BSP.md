# 3.2 HAL、LL 与 BSP

在比较漫长的汇编语言时代，很多程序员都习惯用位操作对底层硬件进行编写，其优点有以下等：

- 代码直观
- 代码执行效率高
- 代码体积小

缺点有以下等：

- 代码可读性差
- 代码移植性差
- 代码维护性差
- 代码复用性差
- 代码调试困难

在微控制器技术变得更加复杂和多样化之后，
人们开始将底层硬件和控制代码解耦合，让开发者所编写的应用代码与底层硬件无关，
HAL 库的设计目的正是为此，从而使开发者更容易地移植和维护嵌入式软件。

## 1. CMSIS 介绍

Cortex 微控制器软件接口标准 (Cortex Microcontroller Software Interface Standard, CMSIS) 是 Arm 为 Cortex-M 系列微控制器制定的标准化软件接口，目的是让软件在不同的 Cortex-M 微控制器之间更容易移植和复用。

![CMSIS 结构图](/images/嵌入式/CMSIS架构图.png)

CMSIS 包括以下几个部分：

1. CMSIS-Core：用于启动、中断处理、内存管理和异常处理的通用函数。
2. CMSIS-DSP：数字信号处理 (Digital Signal Processing, DSP) 函数库。
3. CMSIS-RTOS：实时操作系统 (Real-Time Operating System, RTOS) 的通用接口，开发者可以选择并集成不同的 RTOS 实现。
4. CMSIS-Device：为每种 Cortex-M 微控制器定义寄存器映射和中断向量表。
5. CMSIS-Driver：访问微控制器通用外设的驱动程序接口。

CMSIS 统一的主要是内核部分（如 NVIC、SysTick）的访问方式。各厂商外设的寄存器布局互不相同，更换芯片厂商时，直接操作外设寄存器的代码仍要重写。

## 2. HAL 库介绍

ST 为了方便用户开发 STM32 芯片，提供了三种库：

1. 标准外设库（Standard Peripheral Libraries）：
   - ST 最早的库，现在还在用
   - 目前只兼容 F0/F1/F3/F2/F4/L1 系列
   - 目前已停止维护
2. 硬件抽象层（Hardware Abstraction Layer）：
   - ST 目前主推的库
   - 全系列兼容
   - 兼容性好、移植方便，但效率不太高
3. 低级抽象层（Low Layer）：
   - 与 HAL 库捆绑发布
   - 全系列兼容
   - 轻量级、效率高
   - 不匹配部分复杂外设，比如 USB

## 3. CMSIS 与 HAL 区别

CMSIS 是 Arm 制定的标准接口，在不同厂商的 Cortex-M 微控制器之间通用；HAL 库由微控制器厂商提供（STM32 的 HAL 库由 ST 提供），针对自家芯片实现。

CMSIS 的抽象层次低，直接对应硬件寄存器和中断向量表；HAL 库的层次更高，隐藏了硬件细节，把外设的初始化和操作封装成一系列易于使用的函数。

两者可以一起使用：CMSIS 定义硬件访问的框架，HAL 库在它之上提供具体实现，共同构成从启动到外设访问的完整软件栈。
