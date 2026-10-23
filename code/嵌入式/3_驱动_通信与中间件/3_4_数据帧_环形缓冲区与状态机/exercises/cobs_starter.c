#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

/*
 * TODO：实现 COBS 编码函数 cobs_encode 与解码函数 cobs_decode。
 * 规则（假设输入长度 len <= 254）：
 * - 编码后的输出不含任何 0x00 字节；
 * - 每个块的第 1 个字节 code 表示距离下一个 0x00（或帧末尾隐含的 0x00）的字节数（含 code 自身）；
 * - 解码遇到 0x00 或非法长度时返回 0 表示失败，成功时返回解码出的原始字节数。
 */
size_t cobs_encode(const uint8_t *in, size_t len, uint8_t *out) {
    (void)in;
    (void)len;
    (void)out;
    return 0u;
}

size_t cobs_decode(const uint8_t *in, size_t len, uint8_t *out) {
    (void)in;
    (void)len;
    (void)out;
    return 0u;
}

int main(void) {
    const uint8_t raw[] = { 0x11u, 0x22u, 0x00u, 0x33u, 0x00u, 0x00u, 0x44u };
    uint8_t enc[16] = {0};
    uint8_t dec[16] = {0};

    size_t enc_len = cobs_encode(raw, sizeof(raw), enc);
    size_t dec_len = cobs_decode(enc, enc_len, dec);

    printf("enc (%u B):", (unsigned)enc_len);
    for (size_t i = 0u; i < enc_len; ++i) {
        printf(" %02X", (unsigned)enc[i]);
    }
    printf("\ndec (%u B):", (unsigned)dec_len);
    for (size_t i = 0u; i < dec_len; ++i) {
        printf(" %02X", (unsigned)dec[i]);
    }
    printf("\n");
    return 0;
}
