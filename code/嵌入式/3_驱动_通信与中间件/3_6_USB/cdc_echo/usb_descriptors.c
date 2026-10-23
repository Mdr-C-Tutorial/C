#include "tusb.h"
#include <string.h>

#define USB_VID 0xCAFEu
#define USB_PID 0x4001u
#define USB_BCD 0x0200u

/* 18 字节设备描述符：使用 IAD（0xEF / 0x02 / 0x01）捆绑 CDC 的两个接口 */
static const tusb_desc_device_t g_desc_device = {
    .bLength            = sizeof(tusb_desc_device_t),
    .bDescriptorType    = TUSB_DESC_DEVICE,
    .bcdUSB             = USB_BCD,
    .bDeviceClass       = TUSB_CLASS_MISC,
    .bDeviceSubClass    = MISC_SUBCLASS_COMMON,
    .bDeviceProtocol    = MISC_PROTOCOL_IAD,
    .bMaxPacketSize0    = CFG_TUD_ENDPOINT0_SIZE,
    .idVendor           = USB_VID,
    .idProduct          = USB_PID,
    .bcdDevice          = 0x0100u,
    .iManufacturer      = 0x01u,
    .iProduct           = 0x02u,
    .iSerialNumber      = 0x03u,
    .bNumConfigurations = 0x01u
};

uint8_t const *tud_descriptor_device_cb(void) {
    return (uint8_t const *)&g_desc_device;
}

enum {
    ITF_NUM_CDC = 0,
    ITF_NUM_CDC_DATA,
    ITF_NUM_TOTAL
};

#define EPNUM_CDC_NOTIF 0x81u
#define EPNUM_CDC_OUT   0x02u
#define EPNUM_CDC_IN    0x82u

#define CONFIG_TOTAL_LEN (TUD_CONFIG_DESC_LEN + TUD_CDC_DESC_LEN)

static const uint8_t g_desc_configuration[] = {
    /* 配置号 1，接口数 2，字符串索引 0，总长 75 字节，无远程唤醒，最大电流 100 mA */
    TUD_CONFIG_DESCRIPTOR(1, ITF_NUM_TOTAL, 0, CONFIG_TOTAL_LEN, 0x00, 100),
    /* CDC 控制接口 0，字符串索引 4，通知端点 0x81（8B），批量 OUT 0x02 / IN 0x82（64B） */
    TUD_CDC_DESCRIPTOR(ITF_NUM_CDC, 4, EPNUM_CDC_NOTIF, 8,
                       EPNUM_CDC_OUT, EPNUM_CDC_IN, 64)
};

_Static_assert(sizeof g_desc_configuration == 75,
               "CDC configuration descriptor bundle must be 75 bytes");

uint8_t const *tud_descriptor_configuration_cb(uint8_t index) {
    (void)index;
    return g_desc_configuration;
}

static char const *const g_string_desc_arr[] = {
    (const char[]){ 0x09, 0x04 }, /* 0: 语言 ID = 0x0409（美式英语） */
    "DreamCat Tutorial",          /* 1: 厂商字符串 */
    "STM32F103 CDC Echo",         /* 2: 产品字符串 */
    "F103-0001",                  /* 3: 序列号字符串 */
    "DreamCat Virtual COM"        /* 4: CDC 接口字符串 */
};

static uint16_t g_desc_str[32];

uint16_t const *tud_descriptor_string_cb(uint8_t index, uint16_t langid) {
    (void)langid;
    size_t chr_count = 0u;

    if (index == 0u) {
        memcpy(&g_desc_str[1], g_string_desc_arr[0], 2u);
        chr_count = 1u;
    } else {
        size_t arr_len = sizeof g_string_desc_arr / sizeof g_string_desc_arr[0];
        if (index >= arr_len) {
            return NULL;
        }
        const char *str = g_string_desc_arr[index];
        chr_count = strlen(str);
        if (chr_count > 31u) {
            chr_count = 31u;
        }
        /* 把 ASCII 字符零扩展为 UTF-16LE 编码单元 */
        for (size_t i = 0u; i < chr_count; ++i) {
            g_desc_str[1u + i] = (uint16_t)(uint8_t)str[i];
        }
    }

    /* 首字节为 bLength（2 + 2 * chr_count），次字节为 TUSB_DESC_STRING (0x03) */
    g_desc_str[0] = (uint16_t)(((uint16_t)TUSB_DESC_STRING << 8) |
                               (2u * chr_count + 2u));
    return g_desc_str;
}
