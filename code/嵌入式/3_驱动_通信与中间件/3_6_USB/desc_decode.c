/*
 * desc_decode.c：在主机上对比未打包结构体的填充偏移，并解码 USB 设备描述符、
 * CDC ACM 配置描述符集合与 UTF-16LE 字符串描述符。
 *
 * 编译运行：
 *   gcc -std=c17 -Wall -Wextra desc_decode.c -o desc_decode
 *   ./desc_decode
 */
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

/* 错误示范：不加打包属性时，编译器在奇数字节偏移后插入填充字节 */
struct cdc_header_desc_bad {
    uint8_t b_length;
    uint8_t b_descriptor_type;
    uint8_t b_descriptor_subtype;
    /* 编译器在此插入 1 字节填充，使 uint16_t 对齐到偶数偏移 */
    uint16_t bcd_cdc;
};

struct usb_config_desc_bad {
    uint8_t b_length;
    uint8_t b_descriptor_type;
    uint16_t w_total_length;
    uint8_t b_num_interfaces;
    uint8_t b_configuration_value;
    uint8_t i_configuration;
    uint8_t bm_attributes;
    uint8_t b_max_power;
    /* 编译器在末尾补 1 字节填充，使结构体大小为 2 的倍数 */
};

/* 正确写法之一：显式指定 packed 属性（GCC / Clang） */
struct __attribute__((packed)) cdc_header_desc_packed {
    uint8_t b_length;
    uint8_t b_descriptor_type;
    uint8_t b_descriptor_subtype;
    uint16_t bcd_cdc;
};

_Static_assert(sizeof(struct cdc_header_desc_packed) == 5,
               "CDC header functional descriptor must be 5 bytes");

/* 18 字节 USB 设备描述符（使用 IAD 的 CDC ACM 设备） */
static const uint8_t g_device_desc[18] = {
    0x12,       /* bLength = 18 */
    0x01,       /* bDescriptorType = DEVICE (0x01) */
    0x00, 0x02, /* bcdUSB = 0x0200 (USB 2.0，小端) */
    0xEF,       /* bDeviceClass = 0xEF (Miscellaneous) */
    0x02,       /* bDeviceSubClass = 0x02 (Common Class) */
    0x01,       /* bDeviceProtocol = 0x01 (IAD) */
    0x40,       /* bMaxPacketSize0 = 64 */
    0xFE, 0xCA, /* idVendor = 0xCAFE（实验用 VID，小端） */
    0x01, 0x40, /* idProduct = 0x4001（小端） */
    0x00, 0x01, /* bcdDevice = 0x0100 */
    0x01,       /* iManufacturer = 1 */
    0x02,       /* iProduct = 2 */
    0x03,       /* iSerialNumber = 3 */
    0x01        /* bNumConfigurations = 1 */
};

/*
 * 75 字节配置描述符集合（与 TinyUSB 的 TUD_CONFIG_DESCRIPTOR +
 * TUD_CDC_DESCRIPTOR 展开结果一致）：
 *   9B 配置 + 8B IAD + 9B CDC 控制接口 + 5B Header + 5B Call Mgmt +
 *   4B ACM + 5B Union + 7B 通知端点(0x81) + 9B 数据接口 +
 *   7B 批量 OUT(0x02) + 7B 批量 IN(0x82) = 75 字节
 */
static const uint8_t g_config_desc[75] = {
    /* 配置描述符（9 字节） */
    0x09, 0x02, 0x4B, 0x00, 0x02, 0x01, 0x00, 0x80, 0x32,
    /* 接口关联描述符 IAD（8 字节） */
    0x08, 0x0B, 0x00, 0x02, 0x02, 0x02, 0x00, 0x04,
    /* 接口 0：CDC 通信控制接口（9 字节） */
    0x09, 0x04, 0x00, 0x00, 0x01, 0x02, 0x02, 0x00, 0x04,
    /* CDC Header 功能描述符（5 字节，bcdCDC = 0x0120） */
    0x05, 0x24, 0x00, 0x20, 0x01,
    /* CDC Call Management 功能描述符（5 字节，数据接口 = 1） */
    0x05, 0x24, 0x01, 0x00, 0x01,
    /* CDC ACM 功能描述符（4 字节，bmCapabilities = 0x06） */
    0x04, 0x24, 0x02, 0x06,
    /* CDC Union 功能描述符（5 字节，主接口 0，从接口 1） */
    0x05, 0x24, 0x06, 0x00, 0x01,
    /* 通知端点描述符：EP1 IN，中断传输，最大包 8 字节，间隔 16 ms（7 字节） */
    0x07, 0x05, 0x81, 0x03, 0x08, 0x00, 0x10,
    /* 接口 1：CDC 数据接口（9 字节） */
    0x09, 0x04, 0x01, 0x00, 0x02, 0x0A, 0x00, 0x00, 0x00,
    /* 数据端点描述符：EP2 OUT，批量传输，最大包 64 字节（7 字节） */
    0x07, 0x05, 0x02, 0x02, 0x40, 0x00, 0x00,
    /* 数据端点描述符：EP2 IN，批量传输，最大包 64 字节（7 字节） */
    0x07, 0x05, 0x82, 0x02, 0x40, 0x00, 0x00
};

