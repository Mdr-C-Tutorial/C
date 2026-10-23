#include "../modbus/crc16_modbus.h"
#include "../modbus/rtu_slave.h"
#include <stdio.h>
#include <string.h>

/*
 * 练习 63339 初始代码：
 * 在 rtu_slave.c 中补充功能码 0x10（写多个保持寄存器）后，运行本测试验证。
 *
 * 0x10 请求帧格式：
 *   从站地址(1B) + 0x10(1B) + 起始地址(2B, BE) + 寄存器数量 N(2B, BE)
 *   + 字节数 2N(1B) + 寄存器值(2N B, BE) + CRC(2B, LE)
 * 0x10 正常应答帧格式（8 字节）：
 *   从站地址(1B) + 0x10(1B) + 起始地址(2B, BE) + 寄存器数量 N(2B, BE) + CRC(2B, LE)
 */
static void send_and_print(struct modbus_rtu_slave *slave,
                           const uint8_t *pdu_with_addr,
                           size_t len_no_crc) {
    uint8_t req[64];
    uint8_t resp[64];

    memcpy(req, pdu_with_addr, len_no_crc);
    uint16_t crc = crc16_modbus(req, len_no_crc);
    req[len_no_crc] = (uint8_t)(crc & 0xFFu);
    req[len_no_crc + 1u] = (uint8_t)(crc >> 8);

    size_t resp_len = modbus_rtu_process(slave, req, len_no_crc + 2u,
                                         resp, sizeof resp);
    printf("Resp (%u B):", (unsigned)resp_len);
    for (size_t i = 0u; i < resp_len; ++i) {
        printf(" %02X", resp[i]);
    }
    printf("\n");
}

int main(void) {
    struct modbus_rtu_slave slave = { .slave_addr = 0x01u };

    /* 向地址 0x0002 起连续写入 2 个寄存器：0x000A 和 0x0102 */
    const uint8_t write_multi[] = {
        0x01u, 0x10u, 0x00u, 0x02u, 0x00u, 0x02u, 0x04u,
        0x00u, 0x0Au, 0x01u, 0x02u
    };
    send_and_print(&slave, write_multi, sizeof write_multi);
    printf("regs[2]=0x%04X, regs[3]=0x%04X\n",
           slave.holding_regs[2], slave.holding_regs[3]);
    return 0;
}
