#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

static void decode_ad_structures(const uint8_t *adv, size_t total_len) {
    size_t pos = 0u;
    unsigned index = 0u;

    while (pos < total_len) {
        uint8_t len = adv[pos];
        if (len == 0u) {
            break;
        }
        if (pos + 1u + (size_t)len > total_len) {
            printf("AD[%u] truncated at offset %u\n", index, (unsigned)pos);
            break;
        }

        uint8_t type = adv[pos + 1u];
        const uint8_t *data = &adv[pos + 2u];
        size_t data_len = (size_t)len - 1u;

        printf("AD[%u] offset=%2u len=%2u type=0x%02X: ",
               index, (unsigned)pos, (unsigned)len, (unsigned)type);

        if (type == 0x01u && data_len >= 1u) {
            uint8_t flags = data[0];
            printf("Flags = 0x%02X (LE General Discoverable=%u, "
                   "BR/EDR Not Supported=%u)\n",
                   flags,
                   (unsigned)((flags >> 1) & 1u),
                   (unsigned)((flags >> 2) & 1u));
        } else if (type == 0x08u || type == 0x09u) {
            printf("%s Local Name = \"",
                   (type == 0x09u) ? "Complete" : "Shortened");
            for (size_t i = 0u; i < data_len; ++i) {
                putchar((int)data[i]);
            }
            printf("\"\n");
        } else if (type == 0x02u || type == 0x03u) {
            printf("16-bit Service UUIDs =");
            for (size_t i = 0u; i + 1u < data_len; i += 2u) {
                uint16_t uuid =
                    (uint16_t)data[i] | ((uint16_t)data[i + 1u] << 8);
                printf(" 0x%04X", uuid);
            }
            putchar('\n');
        } else {
            printf("Raw (%u bytes)\n", (unsigned)data_len);
        }

        pos += 1u + (size_t)len;
        ++index;
    }
}

int main(void) {
    /* ESP-AT BLE 广播示例：0201060A09457370726573736966030302A0 */
    const uint8_t adv_pdu[] = {
        0x02, 0x01, 0x06,
        0x0A, 0x09, 0x45, 0x73, 0x70, 0x72, 0x65, 0x73, 0x73, 0x69, 0x66,
        0x03, 0x03, 0x02, 0xA0
    };

    decode_ad_structures(adv_pdu, sizeof(adv_pdu));
    return 0;
}
