#define _POSIX_C_SOURCE 200809L

#include "sample_io.h"

#include <string.h>

#define SAMPLE_BYTES 24

void sample_reader_init(struct sample_reader *sr) { sr->len = 0; }

size_t sample_reader_pending(const struct sample_reader *sr) {
    return sr->len;
}

size_t sample_reader_feed(struct sample_reader *sr, const unsigned char *data,
                          size_t n, struct imu_sample *out, size_t cap) {
    size_t produced = 0;
    size_t off = 0;

    while (off < n) {
        size_t room = sizeof sr->buf - sr->len;
        size_t take = n - off < room ? n - off : room;

        if (take == 0) {
            /* 缓冲被半个样本占满又放不下新数据：调用方给的空间太小 */
            return produced;
        }
        memcpy(sr->buf + sr->len, data + off, take);
        sr->len += take;
        off += take;

        while (sr->len >= SAMPLE_BYTES) {
            struct imu_sample s;
            size_t consumed;

            if (produced == cap) {
                /* 调用方给的空间用完了，剩下的整样本留在缓冲里 */
                return produced;
            }
            s = sample_from_le_bytes(sr->buf);
            consumed = SAMPLE_BYTES;
            sr->len -= consumed;
            memmove(sr->buf, sr->buf + consumed, sr->len);
            out[produced++] = s;
        }
    }
    return produced;
}

/* 让样本的字节布局在发送端与接收端一致：显式按小端写 */
void sample_to_le_bytes(const struct imu_sample *s, unsigned char out[24]) {
    uint64_t ts = s->timestamp_ns;

    for (int i = 0; i < 8; i++) {
        out[i] = (unsigned char)((ts >> (8 * i)) & 0xFFu);
    }
    for (int i = 0; i < 3; i++) {
        uint16_t v = (uint16_t)s->accel[i];

        out[8 + 2 * i] = (unsigned char)(v & 0xFFu);
        out[9 + 2 * i] = (unsigned char)(v >> 8);
    }
    for (int i = 0; i < 3; i++) {
        uint16_t v = (uint16_t)s->gyro[i];

        out[14 + 2 * i] = (unsigned char)(v & 0xFFu);
        out[15 + 2 * i] = (unsigned char)(v >> 8);
    }
    out[20] = (unsigned char)(s->accel_range & 0xFFu);
    out[21] = (unsigned char)(s->accel_range >> 8);
    out[22] = 0;
    out[23] = 0;
}

struct imu_sample sample_from_le_bytes(const unsigned char in[24]) {
    struct imu_sample s;
    uint64_t ts = 0;

    for (int i = 0; i < 8; i++) {
        ts |= (uint64_t)in[i] << (8 * i);
    }
    s.timestamp_ns = ts;
    for (int i = 0; i < 3; i++) {
        s.accel[i] = (int16_t)((uint16_t)in[8 + 2 * i] |
                               ((uint16_t)in[9 + 2 * i] << 8));
    }
    for (int i = 0; i < 3; i++) {
        s.gyro[i] = (int16_t)((uint16_t)in[14 + 2 * i] |
                              ((uint16_t)in[15 + 2 * i] << 8));
    }
    s.accel_range = (uint16_t)((uint16_t)in[20] |
                               ((uint16_t)in[21] << 8));
    s.reserved = 0;
    return s;
}