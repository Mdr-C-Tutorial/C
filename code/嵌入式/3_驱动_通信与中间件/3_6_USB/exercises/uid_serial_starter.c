/*
 * uid_serial_starter.c：习题 63349 起始代码。
 * 把 STM32F103 的 96 位（12 字节）唯一设备 ID 编码为 24 个大写十六进制字符的
 * UTF-16LE USB 字符串描述符。
 *
 * 主机自测编译运行：
 *   gcc -std=c17 -Wall -Wextra uid_serial_starter.c -o uid_serial_test
 *   ./uid_serial_test
 */
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#define USB_DESC_STRING 0x03u

/*
 * 根据 12 字节 uid 生成 USB 字符串描述符并写入 out_desc_str（至少 25 个 uint16_t）。
 * out_desc_str[0] 的低字节为 bLength（50），高字节为 0x03（STRING 描述符类型）；
 * out_desc_str[1..24] 为 24 个大写十六进制字符的 UTF-16LE 编码单元。
 * 返回 bLength（字节数，即 50）。
 */
size_t uid_to_string_desc(const uint8_t uid[12], uint16_t out_desc_str[25]) {
    static const char hex_digits[16] = "0123456789ABCDEF";
    size_t chr_count = 24u;

    for (size_t i = 0u; i < 12u; ++i) {
        uint8_t b = uid[i];
        out_desc_str[1u + 2u * i] = (uint16_t)(uint8_t)hex_digits[(b >> 4) & 0x0Fu];
        out_desc_str[2u + 2u * i] = (uint16_t)(uint8_t)hex_digits[b & 0x0Fu];
    }

    size_t b_length = 2u + 2u * chr_count;
    out_desc_str[0] = (uint16_t)(((uint16_t)USB_DESC_STRING << 8) | b_length);
    return b_length;
}

int main(void) {
    /* 模拟从 0x1FFFF7E8 读出的 12 字节唯一 ID */
    const uint8_t sample_uid[12] = {
        0x57, 0xFF, 0x6E, 0x06, 0x49, 0x80,
        0x53, 0x56, 0x35, 0x43, 0x10, 0x87
    };
    uint16_t desc[25] = {0};

    size_t b_len = uid_to_string_desc(sample_uid, desc);
    printf("bLength=%u, bDescriptorType=0x%02X, serial=\"",
           (unsigned)(desc[0] & 0xFFu), (unsigned)(desc[0] >> 8));
    for (size_t i = 1u; i <= 24u; ++i) {
        putchar((int)(desc[i] & 0x7Fu));
    }
    printf("\" (returned %u)\n", (unsigned)b_len);
    return 0;
}
