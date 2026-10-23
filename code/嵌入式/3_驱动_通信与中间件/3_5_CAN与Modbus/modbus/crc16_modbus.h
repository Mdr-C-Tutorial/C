#ifndef CRC16_MODBUS_H
#define CRC16_MODBUS_H

#include <stddef.h>
#include <stdint.h>

/*
 * 计算 CRC-16/MODBUS：
 * - 初值 0xFFFF
 * - 反射多项式 0xA001（正向多项式 0x8005 按位反转）
 * - 在线上传输时，返回值的低字节先发、高字节后发
 */
uint16_t crc16_modbus(const uint8_t *data, size_t len);

#endif
