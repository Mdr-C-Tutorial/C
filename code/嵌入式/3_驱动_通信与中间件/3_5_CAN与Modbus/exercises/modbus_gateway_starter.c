#include "../modbus/crc16_modbus.h"
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

/*
 * 练习 63342 初始代码：
 * Modbus TCP <-> Modbus RTU 协议转换网关
 *
 * MBAP 头（7 字节，大端序）：
 *   [0..1] Transaction ID（事务标识符）
 *   [2..3] Protocol ID（协议标识符，Modbus 固定为 0x0000）
 *   [4..5] Length（后续字节数 = Unit ID 1 字节 + PDU 长度）
 *   [6]    Unit ID（对应 RTU 从站地址）
 */
size_t modbus_tcp_to_rtu(const uint8_t *tcp_adu,
                         size_t tcp_len,
                         uint8_t *rtu_adu,
                         size_t rtu_cap,
                         uint16_t *out_tid) {
    /* TODO: 校验 MBAP 头，提取 Transaction ID，构造带 CRC-16/MODBUS 的 RTU 帧 */
    (void)tcp_adu;
    (void)tcp_len;
    (void)rtu_adu;
    (void)rtu_cap;
    (void)out_tid;
    return 0u;
}

size_t modbus_rtu_to_tcp(const uint8_t *rtu_adu,
                         size_t rtu_len,
                         uint16_t tid,
                         uint8_t *tcp_adu,
                         size_t tcp_cap) {
    /* TODO: 校验 RTU CRC，剥离尾部 2 字节 CRC，封装 7 字节 MBAP 头 */
    (void)rtu_adu;
    (void)rtu_len;
    (void)tid;
    (void)tcp_adu;
    (void)tcp_cap;
    return 0u;
}

int main(void) {
    const uint8_t tcp_req[] = {
        0x00u, 0x2Au, /* Transaction ID = 0x002A */
        0x00u, 0x00u, /* Protocol ID = 0 */
        0x00u, 0x06u, /* Length = 6 */
        0x01u,        /* Unit ID = 1 */
        0x03u, 0x00u, 0x00u, 0x00u, 0x02u /* PDU */
    };
    uint8_t rtu_buf[64];
    uint16_t tid = 0u;
    size_t rtu_len = modbus_tcp_to_rtu(tcp_req, sizeof tcp_req,
                                       rtu_buf, sizeof rtu_buf, &tid);
    printf("TID = 0x%04X, RTU len = %u\n", tid, (unsigned)rtu_len);
    return 0;
}
