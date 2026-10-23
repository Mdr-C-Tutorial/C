#include "crc16_modbus.h"
#include "rtu_slave.h"
#include <stdio.h>
#include <string.h>

static void print_hex(const char *label, const uint8_t *buf, size_t len) {
    printf("%-16s (%2u B):", label, (unsigned)len);
    if (len == 0u) {
        printf(" <no response>\n");
        return;
    }
    for (size_t i = 0u; i < len; ++i) {
        printf(" %02X", buf[i]);
    }
    printf("\n");
}

static void run_case(const char *title,
                     struct modbus_rtu_slave *slave,
                     const uint8_t *req_pdu_with_addr,
                     size_t len_without_crc) {
    uint8_t req[64];
    uint8_t resp[64];

    memcpy(req, req_pdu_with_addr, len_without_crc);
    uint16_t crc = crc16_modbus(req, len_without_crc);
    req[len_without_crc] = (uint8_t)(crc & 0xFFu);
    req[len_without_crc + 1u] = (uint8_t)(crc >> 8);
    size_t req_len = len_without_crc + 2u;

    printf("--- %s ---\n", title);
    print_hex("Request", req, req_len);
    size_t resp_len = modbus_rtu_process(slave, req, req_len, resp, sizeof resp);
    print_hex("Response", resp, resp_len);
}

int main(void) {
    const char *ascii = "123456789";
    uint16_t crc_ascii = crc16_modbus((const uint8_t *)ascii, 9u);
    const uint8_t spec_pdu[] = { 0x11u, 0x03u, 0x00u, 0x6Bu, 0x00u, 0x03u };
    uint16_t crc_spec = crc16_modbus(spec_pdu, sizeof spec_pdu);

    printf("CRC-16/MODBUS(\"123456789\") = 0x%04X\n", crc_ascii);
    printf("Spec example 11 03 00 6B 00 03 -> CRC = 0x%04X (wire: %02X %02X)\n\n",
           crc_spec, (uint8_t)(crc_spec & 0xFFu), (uint8_t)(crc_spec >> 8));

    struct modbus_rtu_slave slave = {
        .slave_addr = 0x01u,
        .holding_regs = { 100u, 200u, 300u, 400u },
    };

    /* 1. 读保持寄存器 0 ~ 1（初值 100 = 0x0064，200 = 0x00C8） */
    const uint8_t req_read2[] = { 0x01u, 0x03u, 0x00u, 0x00u, 0x00u, 0x02u };
    run_case("1. Read holding regs [0..1]", &slave, req_read2, sizeof req_read2);

    /* 2. 写单个寄存器 1 为 3（0x0003） */
    const uint8_t req_write1[] = { 0x01u, 0x06u, 0x00u, 0x01u, 0x00u, 0x03u };
    run_case("2. Write holding reg [1] = 3", &slave, req_write1, sizeof req_write1);

    /* 3. 再次读取保持寄存器 0 ~ 1，验证寄存器 1 已更新为 3 */
    run_case("3. Read holding regs [0..1] after write", &slave, req_read2, sizeof req_read2);

    /* 4. 读取越界地址 0x0020，触发异常码 0x02（非法数据地址） */
    const uint8_t req_bad_addr[] = { 0x01u, 0x03u, 0x00u, 0x20u, 0x00u, 0x02u };
    run_case("4. Read out-of-range address 0x0020", &slave, req_bad_addr, sizeof req_bad_addr);

    /* 5. 广播写寄存器 2 为 10（0x000A），从站执行写入但不回发应答 */
    const uint8_t req_bcast[] = { 0x00u, 0x06u, 0x00u, 0x02u, 0x00u, 0x0Au };
    run_case("5. Broadcast write reg [2] = 10", &slave, req_bcast, sizeof req_bcast);
    printf("holding_regs[2] after broadcast = %u\n", slave.holding_regs[2]);

    return 0;
}
