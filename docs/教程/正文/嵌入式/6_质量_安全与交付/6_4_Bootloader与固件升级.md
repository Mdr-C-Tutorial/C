# 6.4 Bootloader 与固件升级

一批设备已经装到现场，固件里有一个只在特定输入下才出现的缺陷。把设备收回来、拆开、接上 ST-LINK 重新烧录，成本比设备本身还高；如果设备装在楼顶或者配电柜里，这件事根本做不了。设备必须能从自己的通信接口收下新固件，自己把它装进去，装坏了还能退回来。

这一页做一套最小但完整的方案：把 64 KiB 片内 Flash 划成 Bootloader、应用槽和配置页，把应用改到新地址上运行；给固件加一个带版本与 CRC 的头部，由 Bootloader 校验后跳转；用外部 W25Q64 做暂存与备份，设计一条在任意时刻断电都能恢复到可启动状态的升级流程。读完这一页，读者能自己写出这套东西，也能判断什么时候该直接改用 MCUboot 这类现成方案。

## 1. 为什么要自己的 Bootloader

STM32F103 出厂时在系统存储器里带了一个引导程序，BOOT0 接高电平、BOOT1 接低电平上电就进入它。它通过 USART1 接收命令，擦除、写入、读出 Flash，协议见 ST 的 AN3155。[1.6 构建、烧录与调试](/教程/正文/嵌入式/1_平台与工具链/1_6_构建_烧录与调试) 里用 FlyMCU 下载程序，走的就是这条路。

它解决的是「产线上把固件灌进空芯片」，不是「现场升级」。三个问题绕不过去：要改 BOOT0 的电平，设备装进外壳之后这个引脚未必引得出来；下载工具每次从头写整片 Flash，没有版本概念，也不会拒绝一个不完整的文件；写完不复位，得再断一次电。固件自己升级需要的几件事——判断当前该运行哪一份映像、校验它是否完整、把新映像装进去、装失败时恢复——它一件都不管。

自己写一个 Bootloader，就是补上这四件事。它占据 Flash 起始处的一小块，上电后先运行，检查应用槽里的映像，合格就跳过去，不合格就停在原地等新固件。

| 方案 | 谁在用 | 适用场合 | 主要限制 |
| --- | --- | --- | --- |
| 芯片自带的系统存储器引导程序 | 产线初次烧录 | 研发阶段、产线工装 | 要动 BOOT 引脚，无版本与校验管理 |
| 自己写的 Bootloader | 中小批量的产品 | 片内 Flash 装得下两份元数据、团队能维护 | 升级流程与断电安全要自己保证 |
| MCUboot | 需要成熟方案的产品 | 有 RTOS 或较大 Flash 的 MCU | 代码体积比手写的大，移植要改 flash 驱动 |

MCUboot 是 Arm 生态里使用很广的开源 Bootloader，支持签名校验、防回滚与多种槽交换方式，后面几节会拿它作对照。

## 2. 存储划分

F103C8 的片内 Flash 一共 64 KiB，页大小 1 KiB。页是擦除的最小单位，写之前必须先擦，这一条决定了所有布局都要按 1 KiB 对齐。

| 区域 | 地址范围 | 大小 | 内容 |
| --- | --- | --- | --- |
| Bootloader | 0x08000000–0x08003FFF | 16 KiB | 启动代码、校验、跳转、安装逻辑 |
| 应用槽 | 0x08004000–0x0800FBFF | 47 KiB | 0x200 字节映像头 + 应用（向量表在 0x08004200） |
| 配置页 | 0x0800FC00–0x0800FFFF | 1 KiB | 升级日志、设备参数（[3.9](/教程/正文/嵌入式/3_驱动_通信与中间件/3_9_Flash_EEPROM_SD与文件系统) 已预留） |

<MemoryMap
  title="STM32F103C8 的 Flash 分区"
  order="asc"
  :regions="[
    { name: 'Bootloader', start: 0x08000000, size: '16K', tone: 'brand', desc: '上电先运行，校验并跳转' },
    { name: '映像头', start: 0x08004000, size: '512', tone: 'yellow', desc: '魔数、版本、长度、CRC-32' },
    { name: '应用', start: 0x08004200, size: '0xBA00', tone: 'green', startLabel: 'APP_VECTORS', desc: '向量表在起始处，4 字节对齐' },
    { name: '配置页', start: 0x0800FC00, size: '1K', tone: 'purple', desc: '升级日志，追加写不擦除' }
  ]"
/>

外部 W25Q64 的 8 MiB 分成四段，这是 6.2、6.7、7.2、7.3 共用的划分：0x000000–0x00FFFF 是固件暂存槽，0x010000–0x01FFFF 是固件备份槽，0x020000–0x02FFFF 放诊断与核心转储（[6.2](/教程/正文/嵌入式/6_质量_安全与交付/6_2_日志_断言与故障诊断)），0x030000 起是 littlefs 的日志区（[7.2](/教程/正文/嵌入式/7_综合实践/7_2_RTOS多任务设备)）。

暂存槽负责「收」，备份槽负责「退」。片内 Flash 只有 64 KiB，装不下两份完整的应用，所以版本回退用的旧映像放在外部 Flash 里。

映像头为什么是 0x200 字节而不是 32 字节？因为向量表必须放在 512 字节对齐的地址上。PM0056 规定 VTOR 的低 9 位是保留位、写入时必须为 0，因此向量表的地址必须是 0x200 的整数倍。应用槽从 0x08004000 开始，头部占 0x200，向量表正好落在 0x08004200；头部再小一点，向量表的位置就不再对齐，跳过去之后第一次中断就会取错向量。

## 3. 让应用在新地址运行

应用的链接脚本只改 MEMORY 段：Flash 的起始地址从 0x08000000 挪到 0x08004200，长度从 64K 改成应用槽去掉头部之后的 0xBA00 字节。其余段定义与 [1.8 复位、启动与链接](/教程/正文/嵌入式/1_平台与工具链/1_8_复位_启动与链接) 的 `stm32f103c8.ld` 完全相同：

```text
MEMORY
{
    FLASH (rx)  : ORIGIN = 0x08004200, LENGTH = 0xBA00
    RAM   (rwx) : ORIGIN = 0x20000000, LENGTH = 20K
}
```

