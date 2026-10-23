/*
 * startup_stm32f103.c：STM32F103x8/xB（中容量）的向量表与复位处理函数。
 *
 * 向量表放在 .isr_vector 段，由链接脚本 stm32f103c8.ld 放到 Flash 起始处
 * 0x08000000。表项依次是初始栈指针、15 个系统异常（含保留项）和 43 个外设
 * 中断，名字与 ST 的 startup_stm32f103xb.s 相同。除 Reset_Handler 外，所有
 * 处理函数都是 Default_Handler 的弱别名：程序里定义一个同名函数即可替换。
 *
 * 讲解见教程 1.8 节「复位、启动与链接」。
 */
#include <stdint.h>

/* 链接脚本定义的符号：只有地址，没有存储。声明成数组，直接当地址用。 */
extern uint32_t _sidata[]; /* .data 初值在 Flash 中的起始地址（LMA） */
extern uint32_t _sdata[];  /* .data 在 RAM 中的起始地址（VMA） */
extern uint32_t _edata[];  /* .data 在 RAM 中的结束地址 */
extern uint32_t _sbss[];   /* .bss 的起始地址 */
extern uint32_t _ebss[];   /* .bss 的结束地址 */
extern uint32_t _estack[]; /* 栈顶：RAM 末尾之后的第一个字节 */

int main(void);

void Reset_Handler(void);
void Default_Handler(void);

#define DEFAULT_HANDLER __attribute__((weak, alias("Default_Handler")))

/* Cortex-M3 系统异常 */
void NMI_Handler(void) DEFAULT_HANDLER;
void HardFault_Handler(void) DEFAULT_HANDLER;
void MemManage_Handler(void) DEFAULT_HANDLER;
void BusFault_Handler(void) DEFAULT_HANDLER;
void UsageFault_Handler(void) DEFAULT_HANDLER;
void SVC_Handler(void) DEFAULT_HANDLER;
void DebugMon_Handler(void) DEFAULT_HANDLER;
void PendSV_Handler(void) DEFAULT_HANDLER;
void SysTick_Handler(void) DEFAULT_HANDLER;

/* 外设中断（RM0008 表 63，位置 0～42） */
void WWDG_IRQHandler(void) DEFAULT_HANDLER;
void PVD_IRQHandler(void) DEFAULT_HANDLER;
void TAMPER_IRQHandler(void) DEFAULT_HANDLER;
void RTC_IRQHandler(void) DEFAULT_HANDLER;
void FLASH_IRQHandler(void) DEFAULT_HANDLER;
void RCC_IRQHandler(void) DEFAULT_HANDLER;
void EXTI0_IRQHandler(void) DEFAULT_HANDLER;
void EXTI1_IRQHandler(void) DEFAULT_HANDLER;
void EXTI2_IRQHandler(void) DEFAULT_HANDLER;
void EXTI3_IRQHandler(void) DEFAULT_HANDLER;
void EXTI4_IRQHandler(void) DEFAULT_HANDLER;
void DMA1_Channel1_IRQHandler(void) DEFAULT_HANDLER;
void DMA1_Channel2_IRQHandler(void) DEFAULT_HANDLER;
void DMA1_Channel3_IRQHandler(void) DEFAULT_HANDLER;
void DMA1_Channel4_IRQHandler(void) DEFAULT_HANDLER;
void DMA1_Channel5_IRQHandler(void) DEFAULT_HANDLER;
void DMA1_Channel6_IRQHandler(void) DEFAULT_HANDLER;
void DMA1_Channel7_IRQHandler(void) DEFAULT_HANDLER;
void ADC1_2_IRQHandler(void) DEFAULT_HANDLER;
void USB_HP_CAN1_TX_IRQHandler(void) DEFAULT_HANDLER;
void USB_LP_CAN1_RX0_IRQHandler(void) DEFAULT_HANDLER;
void CAN1_RX1_IRQHandler(void) DEFAULT_HANDLER;
void CAN1_SCE_IRQHandler(void) DEFAULT_HANDLER;
void EXTI9_5_IRQHandler(void) DEFAULT_HANDLER;
void TIM1_BRK_IRQHandler(void) DEFAULT_HANDLER;
void TIM1_UP_IRQHandler(void) DEFAULT_HANDLER;
void TIM1_TRG_COM_IRQHandler(void) DEFAULT_HANDLER;
void TIM1_CC_IRQHandler(void) DEFAULT_HANDLER;
void TIM2_IRQHandler(void) DEFAULT_HANDLER;
void TIM3_IRQHandler(void) DEFAULT_HANDLER;
void TIM4_IRQHandler(void) DEFAULT_HANDLER;
void I2C1_EV_IRQHandler(void) DEFAULT_HANDLER;
void I2C1_ER_IRQHandler(void) DEFAULT_HANDLER;
void I2C2_EV_IRQHandler(void) DEFAULT_HANDLER;
void I2C2_ER_IRQHandler(void) DEFAULT_HANDLER;
void SPI1_IRQHandler(void) DEFAULT_HANDLER;
void SPI2_IRQHandler(void) DEFAULT_HANDLER;
void USART1_IRQHandler(void) DEFAULT_HANDLER;
void USART2_IRQHandler(void) DEFAULT_HANDLER;
void USART3_IRQHandler(void) DEFAULT_HANDLER;
void EXTI15_10_IRQHandler(void) DEFAULT_HANDLER;
void RTC_Alarm_IRQHandler(void) DEFAULT_HANDLER;
void USBWakeUp_IRQHandler(void) DEFAULT_HANDLER;

typedef void (*vector_entry)(void);

#define VECTOR_COUNT (16 + 43)

