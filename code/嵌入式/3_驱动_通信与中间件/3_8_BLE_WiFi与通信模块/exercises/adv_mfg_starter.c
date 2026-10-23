/*
 * 习题 60383 初始代码：解码含厂商自定义数据（0xFF）与多个 16 位 UUID 的 BLE 广播。
 *
 * 编译：
 *   gcc -std=c17 -Wall -Wextra exercises/adv_mfg_starter.c -o adv_mfg_starter
 */
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

static void decode_adv(const uint8_t *adv, size_t total_len) {
    size_t pos = 0u;
    while (pos < total_len) {
        uint8_t len = adv[pos];
        if (len == 0u || pos + 1u + (size_t)len > total_len) {
            break;
        }
        uint8_t type = adv[pos + 1u];
        const uint8_t *data = &adv[pos + 2u];
        size_t data_len = (size_t)len - 1u;

        if (type == 0x01u && data_len >= 1u) {
            printf("Flags: 0x%02X\n", data[0]);
        } else if (type == 0x08u || type == 0x09u) {
            printf("Name: %.*s\n", (int)data_len, (const char *)data);
        } else if (type == 0x03u) {
            printf("UUID16:");
            for (size_t i = 0u; i + 1u < data_len; i += 2u) {
                uint16_t u =
                    (uint16_t)data[i] | ((uint16_t)data[i + 1u] << 8);
                printf(" 0x%04X", u);
            }
            putchar('\n');
        } else if (type == 0xFFu && data_len >= 2u) {
            uint16_t company_id =
                (uint16_t)data[0] | ((uint16_t)data[1] << 8);
            printf("Manufacturer: company=0x%04X payload=", company_id);
            for (size_t i = 2u; i < data_len; ++i) {
                printf("%02X", data[i]);
            }
            putchar('\n');
        }
        pos += 1u + (size_t)len;
    }
}

int main(void) {
    const uint8_t sample_adv[] = {
        0x02, 0x01, 0x06,
        0x07, 0x09, 'M', 'd', 'r', 'B', 'L', 'E',
        0x05, 0x03, 0x09, 0x18, 0x0F, 0x18,
        0x06, 0xFF, 0xE5, 0x02, 0x01, 0x19, 0x64
    };
    decode_adv(sample_adv, sizeof(sample_adv));
    return 0;
}