用 arm-none-eabi-gcc 13.3 链接一个最小的应用（`main` 里设置 VTOR、开 SysTick、闪灯），`objdump -h` 里能看到段确实搬了位置：

::: terminal title="bash - 应用链接到 0x08004200 后的段布局"

```text
Sections:
Idx Name          Size      VMA       LMA       File off  Algn
  0 .isr_vector   000000ec  08004200  08004200  00000200  2**2
                  CONTENTS, ALLOC, LOAD, READONLY, DATA
  1 .text         00000288  080042ec  080042ec  000002ec  2**2
                  CONTENTS, ALLOC, LOAD, READONLY, CODE
  2 .rodata       00000000  08004574  08004574  00001004  2**0
                  CONTENTS, ALLOC, LOAD, DATA
  3 .ARM.exidx    00000008  08004574  08004574  00000574  2**2
                  CONTENTS, ALLOC, LOAD, READONLY, DATA
  4 .data         00000004  20000000  0800457c  00001000  2**2
                  CONTENTS, ALLOC, LOAD, DATA
```

:::

`.isr_vector` 长度 0xEC（236 字节，59 项），地址 0x08004200，正是映像头之后的位置。把这一段的头两个字读出来，前一个是初始栈指针，后一个是复位向量：

::: terminal title="bash - 应用向量表的前两个字"

```text
Contents of section .isr_vector:
 8004200 00500020 f1420008 ed420008 ed420008  .P. .B...B...B..
```

:::

`00500020` 按小端读是 0x20005000，即 SRAM 末尾之后那个字节，与 `_estack` 一致；`f1420008` 是 0x080042F1，最低位为 1 表示 Thumb 代码，去掉最低位就是复位处理函数 0x080042F0。

应用不能只靠链接地址就正确运行。Cortex-M3 复位时 VTOR 是 0，所有中断都从 0x00000000 起的向量表取地址；Bootloader 把它改到了自己的表上，跳转时如果不再改一次，应用里发生的每个中断都会跳进 Bootloader 的处理函数。应用启动的第一件事就是写 VTOR：

```c
int main(void) {
    SCB->VTOR = 0x08004200u;
    __enable_irq();
    SysTick_Config(SystemCoreClock / 1000u);
    for (;;) {
    }
}
```

CubeMX 生成的工程里，这个值由 `system_stm32f1xx.c` 的 `VECT_TAB_OFFSET` 给出，前提是定义了 `USER_VECT_TAB_ADDRESS`，而模板默认没有定义。手工改链接脚本时最容易漏掉这一处。

::: warning 忘了设 VTOR

链接到 0x08004200 的应用在 Bootloader 跳过来之后能跑主循环，因为复位向量是直接调用过去的；但第一个中断一到，CPU 按 VTOR 指向的旧表取址，跳进的是 Bootloader 的处理函数。现象是「一开定时器就死在莫名其妙的地方」，用调试器看 PC 会落在 0x0800xxxx 的低地址段。

:::

::: concept img-vtor-alignment title="向量表的地址必须 512 字节对齐"

Cortex-M3 的 VTOR 寄存器低 9 位为保留位，写入的值必须是 0x200 的整数倍。这条限制来自异常向量表查找方式：处理器把异常号乘以 4 加到 VTOR 上取地址，表项本身是 4 字节，但架构规定表基址按 512 字节对齐。所以映像头取 0x200 字节，让向量表正好落在应用槽起始地址加 0x200 的位置。

:::

<Quiz
  type="single"
  :options="[
    { label: '把映像头缩小到 0x20 字节，向量表放到 0x08004020', value: 'A' },
    { label: '把映像头定为 0x200 字节，向量表放到 0x08004200', value: 'B' },
    { label: '把向量表放到应用槽末尾，VTOR 指向末尾', value: 'C' },
    { label: '保持向量表在 0x08004000，不设 VTOR', value: 'D' }
  ]"
  answer="B"
>

<template #question>

应用槽从 0x08004000 开始（1 KiB 页对齐），头部之后紧跟向量表。下面哪种布局能让 VTOR 写入时不丢失低位？

</template>

正确答案是 **B**。VTOR 的低 9 位是保留位，向量表地址必须是 0x200 的整数倍。0x08004000 本身是 0x200 的倍数，加 0x200 之后仍然对齐；加 0x20 就不再对齐，写入 VTOR 时低位被硬件忽略，向量表实际会被定位到 0x08004000，与头部重叠。D 的错误在于应用根本不会回到 0x08004000 那个位置运行。

</Quiz>

## 4. 映像头与校验

头部要回答 Bootloader 的四个问题：这是不是一个合法的映像、它有多长、它是哪个版本、它有没有损坏。字段按地址顺序排列，一共 32 字节，其余填充到 0x200：

<BitField
  mode="packet"
  title="映像头的前 32 字节（小端，按地址顺序）"
  :fields="[
    { name: 'magic', bits: 32, value: '0x4252444D', tone: 'brand', desc: '小端存储时字节序列是 4D 44 52 42，即 MDRB' },
    { name: 'header_version', bits: 16, value: '0x0001', desc: '头部格式版本，将来加字段时靠它区分' },
    { name: 'header_size', bits: 16, value: '0x0200', desc: '头部总长，含填充' },
    { name: 'image_size', bits: 32, desc: '头部加负载的总长度，Bootloader 靠它确定 CRC 的范围' },
    { name: 'load_addr', bits: 32, value: '0x08004200', desc: '负载的加载地址，即向量表地址' },
    { name: 'fw_version', bits: 32, value: '0x00010200', desc: '主 << 16 | 次 << 8 | 修订，这里是 1.2.0' },
    { name: 'hw_version', bits: 32, desc: '硬件版本，防止把 A 板的固件装到 B 板上' },
    { name: 'payload_crc32', bits: 32, tone: 'yellow', desc: '只覆盖负载，不含头部' },
    { name: 'build_id', bits: 32, desc: '构建标识，6.8 会把它和 Git 提交对上' }
  ]"
/>

后 0x1E0 字节填 0。头部的 CRC 只覆盖负载而不是整个映像，这样头部本身的错误由魔数、长度范围和加载地址三项检查兜住，不必自己校验自己。

### 4.1 生成带头的映像