/*
 * used 让编译器即使看不到引用也照样生成这张表；链接时不被 --gc-sections
 * 回收则靠链接脚本里的 KEEP。数组不写长度，表项数与 VECTOR_COUNT 是否一致
 * 交给下面的静态断言检查。
 */
__attribute__((section(".isr_vector"), used))
const vector_entry g_vector_table[] = {
    (vector_entry)_estack,      /* 0x000 初始 MSP */
    Reset_Handler,              /* 0x004 复位 */
    NMI_Handler,                /* 0x008 */
    HardFault_Handler,          /* 0x00C */
    MemManage_Handler,          /* 0x010 */
    BusFault_Handler,           /* 0x014 */
    UsageFault_Handler,         /* 0x018 */
    0,                          /* 0x01C 保留 */
    0,                          /* 0x020 保留 */
    0,                          /* 0x024 保留 */
    0,                          /* 0x028 保留 */
    SVC_Handler,                /* 0x02C */
    DebugMon_Handler,           /* 0x030 */
    0,                          /* 0x034 保留 */
    PendSV_Handler,             /* 0x038 */
    SysTick_Handler,            /* 0x03C */
    WWDG_IRQHandler,            /* 0x040 IRQ 0 */
    PVD_IRQHandler,             /* 0x044 IRQ 1 */
    TAMPER_IRQHandler,          /* 0x048 IRQ 2 */
    RTC_IRQHandler,             /* 0x04C IRQ 3 */
    FLASH_IRQHandler,           /* 0x050 IRQ 4 */
    RCC_IRQHandler,             /* 0x054 IRQ 5 */
    EXTI0_IRQHandler,           /* 0x058 IRQ 6 */
    EXTI1_IRQHandler,           /* 0x05C IRQ 7 */
    EXTI2_IRQHandler,           /* 0x060 IRQ 8 */
    EXTI3_IRQHandler,           /* 0x064 IRQ 9 */
    EXTI4_IRQHandler,           /* 0x068 IRQ 10 */
    DMA1_Channel1_IRQHandler,   /* 0x06C IRQ 11 */
    DMA1_Channel2_IRQHandler,   /* 0x070 IRQ 12 */
    DMA1_Channel3_IRQHandler,   /* 0x074 IRQ 13 */
    DMA1_Channel4_IRQHandler,   /* 0x078 IRQ 14 */
    DMA1_Channel5_IRQHandler,   /* 0x07C IRQ 15 */
    DMA1_Channel6_IRQHandler,   /* 0x080 IRQ 16 */
    DMA1_Channel7_IRQHandler,   /* 0x084 IRQ 17 */
    ADC1_2_IRQHandler,          /* 0x088 IRQ 18 */
    USB_HP_CAN1_TX_IRQHandler,  /* 0x08C IRQ 19 */
    USB_LP_CAN1_RX0_IRQHandler, /* 0x090 IRQ 20 */
    CAN1_RX1_IRQHandler,        /* 0x094 IRQ 21 */
    CAN1_SCE_IRQHandler,        /* 0x098 IRQ 22 */
    EXTI9_5_IRQHandler,         /* 0x09C IRQ 23 */
    TIM1_BRK_IRQHandler,        /* 0x0A0 IRQ 24 */
    TIM1_UP_IRQHandler,         /* 0x0A4 IRQ 25 */
    TIM1_TRG_COM_IRQHandler,    /* 0x0A8 IRQ 26 */
    TIM1_CC_IRQHandler,         /* 0x0AC IRQ 27 */
    TIM2_IRQHandler,            /* 0x0B0 IRQ 28 */
    TIM3_IRQHandler,            /* 0x0B4 IRQ 29 */
    TIM4_IRQHandler,            /* 0x0B8 IRQ 30 */
    I2C1_EV_IRQHandler,         /* 0x0BC IRQ 31 */
    I2C1_ER_IRQHandler,         /* 0x0C0 IRQ 32 */
    I2C2_EV_IRQHandler,         /* 0x0C4 IRQ 33 */
    I2C2_ER_IRQHandler,         /* 0x0C8 IRQ 34 */
    SPI1_IRQHandler,            /* 0x0CC IRQ 35 */
    SPI2_IRQHandler,            /* 0x0D0 IRQ 36 */
    USART1_IRQHandler,          /* 0x0D4 IRQ 37 */
    USART2_IRQHandler,          /* 0x0D8 IRQ 38 */
    USART3_IRQHandler,          /* 0x0DC IRQ 39 */
    EXTI15_10_IRQHandler,       /* 0x0E0 IRQ 40 */
    RTC_Alarm_IRQHandler,       /* 0x0E4 IRQ 41 */
    USBWakeUp_IRQHandler,       /* 0x0E8 IRQ 42 */
};

/* C11：表项数目写错时编译失败 */
_Static_assert(sizeof g_vector_table == VECTOR_COUNT * sizeof(vector_entry),
               "vector table must have 16 + 43 entries");

/*
 * 复位后硬件已经从向量表取出 MSP，栈可以用，所以这个函数可以用 C 写。
 * 但在两个循环结束之前，任何静态存储期对象都还没有正确的值，这里只能使用
 * 自动对象和链接脚本给出的地址。
 */
void Reset_Handler(void) {
    /* 把 .data 的初值从 Flash 复制到 RAM */
    uint32_t *src = _sidata;
    uint32_t *dst = _sdata;
    while (dst < _edata) {
        *dst++ = *src++;
    }

    /* 把 .bss 清零 */
    for (dst = _sbss; dst < _ebss; ++dst) {
        *dst = 0;
    }

    main();

    /* main 不应返回；万一返回，停在这里 */
    for (;;) {
    }
}

/* 没有自己处理函数的异常和中断都进入这里，停住，便于调试器查看现场 */
void Default_Handler(void) {
    for (;;) {
    }
}
