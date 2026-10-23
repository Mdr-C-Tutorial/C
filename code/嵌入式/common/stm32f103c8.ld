/*
 * stm32f103c8.ld：STM32F103C8 的链接脚本（64 KiB Flash，20 KiB SRAM）。
 *
 * 符号名沿用 ST 风格（_sidata、_sdata、_edata、_sbss、_ebss、_estack），
 * 便于对照 CubeMX 生成的工程。startup_stm32f103.c 用这些符号复制 .data、
 * 清零 .bss。讲解见教程 1.8 节「复位、启动与链接」。
 */

ENTRY(Reset_Handler)

MEMORY
{
    FLASH (rx)  : ORIGIN = 0x08000000, LENGTH = 64K
    RAM   (rwx) : ORIGIN = 0x20000000, LENGTH = 20K
}

/* 栈顶：RAM 末尾之后的第一个字节。复位时硬件从向量表第 0 项把它装入 MSP。 */
_estack = ORIGIN(RAM) + LENGTH(RAM);

/* 至少留给堆和栈的空间。RAM 放不下时链接报错，而不是等到运行时栈溢出。 */
_Min_Heap_Size = 0x200;
_Min_Stack_Size = 0x400;

SECTIONS
{
    /* 向量表必须在 Flash 起始处。没有代码引用它，用 KEEP 防止被 --gc-sections 回收。 */
    .isr_vector :
    {
        . = ALIGN(4);
        KEEP(*(.isr_vector))
        . = ALIGN(4);
    } > FLASH

    /* 代码 */
    .text :
    {
        . = ALIGN(4);
        *(.text)
        *(.text*)
        *(.glue_7)
        *(.glue_7t)
        *(.eh_frame)
        KEEP(*(.init))
        KEEP(*(.fini))
        . = ALIGN(4);
        _etext = .;
    } > FLASH

    /* 只读数据：const 对象、字符串字面量 */
    .rodata :
    {
        . = ALIGN(4);
        *(.rodata)
        *(.rodata*)
        . = ALIGN(4);
    } > FLASH

    /* 栈回溯表，库代码里可能带有 */
    .ARM.extab :
    {
        *(.ARM.extab* .gnu.linkonce.armextab.*)
    } > FLASH

    .ARM.exidx :
    {
        __exidx_start = .;
        *(.ARM.exidx*)
        __exidx_end = .;
    } > FLASH

    /* .data 的初值在 Flash 中的地址（LMA），启动代码从这里复制 */
    _sidata = LOADADDR(.data);

    /* 有初值的可写数据：运行地址（VMA）在 RAM，初值存放在 Flash */
    .data :
    {
        . = ALIGN(4);
        _sdata = .;
        *(.data)
        *(.data*)
        *(.RamFunc)
        *(.RamFunc*)
        . = ALIGN(4);
        _edata = .;
    } > RAM AT> FLASH

    /* 没有初值或初值为 0 的数据：启动代码清零 */
    .bss :
    {
        . = ALIGN(4);
        _sbss = .;
        __bss_start__ = _sbss;
        *(.bss)
        *(.bss*)
        *(COMMON)
        . = ALIGN(4);
        _ebss = .;
        __bss_end__ = _ebss;
    } > RAM

    /* 启动代码不碰的数据：软件复位、看门狗复位后保留上次的值（2.10 节使用） */
    .noinit (NOLOAD) :
    {
        . = ALIGN(4);
        _snoinit = .;
        *(.noinit)
        *(.noinit*)
        . = ALIGN(4);
        _enoinit = .;
    } > RAM

    /* 不放任何内容，只检查剩下的 RAM 够不够堆和栈 */
    ._user_heap_stack :
    {
        . = ALIGN(8);
        PROVIDE(end = .);
        PROVIDE(_end = .);
        . = . + _Min_Heap_Size;
        . = . + _Min_Stack_Size;
        . = ALIGN(8);
    } > RAM

    .ARM.attributes 0 : { *(.ARM.attributes) }
}