主机上的小工具读进 `app.bin`，加上头部写出 `app.img`。核心是填结构体、算 CRC、按顺序写出：

```c
image_header_t header;
memset(&header, 0, sizeof header);
header.magic = IMAGE_MAGIC;              /* 'M','D','R','B' */
header.header_version = IMAGE_HEADER_VERSION;
header.header_size = IMAGE_HEADER_SIZE;  /* 0x200 */
header.image_size = (uint32_t)payload_size + IMAGE_HEADER_SIZE;
header.load_addr = IMAGE_LOAD_ADDR;      /* 0x08004200 */
header.fw_version = fw;                  /* 主 << 16 | 次 << 8 | 修订 */
header.hw_version = hw;
header.payload_crc32 = crc32(payload, payload_size);
header.build_id = 0x20261021u;

FILE *out = fopen(argv[2], "wb");
fwrite(&header, 1, sizeof header, out);
fwrite(payload, 1, (size_t)payload_size, out);
fclose(out);
```

本机 MinGW gcc 13.2 编译运行，输入是上一节那个 896 字节的应用：

::: terminal title="PowerShell - mkimage app.bin app.img 1.2.0 0x0001"

```text
输入：app.bin，896 字节
输出：app.img，1408 字节
  魔数        MDRB (0x4252444D)
  头版本      1，头部大小 512 字节
  映像长度    1408 字节
  加载地址    0x08004200
  固件版本    1.2.0 (0x00010200)
  硬件版本    0x0001
  负载 CRC-32 0x7B5273C5
```

:::

1408 = 512 + 896，对得上。头部里有三个长度，含义各不相同：`header_size` 是头部自身，`image_size` 是整份映像，CRC 的范围是 `image_size - header_size`。Bootloader 端必须按同一套约定计算，否则两边算出的值永远对不上。

### 4.2 CRC-32 用软件实现

CRC-32 的标准校验值是固定字符串 `"123456789"` 算出 `0xCBF43926`。工具里带一个自检入口，构建之后先跑一遍：

::: terminal title="PowerShell - mkimage --selftest"

```text
CRC-32("123456789") = 0xCBF43926（期望 0xCBF43926）
```

:::

实现用 16 项的半字节表：一次处理半个字节，查表 2 次，表本身只占 64 字节常量，不必在 RAM 里建 256 项的表，也不必多占 1 KiB Flash。片上 CRC 单元看着更省事，但 F103 的 CRC 单元算的不是这个算法：它的多项式是 0x04C11DB7，输入不反射、输出也不反射，等同于 CRC-32/MPEG-2，同一串 `"123456789"` 得到的是 0x0376E6E7，与 zlib 的 `crc32()` 不同。

::: warning 片上 CRC 单元与 zlib 不是同一个算法

F103 的 CRC 单元是 CRC-32/MPEG-2（不反射、初值 0xFFFFFFFF、结果不异或），主机上常用的 CRC-32 是 IEEE 802.3（反射、结果异或 0xFFFFFFFF）。两者混用，映像在主机上校验通过、到设备上报错。两端统一用同一份软件实现，或者把主机的算法改成 MPEG-2，不要一边硬件一边软件。

:::

## 5. 跳转到应用

Bootloader 在跳转之前要做三件事：确认头部合法、确认负载的 CRC 对得上、确认向量表前两项落在合理范围。前两件在上一节已经具备，第三件是跳转前的最后一道检查：

```c
static bool vectors_plausible(uint32_t vt) {
    const uint32_t *v = (const uint32_t *)vt;
    uint32_t sp = v[0];
    uint32_t pc = v[1];
    if (sp <= APP_SRAM_BASE || sp > APP_SRAM_END || (sp & 0x3u) != 0u) {
        return false;
    }
    return (pc & 1u) != 0u && pc > APP_SLOT_ADDR && pc < APP_SLOT_ADDR + 0xBC00u;
}
```

初始栈指针必须落在 SRAM 里、按 4 字节对齐；复位向量最低位必须是 1（Thumb 状态），地址落在应用槽内。这两项检查不花多少代码，却能挡住一整类「CRC 碰巧对上、但映像根本不是给这块板子的」错误。

跳转本身只有几行，但顺序很重要：

```c
static void jump_to_app(uint32_t vt) {
    uint32_t sp = ((const uint32_t *)vt)[0];
    uint32_t pc = ((const uint32_t *)vt)[1];

    __disable_irq();          /* PRIMASK = 1，跳转过程中不响应中断 */
    SysTick->CTRL = 0u;
    SysTick->LOAD = 0u;
    SysTick->VAL = 0u;

    /* Bootloader 用过的外设复位到上电状态 */
    RCC->APB1RSTR = 0xFFFFFFFFu;
    RCC->APB2RSTR = 0xFFFFFFFFu;
    RCC->APB1RSTR = 0u;
    RCC->APB2RSTR = 0u;

    /* 关掉所有中断、清掉挂起位，应用自己重新使能 */
    for (uint32_t i = 0; i < 8u; ++i) {
        NVIC->ICER[i] = 0xFFFFFFFFu;
        NVIC->ICPR[i] = 0xFFFFFFFFu;
    }

    SCB->VTOR = vt;
    __DSB();
    __ISB();

    __set_MSP(sp);
    void (*entry)(void) = (void (*)(void))pc;
    entry();
    for (;;) {
    }
}
```

编译出来，最后三条指令就是这三步：

::: terminal title="bash - jump_to_app 的末尾（arm-none-eabi-objdump -d）"

```text
 8000542:	609a      	str	r2, [r3, #8]        ; SCB->VTOR = 0x08004200
 8000544:	f3bf 8f4f 	dsb	sy
 8000548:	f3bf 8f6f 	isb	sy
 800054c:	f386 8808 	msr	MSP, r6             ; 换上应用的初始栈指针
 8000550:	47a8      	blx	r5                  ; 跳到应用的复位向量
 8000552:	e7fe      	b.n	8000552 <main+0xe6> ; 应用返回了就停在这里
```

:::

清理外设与中断这一步，做与不做差别很大：

::: compare left-title="直接跳转" right-title="先清理再跳转"

```c
__set_MSP(sp);
SCB->VTOR = vt;
((void (*)(void))pc)();
```

---

