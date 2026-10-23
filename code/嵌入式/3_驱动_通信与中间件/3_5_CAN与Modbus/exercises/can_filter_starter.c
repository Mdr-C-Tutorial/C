#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

/*
 * 练习 63338 初始代码：
 * 在 32 位掩码模式下，bxCAN 将接收帧的标识符寄存器 RIR（不含最低位 TXRQ）
 * 与过滤器寄存器 FR1、掩码寄存器 FR2 按下式匹配：
 *   (rir & fr2) == (fr1 & fr2)
 */
static uint32_t pack_std_data_rir(uint16_t std_id) {
    /* 标准数据帧：STID[10:0] 位于第 31:21 位，IDE(位 2) = 0，RTR(位 1) = 0 */
    return ((uint32_t)(std_id & 0x7FFu) << 21);
}

static bool bxcan_filter_match_32bit_mask(uint32_t rir, uint32_t fr1, uint32_t fr2) {
    return (rir & fr2) == (fr1 & fr2);
}

int main(void) {
    /* TODO: 修改 fr1 与 fr2，使过滤器仅接收标准数据帧且 ID 在 0x100 ~ 0x10F 范围内 */
    uint32_t fr1 = 0x00000000u;
    uint32_t fr2 = 0x00000000u;

    const uint16_t test_ids[] = { 0x0FFu, 0x100u, 0x105u, 0x10Fu, 0x110u };
    for (size_t i = 0u; i < sizeof test_ids / sizeof test_ids[0]; ++i) {
        uint32_t rir = pack_std_data_rir(test_ids[i]);
        bool pass = bxcan_filter_match_32bit_mask(rir, fr1, fr2);
        printf("ID 0x%03X -> %s\n", test_ids[i], pass ? "ACCEPT" : "REJECT");
    }
    return 0;
}
