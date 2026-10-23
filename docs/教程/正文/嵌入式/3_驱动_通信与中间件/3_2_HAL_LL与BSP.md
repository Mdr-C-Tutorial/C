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

## 4. 习题

<Exercise id="63311" :d="1" :w="1">

下面四行代码分别属于哪一层（寄存器直写、LL、HAL、BSP），各写一句判断依据：

```c
GPIOC->BSRR = 1u << 13;
LL_GPIO_SetOutputPin(GPIOC, 1u << 13);
HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_RESET);
board_led_set(true);
```

再说明第 3 行与第 4 行虽然都能点亮同一块板上的灯，换一块 LED 接在 PB0、高电平点亮的板子时，哪一行必须改写、哪一行可能一个字都不用动。

</Exercise>

<Exercise id="63312" :d="3" :w="3">

同一个功能写三份，比较生成代码的大小。新建三个文件，每个只含一个 `board_led_set(int on)`：

- `cmp_raw.c` 直接写 `GPIOC->BSRR`（置位用 `1u << 13`，清零用 `1u << 29`）；
- `cmp_ll.c` 自己写两个 `static inline` 函数 `LL_GPIO_SetOutputPin` 与 `LL_GPIO_ResetOutputPin`，内部都写 `BSRR`，`board_led_set` 只调这两个函数；
- `cmp_hal.c` 照 `HAL_GPIO_WritePin(GPIOx, GPIO_Pin, PinState)` 的形式写一个带 `GPIO_TypeDef *` 与 `uint16_t` 两个参数的普通函数，再用一个结构体保存 LED 的端口与引脚号来调用它。

三个文件都用下面的命令编译，用 `arm-none-eabi-size` 与 `arm-none-eabi-objdump -d` 看 `text` 段与 `board_led_set` 的反汇编：

```sh
arm-none-eabi-gcc -std=c11 -mcpu=cortex-m3 -mthumb -O2 -Wall -Wextra \
    -DSTM32F103xB -I cmsis/CMSIS/Core/Include -I cmsis/cmsis_device_f1/Include \
    -c cmp_ll.c -o cmp_ll.o
```

回答：

(1) 三份 `text` 段各多少字节，`board_led_set` 各多少字节。

(2) `cmp_ll` 与 `cmp_raw` 的反汇编是否逐条相同。如果相同，说明 `static inline` 与宏在这里的差别是什么；如果不同，贴出差异。

(3) `cmp_hal` 里那句 `if (h == NULL || h->port == NULL)` 在 `-O2` 下还剩几条指令。把这一句删掉再编一次，比较 `text` 段，说明编译器证明了什么。

(4) 把三个文件都改成 `-O0` 再编一次。哪一份的 `text` 段涨得最多，涨在哪几条指令上（提示：看 `bl` 与压栈）。

(5) 综合 (3)(4) 回答：「HAL 效率低」这个说法在什么条件下成立、在什么条件下不成立；如果项目真的卡在 Flash 容量上，该做的是换库还是换编译选项。

</Exercise>

<Exercise id="63313" :d="5" :w="4">

按 CMSIS-Driver 的思路，把 `uart` 驱动改造成函数表，让应用代码不出现 `USART_TypeDef`：

(1) 定义

```c
struct uart_driver {
    const char *name;
    int32_t (*init)(void *inst, uint32_t baud);
    int32_t (*send)(void *inst, const void *buf, uint32_t len);
    int32_t (*recv)(void *inst, void *buf, uint32_t len);
};
```

(2) 写两份实现：一份包住现有的 `uart_init`、`uart_write`（用 `USART1`，`pclk` 取 `72000000u`）；另一份是宿主机上的假实现，把收到的字节追加进一个字符数组。

(3) `app_run(const struct uart_driver *drv, void *inst)` 只调函数表，依次发 `"hello"` 与 `" world"`，最后打印 `drv->name` 与假实现收到的内容。宿主机运行结果应为 `FAKE: hello world`。

(4) 再写一个 `main`，用同一个 `app_run` 分别跑两次：一次传假实现，一次传真实现（只在 `arm-none-eabi-gcc` 下编译，不运行）。

(5) 说明这种函数表与 3.1 第 6 节「外设实例作参数」的接口相比，各自由谁决定用哪个外设，以及函数表多出来的那一层在什么规模的项目里才划算。

</Exercise>

<Exercise id="63314" :d="5" :w="4" scope="*">

选一块与 Blue Pill 接线不同的板子（LED 接 PB0 高电平点亮、按键接 PB1 按下为低电平、控制台走 USART2），在 3.1 的工程上验证「换板只改板级」这个说法到底有多成立。

(1) 写出新的 `board_stm32f103_b.c`：只改 `k_led_pin`、`k_btn_pin` 的端口与有效电平，控制台改用 `USART2`（PA2 复用推挽、PA3 浮空输入，PCLK1 为 PCLK2 的一半）。

(2) 用 `arm-none-eabi-gcc -c` 编译。如果 `app/blink_logic.c` 与 `drivers/gpio.c`、`drivers/uart.c` 一个字都没改就通过，说明分层成立；如果有文件必须改，指出是哪个、为什么，并说明这属于设计缺陷还是本来就应该由板级承担。

(3) 用 `arm-none-eabi-size` 比较新旧两份板级文件编出来的 `text` 段，说明差别来自哪里。

(4) 在 PC 上用 `board_host.c` 模拟这块新板（LED 高电平亮、按键低电平有效），把同样的按键序列喂进去，确认 `blink_logic.c` 的行为与 Blue Pill 上一致。

(5) 反思 3.1 第 6 节的「应用层不该知道 LED 接在哪个引脚」：`board_button_pressed` 把「按下」定义成高电平有效，这个决定属于驱动、板级还是应用？如果换成一块按键按下为低电平、且外部有上拉电阻的板子，`gpio_pin` 里的 `active_low` 该写什么？

</Exercise>
