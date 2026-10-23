#ifndef RTU_SLAVE_H
#define RTU_SLAVE_H

#include <stddef.h>
#include <stdint.h>

#define MODBUS_HOLDING_REG_COUNT 16u

/* Modbus 异常码 */
#define MODBUS_EX_ILLEGAL_FUNCTION   0x01u
#define MODBUS_EX_ILLEGAL_DATA_ADDR  0x02u
#define MODBUS_EX_ILLEGAL_DATA_VALUE 0x03u

struct modbus_rtu_slave {
    uint8_t slave_addr;
    uint16_t holding_regs[MODBUS_HOLDING_REG_COUNT];
};

/*
 * 处理一帧完整的 Modbus RTU 请求：
 * - 返回应答帧字节数（写入 resp）；
 * - 若地址不匹配、CRC 错误或为广播帧（地址 0），不发送应答，返回 0。
 */
size_t modbus_rtu_process(struct modbus_rtu_slave *slave,
                          const uint8_t *req,
                          size_t req_len,
                          uint8_t *resp,
                          size_t resp_cap);

#endif