static uint16_t read_u16_le(const uint8_t *p) {
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

static const char *xfer_type_name(uint8_t bm_attr) {
    switch (bm_attr & 0x03u) {
    case 0:
        return "Control";
    case 1:
        return "Isochronous";
    case 2:
        return "Bulk";
    default:
        return "Interrupt";
    }
}

static void decode_device_descriptor(const uint8_t *d, size_t len) {
    if (len < 18u || d[0] != 18u || d[1] != 0x01u) {
        printf("Invalid device descriptor\n");
        return;
    }
    printf("=== Device Descriptor (18 bytes) ===\n");
    printf("  bcdUSB            : 0x%04X\n", read_u16_le(&d[2]));
    printf("  Class/Sub/Proto   : 0x%02X / 0x%02X / 0x%02X\n", d[4], d[5], d[6]);
    printf("  bMaxPacketSize0   : %u\n", d[7]);
    printf("  idVendor          : 0x%04X\n", read_u16_le(&d[8]));
    printf("  idProduct         : 0x%04X\n", read_u16_le(&d[10]));
    printf("  bcdDevice         : 0x%04X\n", read_u16_le(&d[12]));
    printf("  String indices    : Mfg=%u, Prod=%u, Serial=%u\n",
           d[14], d[15], d[16]);
    printf("  bNumConfigurations: %u\n\n", d[17]);
}

static void decode_config_bundle(const uint8_t *buf, size_t total_len) {
    printf("=== Configuration Descriptor Bundle (%u bytes) ===\n",
           (unsigned)total_len);
    size_t offset = 0u;
    while (offset < total_len) {
        uint8_t b_len = buf[offset];
        if (b_len == 0u || offset + b_len > total_len) {
            printf("  [offset %2u] malformed descriptor (bLength=%u)\n",
                   (unsigned)offset, b_len);
            break;
        }
        uint8_t b_type = buf[offset + 1u];
        const uint8_t *d = &buf[offset];

        printf("  [+%02u, %uB] type=0x%02X ", (unsigned)offset, b_len, b_type);
        if (b_type == 0x02u && b_len >= 9u) {
            printf("CONFIGURATION: wTotalLength=%u, numInterfaces=%u, "
                   "maxPower=%u mA\n",
                   read_u16_le(&d[2]), d[4], (unsigned)d[8] * 2u);
        } else if (b_type == 0x0Bu && b_len >= 8u) {
            printf("IAD: firstIf=%u, count=%u, class=0x%02X/0x%02X\n",
                   d[2], d[3], d[4], d[5]);
        } else if (b_type == 0x04u && b_len >= 9u) {
            printf("INTERFACE %u: numEP=%u, class=0x%02X/0x%02X/0x%02X\n",
                   d[2], d[4], d[5], d[6], d[7]);
        } else if (b_type == 0x24u && b_len >= 3u) {
            printf("CS_INTERFACE subtype=0x%02X\n", d[2]);
        } else if (b_type == 0x05u && b_len >= 7u) {
            uint8_t ep_addr = d[2];
            const char *dir = ((ep_addr & 0x80u) != 0u) ? "IN" : "OUT";
            printf("ENDPOINT 0x%02X (EP%u %s): %s, maxPacket=%u, interval=%u\n",
                   ep_addr, ep_addr & 0x0Fu, dir,
                   xfer_type_name(d[3]), read_u16_le(&d[4]), d[6]);
        } else {
            printf("OTHER\n");
        }
        offset += b_len;
    }
}

int main(void) {
    printf("=== Struct Alignment vs Wire Layout ===\n");
    printf("  cdc_header_desc_bad   : sizeof=%u, offsetof(bcd_cdc)=%u "
           "(wire expects 5, offset 3)\n",
           (unsigned)sizeof(struct cdc_header_desc_bad),
           (unsigned)offsetof(struct cdc_header_desc_bad, bcd_cdc));
    printf("  cdc_header_desc_packed: sizeof=%u, offsetof(bcd_cdc)=%u\n",
           (unsigned)sizeof(struct cdc_header_desc_packed),
           (unsigned)offsetof(struct cdc_header_desc_packed, bcd_cdc));
    printf("  usb_config_desc_bad   : sizeof=%u (wire expects 9)\n\n",
           (unsigned)sizeof(struct usb_config_desc_bad));

    decode_device_descriptor(g_device_desc, sizeof g_device_desc);
    decode_config_bundle(g_config_desc, sizeof g_config_desc);
    return 0;
}