```c
__disable_irq();
SysTick->CTRL = 0u;
RCC->APB1RSTR = 0xFFFFFFFFu;
RCC->APB2RSTR = 0xFFFFFFFFu;
RCC->APB1RSTR = 0u;
RCC->APB2RSTR = 0u;
NVIC->ICER[0] = 0xFFFFFFFFu;
NVIC->ICPR[0] = 0xFFFFFFFFu;  /* 八个字，这里只写一个示意 */
SCB->VTOR = vt;
__DSB();
__ISB();
__set_MSP(sp);
((void (*)(void))pc)();
```

:::

直接跳转的问题不在跳的那一下，而在跳过去之后：Bootloader 初始化过的 USART 还在发、定时器还在计数、某个中断还挂在 NVIC 里等待。应用以为自己是从复位开始跑的，把这些外设重新初始化一遍，于是同一个外设被配置两次；如果 Bootloader 里还开着某个中断，它的处理函数地址仍指向 Bootloader，应用刚开中断就跳回旧代码。

`__disable_irq()` 改的是 PRIMASK，跳转不会复位它。应用必须自己 `__enable_irq()`，否则主循环能跑、中断永远不来，表现为「串口收不到数据、SysTick 不走」。这一句很容易漏，因为调试时单步执行看到主循环正常，会以为跳转成功了。

### 5.1 启动决策

Bootloader 的 `main` 把上面几步串起来：先看配置页里的升级日志，有未完成的升级就接着做完；然后校验应用槽，通过就跳，不通过就停在原地等新固件（此时串口上要能接收升级命令）。

```mermaid
flowchart TD
    A["上电"] --> B["读配置页日志"]
    B --> C{"有未完成的升级"}
    C -->|是| D["执行安装或回滚"]
    D --> B
    C -->|否| E{"应用槽映像合法"}
    E -->|是| F["设置 VTOR 与 MSP，跳到应用"]
    E -->|否| G{"备份槽映像合法"}
    G -->|是| H["从备份槽恢复应用槽"]
    H --> E
    G -->|否| I["停在 Bootloader，等待串口升级"]
```

这个流程的关键性质是：`D` 这一段的每一步都可以重复执行，掉电后重新进来会从日志记录的步骤继续，不会因为「做了一半」而卡死。日志怎么设计，是下一节的内容。

## 6. 升级流程

一次完整的升级分成两侧：运行中的应用负责「收」，刚上电的 Bootloader 负责「装」。

```mermaid
stateDiagram-v2
    [*] --> 运行中
    运行中 --> 已暂存: 收下映像，写入 W25Q64 暂存槽，CRC 校验通过
    已暂存 --> 已备份: 复位，应用槽复制到备份槽
    已备份 --> 已安装: 暂存槽写入应用槽
    已安装 --> 试运行: 记下试运行序号，启动新映像
    试运行 --> 已确认: 应用自检通过，写入确认记录
    试运行 --> 已回滚: 连续两次未确认
    已回滚 --> 运行中: 启动旧映像
    已确认 --> 运行中: 正常启动
```

应用侧的动作很轻：分块接收，写进 W25Q64 暂存槽，全部收完后整份读出来算一次 CRC，与头部里的值比对，通过就记一条「已暂存」的记录。真正动片内 Flash 的是 Bootloader，因为改写自己正在运行的那块 Flash 是不行的。

### 6.1 状态记在配置页里

升级过程要能在任意时刻被打断。最简单可靠的办法是把每次状态变化追加写在配置页里，不擦除、只追加。F103 的 Flash 编程只能把 1 写成 0，擦除才是把 0 变回 1；追加写天然满足「已写的内容不会被后续操作破坏」。

日志记录做成 8 字节，末尾带一个 CRC：

```c
typedef struct {
    uint8_t tag;      /* 状态码 */
    uint8_t arg;      /* 试运行序号 */
    uint16_t arg2;
    uint32_t check;   /* 前 4 字节的 CRC-32 */
} journal_rec_t;

/* 从第一条空记录开始写；写到一半断电，这条的校验对不上，读时跳过 */
static int journal_append(nvm_t *nvm, uint8_t tag, uint8_t arg, uint16_t arg2) {
    journal_rec_t rec = { .tag = tag, .arg = arg, .arg2 = arg2, .check = 0u };
    rec.check = rec_check(&rec);
    for (uint32_t off = 0; off + sizeof rec <= JOURNAL_SIZE; off += sizeof rec) {
        if (nvm->journal[off] == 0xFFu) {
            return nvm_program(nvm, nvm->journal, off, &rec, sizeof rec);
        }
    }
    return -2;
}
```

记录写到一半断电时，那条记录是不完整的，但它的位置已经不再是全 1，后续记录会写到它后面；读日志时按顺序扫，校验对不上的跳过，最后一条有效记录就是当前状态。这个做法不需要「擦除配置页再写」，因此也不存在「擦到一半断电、原来那条也没了」的窗口。

日志区 1 KiB 能写 128 条记录，按一次升级用 6 条算，够二十次升级。真写满了要先把有效状态压缩进一条新记录再擦除，本页不做。

### 6.2 安装与回滚

状态机的核心是「每一步都可以重复执行」。Bootloader 每次上电先读日志，按当前状态做该做的事：

```c
if (st == UPD_STAGED) {
    /* 先把当前应用备份到 W25Q64，再动片内 Flash */
    if (image_slot_valid(nvm->app) && !image_slot_valid(nvm->backup)) {
        if (copy_slot(nvm, nvm->app, nvm->backup) != 0) {
            return BOOT_NONE;
        }
        if (journal_append(nvm, TAG_BACKUP, 0u, 0u) != 0) {
            return BOOT_NONE;
        }
        continue;
    }
    if (copy_slot(nvm, nvm->staging, nvm->app) != 0) {
        return BOOT_NONE;
    }
    if (journal_append(nvm, TAG_INSTALL, 0u, 0u) != 0) {
        return image_slot_valid(nvm->app) ? BOOT_APP : BOOT_NONE;
    }
    continue;
}

if (st == UPD_TRIAL) {
    if (!image_slot_valid(nvm->app)) {
        return BOOT_NONE;
    }
    unsigned trial = update_trial_count(nvm);
    if (trial >= MAX_TRIALS) {
        /* 试运行次数用完仍未确认：从备份恢复 */
        if (!image_slot_valid(nvm->backup)) {
            return BOOT_APP;
        }
        if (copy_slot(nvm, nvm->backup, nvm->app) != 0) {
            return BOOT_NONE;
        }
        if (journal_append(nvm, TAG_ROLLBACK, 0u, 0u) != 0) {
            return BOOT_NONE;
        }
        continue;
    }
    if (journal_append(nvm, TAG_TRIAL, (uint8_t)(trial + 1u), 0u) != 0) {
        return BOOT_NONE;
    }
    return BOOT_APP;
}
```

