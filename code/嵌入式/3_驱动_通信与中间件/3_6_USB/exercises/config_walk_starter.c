/*
 * config_walk_starter.c：习题 63348 起始代码。
 * 遍历一段 100 字节的复合设备（CDC ACM + HID 键盘）配置描述符集合，
 * 打印每个接口与端点的层次结构。
 *
 * 编译运行：
 *   gcc -std=c17 -Wall -Wextra config_walk_starter.c -o config_walk_test
 *   ./config_walk_test
 */
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

/* 100 字节复合设备配置描述符：75B (配置 + CDC ACM) + 25B (HID 接口 + HID 描述符 + 中断 IN 端点) */
static const uint8_t g_composite_config_desc[100] = {
    /* 配置描述符（9B）：wTotalLength = 100 (0x0064)，bNumInterfaces = 3 */
    0x09, 0x02, 0x64, 0x00, 0x03, 0x01, 0x00, 0x80, 0x32,
    /* IAD（8B）：把接口 0 和接口 1 关联为一个 CDC 功能 */
    0x08, 0x0B, 0x00, 0x02, 0x02, 0x02, 0x00, 0x04,
    /* 接口 0：CDC 控制接口（9B） */
    0x09, 0x04, 0x00, 0x00, 0x01, 0x02, 0x02, 0x00, 0x04,
    /* CDC 类专用功能描述符（5B + 5B + 4B + 5B） */
    0x05, 0x24, 0x00, 0x20, 0x01,
    0x05, 0x24, 0x01, 0x00, 0x01,
    0x04, 0x24, 0x02, 0x06,
    0x05, 0x24, 0x06, 0x00, 0x01,
    /* 端点 0x81：CDC 通知端点，中断 IN，8B，16 ms（7B） */
    0x07, 0x05, 0x81, 0x03, 0x08, 0x00, 0x10,
    /* 接口 1：CDC 数据接口（9B） */
    0x09, 0x04, 0x01, 0x00, 0x02, 0x0A, 0x00, 0x00, 0x00,
    /* 端点 0x02：批量 OUT，64B（7B） */
    0x07, 0x05, 0x02, 0x02, 0x40, 0x00, 0x00,
    /* 端点 0x82：批量 IN，64B（7B） */
    0x07, 0x05, 0x82, 0x02, 0x40, 0x00, 0x00,
    /* 接口 2：HID 键盘接口（9B）：class=0x03(HID), sub=0x01(Boot), proto=0x01(Keyboard) */
    0x09, 0x04, 0x02, 0x00, 0x01, 0x03, 0x01, 0x01, 0x05,
    /* HID 描述符（9B）：type=0x21，报告描述符长 65 字节 */
    0x09, 0x21, 0x11, 0x01, 0x00, 0x01, 0x22, 0x41, 0x00,
    /* 端点 0x83：HID 中断 IN，8B，10 ms（7B） */
    0x07, 0x05, 0x83, 0x03, 0x08, 0x00, 0x0A
};

static uint16_t read_u16_le(const uint8_t *p) {
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

static const char *xfer_name(uint8_t attr) {
    static const char *const names[4] = {
        "Control", "Isochronous", "Bulk", "Interrupt"
    };
    return names[attr & 0x03u];
}

void walk_config_descriptor(const uint8_t *buf, size_t len) {
    size_t pos = 0u;
    while (pos < len) {
        uint8_t b_len = buf[pos];
        if (b_len == 0u || pos + b_len > len) {
            break;
        }
        uint8_t b_type = buf[pos + 1u];
        const uint8_t *d = &buf[pos];

        if (b_type == 0x02u && b_len >= 9u) {
            printf("Config %u: total=%u bytes, interfaces=%u\n",
                   d[5], read_u16_le(&d[2]), d[4]);
        } else if (b_type == 0x04u && b_len >= 9u) {
            printf("  Interface %u: endpoints=%u, class=0x%02X, sub=0x%02X, proto=0x%02X\n",
                   d[2], d[4], d[5], d[6], d[7]);
        } else if (b_type == 0x05u && b_len >= 7u) {
            uint8_t ep = d[2];
            printf("    Endpoint 0x%02X (EP%u %s): %s, maxPacket=%u, interval=%u\n",
                   ep, ep & 0x0Fu, ((ep & 0x80u) != 0u) ? "IN" : "OUT",
                   xfer_name(d[3]), read_u16_le(&d[4]), d[6]);
        }
        pos += b_len;
    }
}

int main(void) {
    walk_config_descriptor(g_composite_config_desc, sizeof g_composite_config_desc);
    return 0;
}
