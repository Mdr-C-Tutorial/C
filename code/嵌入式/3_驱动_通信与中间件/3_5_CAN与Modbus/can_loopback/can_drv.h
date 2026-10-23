#ifndef CAN_DRV_H
#define CAN_DRV_H

#include <stdbool.h>
#include <stdint.h>

struct can_frame {
    uint32_t id;     /* 11 位标准 ID 或 29 位扩展 ID */
    uint8_t is_ext;  /* 0 = 标准帧（11 位），1 = 扩展帧（29 位） */
    uint8_t is_rtr;  /* 0 = 数据帧，1 = 远程帧 */
    uint8_t dlc;     /* 数据长度码 0 ~ 8 */
    uint8_t data[8]; /* 载荷字节 */
};

/*
 * 初始化 CAN1：
 * - 重映射到 PB8 (CAN_RX) / PB9 (CAN_TX)
 * - 波特率 500 kbit/s（PCLK1 = 36 MHz，分频 9，BS1 = 6 tq，BS2 = 1 tq，采样点 87.5%）
 * - 过滤器组 0 设为 32 位掩码全接收，关联到 FIFO 0
 * - loopback_silent 为 true 时同时置位 LBKM 与 SILM，用于单板回环自测
 */
bool can1_init_500k(bool loopback_silent);

/* 将一帧写入空闲发送邮箱并请求发送；若 3 个邮箱全忙则返回 false */
bool can1_send(const struct can_frame *frame);

/* 从接收 FIFO 0 非阻塞读取一帧；FIFO 0 为空时返回 false */
bool can1_receive_fifo0(struct can_frame *frame);

#endif