`continue` 回到循环开头重新读状态，所以一条 `if` 分支只做一件事：备份、安装、试运行、回滚各自独立，掉电以后重入也不会重复做已经完成的事。`copy_slot` 先擦后写，被它擦掉的那一侧在完成之前都不是合法映像，而另一侧始终完好。

应用要在自检通过之后写「已确认」：

```c
if (self_test_passed()) {
    update_confirm(&nvm);
}
```

试运行允许两次。第一次上电装上并启动，应用如果因为通信没起来、传感器没响应而没有确认，第二次上电仍然启动它；两次都没确认，第三次上电就回滚。这个次数可调，取值取决于「新固件要多久才能判断自己能不能工作」。

### 6.3 主机上的验证

片内 Flash 与 W25Q64 各写一个模拟版本：一块字节数组，擦除填 0xFF，编程按位与，另外给一个「可编程字节预算」，每写一字节减一，减到 0 就当作这一刻断电，之后所有操作都失败。这套模拟让升级流程能在 PC 上被反复折腾，不必真的拔电。

三组测试跑下来：正常升级、暂存损坏、连续两次不确认：

::: terminal title="PowerShell - test_update.exe"

```text
== 正常升级：1.0.0 在运行，收到 1.1.0 并确认 ==
  初始：1.0.0 在应用槽 状态=无    应用=有效 备份=无效
  应用收到 1.1.0，已暂存 状态=暂存 应用=有效 备份=无效
  第一次上电：试运行 状态=试运行 应用=有效 备份=有效
  第二次上电：仍在试运行 状态=试运行 应用=有效 备份=有效
  应用自检通过，确认 状态=已确认 应用=有效 备份=有效
  再次上电：正常启动 状态=已确认 应用=有效 备份=有效
== 暂存区写入损坏的映像：拒绝登记 ==
  损坏映像               状态=无    应用=无效 备份=无效
== 回滚：新映像启动两次都不确认 ==
  试运行第 1 次         状态=试运行 应用=有效 备份=有效
  试运行第 2 次         状态=试运行 应用=有效 备份=有效
  第 3 次上电：回滚   状态=已回滚 应用=有效 备份=有效
```

:::

第一组里能看出备份槽是什么时候出现的：暂存槽有映像、应用槽还是旧版本时，备份槽是空的；第一次上电之后备份槽里是 1.0.0。回滚那一组里，第三次上电后应用槽的版本回到 1.0.0，状态记录为「已回滚」。

损坏映像那一组里，应用槽与备份槽都是空的（模拟的是空片），暂存区里的映像被改了一个字节，CRC 对不上，`update_stage_image` 直接拒绝，日志里什么也没写。现场发生的更可能是「收到一半断线」，这时暂存槽里是半份映像，同样过不了 CRC。

## 7. 断电安全

上面那套流程为什么要先备份、再安装、再试运行？因为升级过程中任何一刻都可能断电，而设备必须在下次上电时还能启动。

::: concept emb-update-one-bootable title="任何时刻至少保留一个可启动映像"

升级流程的每一步都必须满足：擦除或写入操作只作用于其中一个槽，另一个槽里的内容在这一步结束前保持完整。应用槽被擦掉、尚未写完时，备份槽里的旧映像必须完好；备份槽正在被覆盖时，应用槽里的当前映像必须完好。只要这个性质成立，掉电最坏的结果是「这次升级作废」，而不是「设备变砖」。

:::

单槽直接覆盖为什么危险，用一次操作就能看清：应用正在接收新固件，收完直接擦掉应用槽、往里写。擦除那一瞬间，唯一的可启动映像没了，此后到写完为止的任何一次断电——包括写最后一个字节时的掉电——都会让设备停在 Bootloader 里，而 Bootloader 只能等串口，现场的串口可能根本没接出来。这一段的时长通常是几秒到几十秒，这个窗口不算小。

<Quiz
  type="multi"
  :options="[
    { label: '单槽方案：应用槽刚擦除，新映像还没写完', value: 'A' },
    { label: '本页方案：备份槽复制完成之后，才开始擦应用槽', value: 'B' },
    { label: '本页方案：应用槽擦到一半时断电，备份槽完好', value: 'C' },
    { label: '备份还没完成，就开始擦应用槽', value: 'D' }
  ]"
  :answer="['A', 'D']"
>

<template #question>

下面哪些做法会在断电时让设备失去唯一的可启动映像？

</template>

正确答案是 **A、D**。

- A 是单槽覆盖的典型窗口：唯一的一份被擦掉，新映像没写完就断电，设备停在 Bootloader 里。
- D 等于把两个槽同时置于不确定状态：应用槽正在被擦，备份槽还没写完，两边都不完整。
- B 与 C 都在「另一个槽完整」的前提下动当前槽，断电最坏的结果是这次升级作废，设备仍能启动。

</Quiz>

双槽与「暂存 + 备份」解决同一个问题，取舍不同：

| 方案 | 需要的空间 | 安装时间 | 掉电后的状态 |
| --- | --- | --- | --- |
| 单槽覆盖 | 1 份应用 | 最短 | 有窗口，可能无映像可启动 |
| 片内双槽 | 2 份应用 | 复制一份 | 始终有一个可启动 |
| 暂存 + 备份（本页） | 片内 1 份 + 外部 2 份 | 备份一次 + 安装一次 | 始终有一个可启动 |

F103C8 只有 64 KiB，片内放不下两份应用，所以只能把旧映像放到外部 Flash。片上 Flash 够大的芯片（比如 256 KiB 以上）可以直接用片内双槽，速度更快，也不依赖外部器件。

MCUboot 的三种交换方式值得了解一下，它们对应的正是上面这几种取舍：

