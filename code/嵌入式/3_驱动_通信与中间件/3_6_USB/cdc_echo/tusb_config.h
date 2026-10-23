#ifndef TUSB_CONFIG_H
#define TUSB_CONFIG_H

/* TinyUSB 的 fsdev_common.c 使用了 GNU 扩展关键字 asm；在 -std=c11 下映射到 __asm */
#ifndef asm
#define asm __asm
#endif

#ifndef CFG_TUSB_MCU
#define CFG_TUSB_MCU OPT_MCU_STM32F1
#endif

#ifndef CFG_TUSB_OS
#define CFG_TUSB_OS OPT_OS_NONE
#endif

#define CFG_TUD_ENABLED       1
#define CFG_TUSB_RHPORT0_MODE (OPT_MODE_DEVICE | OPT_MODE_FULL_SPEED)

/* 端点 0 最大包大小：全速设备可取 8、16、32 或 64，常取 64 */
#define CFG_TUD_ENDPOINT0_SIZE 64

/* 启用 1 个 CDC ACM 虚拟串口，不启用其他设备类 */
#define CFG_TUD_CDC    1
#define CFG_TUD_MSC    0
#define CFG_TUD_HID    0
#define CFG_TUD_MIDI   0
#define CFG_TUD_VENDOR 0

/* CDC 软件环形缓冲区与端点临时缓冲区大小（字节） */
#define CFG_TUD_CDC_RX_BUFSIZE 64
#define CFG_TUD_CDC_TX_BUFSIZE 64
#define CFG_TUD_CDC_EP_BUFSIZE 64

#endif /* TUSB_CONFIG_H */
