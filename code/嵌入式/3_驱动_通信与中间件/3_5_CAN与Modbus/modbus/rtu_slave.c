#include "rtu_slave.h"
#include "crc16_modbus.h"

static uint16_t get_be16(const uint8_t *p) {
    return (uint16_t)(((uint16_t)p[0] << 8) | (uint16_t)p[1]);
}

static void put_be16(uint8_t *p, uint16_t val) {
    p[0] = (uint8_t)(val >> 8);
    p[1] = (uint8_t)(val & 0xFFu);
}

static size_t append_crc_le16(uint8_t *frame, size_t pdu_plus_addr_len) {
    uint16_t crc = crc16_modbus(frame, pdu_plus_addr_len);
    frame[pdu_plus_addr_len] = (uint8_t)(crc & 0xFFu);      /* CRC 低字节先发 */
    frame[pdu_plus_addr_len + 1u] = (uint8_t)(crc >> 8);    /* CRC 高字节后发 */
    return pdu_plus_addr_len + 2u;
}

static size_t build_exception(uint8_t addr,
                              uint8_t func,
                              uint8_t ex_code,
                              uint8_t *resp,
                              size_t resp_cap) {
    if (addr == 0u || resp_cap < 5u) {
        return 0u;
    }
    resp[0] = addr;
    resp[1] = (uint8_t)(func | 0x80u);
    resp[2] = ex_code;
    return append_crc_le16(resp, 3u);
}

size_t modbus_rtu_process(struct modbus_rtu_slave *slave,
                          const uint8_t *req,
                          size_t req_len,
                          uint8_t *resp,
                          size_t resp_cap) {
    /* RTU 最小帧长：地址(1) + 功能码(1) + CRC(2) = 4 字节 */
    if (req_len < 4u) {
        return 0u;
    }

    uint8_t addr = req[0];
    if (addr != slave->slave_addr && addr != 0u) {
        return 0u; /* 非本站地址且非广播，静默忽略 */
    }

    /* 校验 CRC（低字节在 req_len - 2，高字节在 req_len - 1） */
    uint16_t rx_crc = (uint16_t)req[req_len - 2u] |
                      ((uint16_t)req[req_len - 1u] << 8);
    uint16_t calc_crc = crc16_modbus(req, req_len - 2u);
    if (rx_crc != calc_crc) {
        return 0u; /* CRC 错误，静默丢弃，不得回传异常应答 */
    }

    uint8_t func = req[1];

    if (func == 0x03u) {
        /* 0x03 读保持寄存器：不支持广播，请求固定 8 字节 */
        if (addr == 0u || req_len != 8u) {
            return 0u;
        }
        uint16_t start_addr = get_be16(&req[2]);
        uint16_t qty = get_be16(&req[4]);

        if (qty < 1u || qty > 125u) {
            return build_exception(addr, func, MODBUS_EX_ILLEGAL_DATA_VALUE,
                                   resp, resp_cap);
        }
        if ((uint32_t)start_addr + (uint32_t)qty > MODBUS_HOLDING_REG_COUNT) {
            return build_exception(addr, func, MODBUS_EX_ILLEGAL_DATA_ADDR,
                                   resp, resp_cap);
        }

        size_t total_len = 3u + (size_t)qty * 2u + 2u;
        if (resp_cap < total_len) {
            return 0u;
        }

        resp[0] = addr;
        resp[1] = func;
        resp[2] = (uint8_t)(qty * 2u);
        for (uint16_t i = 0u; i < qty; ++i) {
            put_be16(&resp[3u + i * 2u], slave->holding_regs[start_addr + i]);
        }
        return append_crc_le16(resp, 3u + (size_t)qty * 2u);
    }

    if (func == 0x06u) {
        /* 0x06 写单个保持寄存器：请求固定 8 字节，正常应答原样回显请求帧 */
        if (req_len != 8u) {
            return 0u;
        }
        uint16_t reg_addr = get_be16(&req[2]);
        uint16_t reg_val = get_be16(&req[4]);

        if (reg_addr >= MODBUS_HOLDING_REG_COUNT) {
            return build_exception(addr, func, MODBUS_EX_ILLEGAL_DATA_ADDR,
                                   resp, resp_cap);
        }

        slave->holding_regs[reg_addr] = reg_val;

        if (addr == 0u || resp_cap < 8u) {
            return 0u; /* 广播写只执行动作，不回发应答 */
        }
        for (size_t i = 0u; i < 6u; ++i) {
            resp[i] = req[i];
        }
        return append_crc_le16(resp, 6u);
    }

    return build_exception(addr, func, MODBUS_EX_ILLEGAL_FUNCTION,
                           resp, resp_cap);
}