- **swap using scratch**：两个槽都在片内，交换时借一小块 scratch 区做周转，逐块交换。掉电安全，需要 scratch 空间。
- **swap using move**：把应用拆成若干块，按块搬到目标位置再搬下一块，不需要 scratch，代价是块的大小要仔细选，逻辑更复杂。
- **overwrite only**：直接把新映像覆盖到应用槽，通常配合「先复制到备份槽」或依赖外部手段恢复。空间最省，掉电安全要靠额外设计。

作者判断：资源允许时优先用 MCUboot。它把交换、签名、回滚、防回滚都做完了，自己写这套东西的代价主要在测试——上面那个断电扫描只覆盖了安装阶段，真实的掉电点还包括擦除、日志写入、暂存写入，每个都要考虑。如果坚持自己写，至少把这一节的状态机和「记录只追加」的做法照搬过去，并且像 6.3 那样把断电测试做成可以重复跑的用例。

### 7.1 擦写期间 CPU 会停住

F103 的 Flash 控制器在擦除或编程时会把总线挂起。CPU 从 Flash 取指，取不到就停在那里，直到操作结束。一页 1 KiB 的擦除要几毫秒到几十毫秒，写满一页又要几毫秒。安装一份接近 40 KiB 的应用要擦写四十来页，按典型值估算，累计的停顿在一秒量级。

这对 Bootloader 有两个实际影响。第一，看门狗：IWDG 由独立的 LSI 时钟驱动，CPU 停住它也照常计数，1 秒的超时很容易在安装过程中被触发。要么在每次擦写前喂狗，要么把 IWDG 的超时按最坏安装时间放大，安装完成后再改回来。第二，中断：停住期间到来的中断不会被丢弃，操作结束后会按优先级依次执行，但如果中断处理函数本身也在 Flash 里，它同样要等。升级期间不响应中断是合理的选择，前提是 Bootloader 里没有任何依赖中断的功能。

如果确实需要在擦写期间保持响应，可以把擦写循环放进 RAM 执行：链接脚本里给这段代码单独一个段，启动时复制到 RAM，函数加 `__attribute__((section(".RamFunc")))`。`stm32f103c8.ld` 里已经为 `.RamFunc` 留了收集规则，用法见 [1.8](/教程/正文/嵌入式/1_平台与工具链/1_8_复位_启动与链接)。升级这种一次性动作一般不值得这么做，喂狗更简单。

<Quiz
  type="single"
  :options="[
    { label: 'IWDG 的时钟来自 LSI，CPU 停住时它照常计数，安装期间可能复位', value: 'A' },
    { label: '擦写期间 SysTick 中断频率升高，接管了 CPU', value: 'B' },
    { label: '擦写结束后所有外设自动复位，应用配置丢失', value: 'C' },
    { label: 'Flash 擦写会把 VTOR 清零，中断跳回 Bootloader', value: 'D' }
  ]"
  answer="A"
>

<template #question>

Bootloader 安装一份 40 KiB 的应用时，看门狗为什么必须在擦写循环里喂？

</template>

正确答案是 **A**。CPU 等 Flash 时停止取指，但 IWDG 由独立的 LSI 振荡器驱动，计数不受影响。B、C、D 描述的现象都不存在。

</Quiz>

## 8. 习题

### 8.1 巩固

<Exercise id="66625" :d="1" :w="2">

正文把 64 KiB Flash 划成 16 KiB Bootloader、47 KiB 应用槽与 1 KiB 配置页，映像头 0x200 字节。下面五组参数都想放进从 0x08004000 开始的应用槽：

| 组 | 头部长度 | 负载长度 |
| --- | --- | --- |
| A | 0x200 | 0xB800 |
| B | 0x100 | 0xB900 |
| C | 0x400 | 0xB800 |
| D | 0x200 | 0xBA00 |
| E | 0x400 | 0xBA00 |

(1) 逐组算出向量表地址，判断它是不是 0x200 的整数倍。哪一组写入 VTOR 之后，向量表实际会落到别的位置？

(2) 逐组算出「槽起始 + 头部 + 负载」的末地址（不含），与配置页起始 0x0800FC00 比较。哪些组刚好填满，哪些组还有余量，哪一组已经压进配置页？

(3) 正文 `.isr_vector` 输出的前两个字是 `00500020` 与 `f1420008`。按小端读出来，说明第一个字为什么等于 `_estack`，第二个字去掉最低位之后指向哪里。

(4) 47 KiB 是 0xBC00 字节。减去 0x200 字节的头部，负载上限是多少？与正文的 0xBA00 核对，并指出它对应 (2) 中的哪一组。

</Exercise>

<Exercise id="66626" :d="2" :w="2">

一个 `app.img` 的前 32 字节如下，地址从 0x08004000 起，小端：

```text
08004000: 4D 44 52 42 01 00 00 02 80 05 00 00 00 42 00 08
08004010: 00 02 01 00 01 00 00 00 F5 0E CC 10 21 10 26 20
```

字段顺序按正文第 4 节的 `image_header_t`：`magic`、`header_version`、`header_size`、`image_size`、`load_addr`、`fw_version`、`hw_version`、`payload_crc32`、`build_id`，其余填 0。

(1) 逐字段写出十六进制值与含义，做成一张表。

(2) `image_size` 是多少字节？负载多少字节？CRC 覆盖的地址范围从哪到哪（末地址不含）？CRC 覆盖的第一个字节在文件里偏移多少？

(3) 生成工具只把 `image_size` 少写 128 字节、CRC 仍按完整负载算，Bootloader 会接受这份映像吗？如果生成工具同时把 CRC 也改成按前 768 字节算，Bootloader 会接受，此时映像末尾那 128 字节处于什么状态？

(4) 魔数在内存里是 `4D 44 52 42`。写出 `IMAGE_MAGIC` 的数值，并说明它与 `'M' | ('D' << 8) | ('R' << 16) | ('B' << 24)` 为什么相等。

</Exercise>

<Exercise id="66627" :d="2" :w="3">

配置页里的升级日志每条记录 8 字节：`tag`、`arg`、`arg2`（小端 16 位）、前 4 字节的 CRC-32。`tag` 的取值是 1 已暂存、2 已备份、3 已安装、4 试运行、5 已确认、6 已回滚。从 0x0800FC00 读出的一段内容如下：

