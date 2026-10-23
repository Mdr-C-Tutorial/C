#include "crc16.h"

uint16_t crc16_ccitt_update(uint16_t crc, const uint8_t *data, size_t len) {
    for (size_t i = 0u; i < len; ++i) {
        crc ^= (uint16_t)data[i] << 8;
        for (int bit = 0; bit < 8; ++bit) {
            if ((crc & 0x8000u) != 0u) {
                crc = (uint16_t)((crc << 1) ^ 0x1021u);
            } else {
                crc = (uint16_t)(crc << 1);
            }
        }
    }
    return crc;
}

uint16_t crc16_ccitt(const uint8_t *data, size_t len) {
    return crc16_ccitt_update(CRC16_CCITT_INIT, data, len);
}