```text
0800FC00: 01 00 00 00 79 B8 F8 99 02 00 00 00 97 17 4D 8B
0800FC10: 03 00 00 00 F2 70 F1 33 04 01 00 00 7C 22 E4 AF
0800FC20: 04 02 FF FF FF FF FF FF 05 00 00 00 2E 2F 9A 16
0800FC30: FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF
```

(1) 逐条切分并校验 CRC。哪几条有效？对不上的那一条是怎么产生的？

(2) 按有效记录推出当前状态，并写出下一次上电 Bootloader 依次做什么。

(3) 把 0x0800FC20 处的 8 字节改写成一条合法的试运行记录（`arg` 为 2），写出这 8 个字节，并用正文的半字节表 CRC-32 验证。改完之后再读一遍日志，最终状态变不变？为什么？

(4) 这段日志之后还剩多少个记录位置？按一次升级 6 条算还能升级几次？日志写满时正文没有实现的处理有哪两种，代价各是什么？

</Exercise>

<Exercise id="66628" :d="3" :w="3">

正文的 `vectors_plausible()` 在跳转前检查向量表的前两项。下面是 12 组取自真实产物的「初始栈指针、复位向量」，应用槽仍然从 0x08004000 起、上限按正文的 0xBC00 计算：

```text
 0  20005000 080042F1
 1  20000000 080042F1
 2  20005004 080042F1
 3  20004FFE 080042F1
 4  20005000 080042F0
 5  20005000 08003FFD
 6  20005000 0800FC01
 7  20005000 0800BC01
 8  20005001 080042F1
 9  20004FFC 0800FBFF
10  20005000 08004201
11  20000004 08004201
```

(1) 实现这个函数，把 12 组数据的判定结果列成表。每判 false 的，写出是被哪一条条件挡下的。

(2) 这十二组里有五组判 true。自己再构造两组「判 true、跳过去却一定出问题」的组合，说明这个检查为什么挡不住它们。

(3) 第 3 组的栈指针是偶数，为什么仍然不合格？第 4 组的复位向量为 0x080042F0，为什么不合格？

(4) 如果把这个函数搬到主机上做单元测试，需要把哪些依赖去掉？写成什么样才能直接测 12 组数据？

</Exercise>

### 8.2 编程

<Exercise id="66629" :d="3" :w="4">

写主机端工具 `mkimage`，把裸二进制包装成正文格式的 `app.img`。

命令行：`mkimage <input.bin> <output.img> <主>.<次>.<修订> <硬件版本>`，例如 `mkimage app.bin app.img 1.2.0 0x0001`。

(1) 用 16 项半字节表实现 CRC-32，并提供 `--selftest` 入口。验收值：字符串 `"123456789"` 得 0xCBF43926；64 个 0 字节得 0x758D6336；字节 0～63 依次为 0..63 得 0x100ECE8C。

(2) 用 `payload[i] = i * 37 + 11`（按 256 取模）生成 896 字节输入，头部写 `fw_version = 主 << 16 | 次 << 8 | 修订`、`hw_version = 0x0001`、`build_id = 0x20261021`。输出文件的 CRC 应为 0x10CC0EF5，前 32 字节应为：

```text
08004000: 4D 44 52 42 01 00 00 02 80 05 00 00 00 42 00 08
08004010: 00 02 01 00 01 00 00 00 F5 0E CC 10 21 10 26 20
```

(3) 把输入的偏移 100 处改成 0，重新生成，负载 CRC 应变成 0xB1327FBF。用一个独立的校验程序确认这一点。

(4) 解释为什么表是 16 项而不是 256 项：查表次数、表的字节数、执行时间各差多少？片上 CRC 单元为什么不能替代这个实现？

</Exercise>

<Exercise id="66630" :d="4" :w="3">

下面这段跳转函数来自一个学生项目，能跑通演示，但不符合正文的要求：

```c
static void jump_to_app(uint32_t vt) {
    __disable_irq();
    void (*entry)(void) = (void (*)(void))((const uint32_t *)vt)[1];
    __set_MSP(((const uint32_t *)vt)[0]);
    SCB->VTOR = vt;
    entry();
}
```

(1) 与正文的 `jump_to_app` 逐项对比，找出至少四处差异。提示：VTOR 与 MSP 的先后、屏障指令、外设与 NVIC 的清理、PRIMASK 最终的值、复位向量的最低位。逐条说明各自的后果。

(2) 按正文的顺序改写，保留 `__disable_irq()`。

(3) 用 `arm-none-eabi-gcc -mcpu=cortex-m3 -mthumb -O2 -c` 编译，再 `objdump -d`。因为 `jump_to_app` 是 `static` 且只被调用一次，`-O2` 会把它整个内联进调用者；找出编译出来的那几条指令，写出 VTOR、MSP、BLX 的先后，与正文的六条对照，并指出这里少了哪两条屏障、多了什么乱序。

(4) 用 `-O0` 再编译一次，观察 `__set_MSP` 前后对栈帧的访问。说明把 `sp`、`pc` 放在局部对象里、又在换栈之后使用，风险在哪里。

(5) 应用侧漏写 `__enable_irq()` 时现象是什么？为什么单步调试时看不出来。

</Exercise>

<Exercise id="66631" :d="4" :w="4">

在主机上实现日志型的升级状态记录，并验证记录写到一半被打断时的行为。

(1) 定义 `journal_rec_t` 与两个函数：`int journal_append(nvm_t *nvm, uint8_t tag, uint8_t arg, uint16_t arg2)`，以及 `int journal_state(const nvm_t *nvm, uint8_t *arg)`，后者从头扫描，返回最后一条有效记录的 `tag`。

(2) 模拟器给 `nvm_program` 一个字节预算，每写一字节减一，减到 0 之后所有写操作失败，模拟这一刻断电。构造三个断电点：记录第 1 个字节写完后、前 4 字节写完后、CRC 的 4 字节写到一半。

(3) 每个断电点之后重新扫描日志，写出状态值，并说明下一次 `journal_append` 会把新记录写到哪个偏移。为什么写到一半的记录不会被覆盖？

(4) 先写 128 条记录，再写第 129 条，函数应返回什么？写满之后仍要升级，有哪两种做法，各自要处理什么问题？

(5) 如果 `journal_state` 扫到一条 CRC 正确、但 `tag` 是不认识的取值，它应该返回什么？给出你的处理和理由。

</Exercise>

<Exercise id="66632" :d="5" :w="5">

正文说安装一份 40 KiB 的应用要擦写四十来页，累计停顿在一秒量级。用下面这组参数把这句话算实，并把断电扫描做成可重复运行的测试。

| 操作 | 时长 |
| --- | --- |
| 片内 Flash 擦一页（1 KiB） | 20 ms |
| 片内 Flash 写一个半字 | 52 μs |
| W25Q64 擦一个扇区（4 KiB） | 45 ms |
| W25Q64 写一页（256 字节） | 0.7 ms |
| 配置页追加一条记录 | 3 ms |

(1) 算出安装 40 KiB（40 页、每页 512 个半字）需要多长时间，备份 40 KiB 到 W25Q64 需要多长时间，两者相加是多少。列出算式，并与正文「累计的停顿在一秒量级」这句话比较：哪一部分占了主要时间，正文的估算偏大还是偏小？

(2) IWDG 的超时是 1 s，由独立的 LSI 驱动，CPU 等 Flash 时照常计数。喂狗点最密可以放在哪里、最疏必须放在哪里？算出两次喂狗之间的最长间隔，与 1 s 比较。

(3) 写一个断电扫描：把「可编程字节预算」从 0 逐个增加，每一步都从头跑一遍升级流程，在预算耗尽处模拟掉电并重新上电，断言「应用槽合法或备份槽合法」至少有一个成立。按下面两种粒度各要跑多少轮：以一次 `nvm_program` 调用为一步（安装 40 KiB = 40 页 × 512 个半字）；以 1 ms 的写入量为一步。

(4) 对单槽覆盖的版本跑同一个扫描，统计违反上述性质的步数占全部步数的比例。这个比例说明什么？

(5) 回滚路径（备份槽恢复到应用槽）是否也要纳入扫描？说明理由，并按 (1) 的参数算出它在 64 KiB 布局下的时长。

</Exercise>

### 8.3 综合与挑战

<Exercise id="66633" :d="6" :w="4" scope="*">

64 KiB 的 F103C8 上，团队在「继续用暂存加备份」与「改用 MCUboot」之间选择。用空间与测试成本做一次定量比较。

(1) 若改用片内双槽：Bootloader 16 KiB、配置页 1 KiB、剩下 47 KiB 平分给两个槽，每槽去掉 0x200 字节头部之后，负载上限是多少 KiB？写出计算过程。

(2) 当前的应用 896 字节，显然放得下。给出一个判据：应用增长到多少字节时，片内双槽不再可行，只能退回暂存加备份。

(3) MCUboot 的三种交换方式里，swap using scratch 需要一块额外的 scratch 区，swap using move 不需要，overwrite only 最省空间但掉电不安全。在 (1) 的布局下，哪些方式还能再挤出空间？scratch 至少占多少（按一个扇区还是按一页算）？

(4) 给出一张三行的选择表，列为「空间」「安装时间」「掉电后的状态」「测试工作量」，行分别为单槽覆盖、暂存加备份、片内双槽。

(5) 作者判断是资源允许时优先用 MCUboot。列出自己写这套东西时，除安装流程之外还必须测到的四个断电点，并说明每个点各要断言什么。

</Exercise>

## 9. 参考资料

1. Memfault, [*From Zero to main(): How to Write a Bootloader from Scratch*](https://interrupt.memfault.com/blog/how-to-write-a-bootloader-from-scratch)：映像头、跳转前的外设清理、向量表重定位的工程做法。
2. Memfault, [*MCUboot Walkthrough and Porting Guide*](https://interrupt.memfault.com/blog/mcuboot-overview)：MCUboot 的槽布局、映像格式与移植步骤。
3. MCUboot, [*Design*](https://docs.mcuboot.com/design.html)：swap using scratch、swap using move、overwrite only 三种方式与掉电安全设计。
4. STMicroelectronics, [*PM0056: STM32F10xxx/20xxx/21xxx/L1xxxx Cortex-M3 programming manual*](https://www.st.com/resource/en/programming_manual/pm0056-stm32f10xxx20xxx21xxxl1xxxx-cortexm3-programming-manual-stmicroelectronics.pdf)：VTOR 的位分配与 512 字节对齐要求，SCB 与 NVIC 寄存器。
5. STMicroelectronics, [*PM0075: STM32F10xxx Flash memory programming manual*](https://www.st.com/resource/en/programming_manual/pm0075-stm32f10xxx-flash-memory-microcontrollers-stmicroelectronics.pdf)：页擦除、编程、写保护与选项字节。
6. STMicroelectronics, [*AN2606: Introduction to system memory boot mode on STM32 MCUs*](https://www.st.com/resource/en/application_note/an2606-introduction-to-system-memory-boot-mode-on-stm32-mcus-stmicroelectronics.pdf)：F103 的 BOOT 引脚组合与系统存储器引导。
7. STMicroelectronics, [*AN3155: USART protocol used in the STM32 bootloader*](https://www.st.com/resource/en/application_note/an3155-usart-protocol-used-in-the-stm32-bootloader-stmicroelectronics.pdf)：系统存储器引导程序的命令集。
8. STMicroelectronics, [*RM0008 Reference manual*, Rev 21](https://www.st.com/resource/en/reference_manual/rm0008-stm32f101xx-stm32f102xx-stm32f103xx-stm32f105xx-and-stm32f107xx-advanced-armbased-32bit-mcus-stmicroelectronics.pdf)：第 3 章 Flash、第 29 章 CRC 计算单元。
9. [*Catalogue of parametrised CRC algorithms*](https://reveng.sourceforge.io/crc-catalogue/17plus.htm)：CRC-32（IEEE 802.3）与 CRC-32/MPEG-2 的参数与校验值。
10. 本页的终端输出均为实测：主机侧（CRC-32、`mkimage`、升级状态机与断电测试）在 MinGW gcc 13.2 上编译运行，Bootloader 与应用用 Arm GNU Toolchain 13.3 编译链接，反汇编与段布局取自实际产物。正文给出了复现所需的全部关键代码，其余部分（头文件、`main` 的调用顺序）按正文说明补上即可。
