#include "can_drv.h"
#include "stm32f1xx.h"

#define CAN_MODE_TIMEOUT 100000u

static bool can1_wait_msr_bit(uint32_t mask, uint32_t expected) {
    for (uint32_t i = 0u; i < CAN_MODE_TIMEOUT; ++i) {
        if ((CAN1->MSR & mask) == expected) {
            return true;
        }
    }
    return false;
}

bool can1_init_500k(bool loopback_silent) {
    /* 1. 开启 AFIO、GPIOB 与 CAN1 时钟 */
    RCC->APB2ENR |= RCC_APB2ENR_AFIOEN | RCC_APB2ENR_IOPBEN;
    RCC->APB1ENR |= RCC_APB1ENR_CAN1EN;

    /* 2. 将 CAN1 重映射到 PB8（RX）/ PB9（TX）：CAN_REMAP[1:0] = 10 */
    AFIO->MAPR = (AFIO->MAPR & ~AFIO_MAPR_CAN_REMAP) |
                 AFIO_MAPR_CAN_REMAP_REMAP2;

    /* PB8（CRH[3:0]）：上拉输入（CNF = 10, MODE = 00 -> 0x8，ODR8 = 1）
     * PB9（CRH[7:4]）：50 MHz 复用推挽输出（CNF = 10, MODE = 11 -> 0xB）
     */
    GPIOB->CRH = (GPIOB->CRH & ~0xFFu) | 0xB8u;
    GPIOB->BSRR = (1u << 8);

    /* 3. 退出睡眠模式并请求进入初始化模式，等待硬件置位 INAK */
    CAN1->MCR &= ~CAN_MCR_SLEEP;
    CAN1->MCR |= CAN_MCR_INRQ;
    if (!can1_wait_msr_bit(CAN_MSR_INAK, CAN_MSR_INAK)) {
        return false;
    }

    /* 开启自动离线恢复（ABOM），发送优先级按请求先后顺序（TXFP） */
    CAN1->MCR &= ~(CAN_MCR_TTCM | CAN_MCR_NART | CAN_MCR_RFLM);
    CAN1->MCR |= CAN_MCR_ABOM | CAN_MCR_TXFP;

    /* 4. 配置位时序：PCLK1 = 36 MHz，500 kbit/s
     *    分频 9（BRP = 8），BS1 = 6 tq（TS1 = 5），BS2 = 1 tq（TS2 = 0），SJW = 1 tq
     */
    uint32_t btr = (0u << CAN_BTR_SJW_Pos) |
                   (0u << CAN_BTR_TS2_Pos) |
                   (5u << CAN_BTR_TS1_Pos) |
                   (8u << CAN_BTR_BRP_Pos);
    if (loopback_silent) {
        btr |= CAN_BTR_LBKM | CAN_BTR_SILM;
    }
    CAN1->BTR = btr;

    /* 5. 配置过滤器组 0：32 位掩码模式，掩码全 0（接收全部报文），关联 FIFO 0 */
    CAN1->FMR |= CAN_FMR_FINIT;
    CAN1->FA1R &= ~(1u << 0);        /* 修改前先停用过滤器组 0 */
    CAN1->FS1R |= (1u << 0);         /* 1 = 单个 32 位位宽 */
    CAN1->FM1R &= ~(1u << 0);        /* 0 = 标识符掩码模式 */
    CAN1->FFA1R &= ~(1u << 0);       /* 0 = 关联到 FIFO 0 */
    CAN1->sFilterRegister[0].FR1 = 0x00000000u; /* ID = 0 */
    CAN1->sFilterRegister[0].FR2 = 0x00000000u; /* Mask = 0（不关心任何位） */
    CAN1->FA1R |= (1u << 0);         /* 激活过滤器组 0 */
    CAN1->FMR &= ~CAN_FMR_FINIT;

    /* 6. 退出初始化模式，进入正常工作（或测试）模式 */
    CAN1->MCR &= ~CAN_MCR_INRQ;
    return can1_wait_msr_bit(CAN_MSR_INAK, 0u);
}

bool can1_send(const struct can_frame *frame) {
    uint32_t tsr = CAN1->TSR;
    uint32_t mbox = 3u;

    if ((tsr & CAN_TSR_TME0) != 0u) {
        mbox = 0u;
    } else if ((tsr & CAN_TSR_TME1) != 0u) {
        mbox = 1u;
    } else if ((tsr & CAN_TSR_TME2) != 0u) {
        mbox = 2u;
    } else {
        return false;
    }

    uint8_t dlc = (frame->dlc <= 8u) ? frame->dlc : 8u;
    CAN1->sTxMailBox[mbox].TDTR = (uint32_t)dlc;

    uint32_t low = 0u;
    uint32_t high = 0u;
    for (uint8_t i = 0u; i < dlc; ++i) {
        if (i < 4u) {
            low |= (uint32_t)frame->data[i] << (8u * i);
        } else {
            high |= (uint32_t)frame->data[i] << (8u * (i - 4u));
        }
    }
    CAN1->sTxMailBox[mbox].TDLR = low;
    CAN1->sTxMailBox[mbox].TDHR = high;

    /* 最后写入 TIR 并置位 TXRQ 启动发送 */
    uint32_t tir = (frame->is_rtr != 0u) ? CAN_TI0R_RTR : 0u;
    if (frame->is_ext != 0u) {
        tir |= ((frame->id & 0x1FFFFFFFu) << CAN_TI0R_EXID_Pos) | CAN_TI0R_IDE;
    } else {
        tir |= ((frame->id & 0x7FFu) << CAN_TI0R_STID_Pos);
    }
    CAN1->sTxMailBox[mbox].TIR = tir | CAN_TI0R_TXRQ;
    return true;
}

bool can1_receive_fifo0(struct can_frame *frame) {
    if ((CAN1->RF0R & CAN_RF0R_FMP0) == 0u) {
        return false;
    }

    uint32_t rir = CAN1->sFIFOMailBox[0].RIR;
    uint32_t rdtr = CAN1->sFIFOMailBox[0].RDTR;
    uint32_t rdlr = CAN1->sFIFOMailBox[0].RDLR;
    uint32_t rdhr = CAN1->sFIFOMailBox[0].RDHR;

    frame->is_ext = ((rir & CAN_RI0R_IDE) != 0u) ? 1u : 0u;
    frame->is_rtr = ((rir & CAN_RI0R_RTR) != 0u) ? 1u : 0u;
    if (frame->is_ext != 0u) {
        frame->id = (rir >> CAN_RI0R_EXID_Pos) & 0x1FFFFFFFu;
    } else {
        frame->id = (rir >> CAN_RI0R_STID_Pos) & 0x7FFu;
    }

    frame->dlc = (uint8_t)(rdtr & CAN_RDT0R_DLC);
    if (frame->dlc > 8u) {
        frame->dlc = 8u;
    }
    for (uint8_t i = 0u; i < 8u; ++i) {
        if (i < 4u) {
            frame->data[i] = (uint8_t)((rdlr >> (8u * i)) & 0xFFu);
        } else {
            frame->data[i] = (uint8_t)((rdhr >> (8u * (i - 4u))) & 0xFFu);
        }
    }

    /* 置位 RFOM0 释放 FIFO 0 当前邮箱，硬件将 FMP0 减 1 */
    CAN1->RF0R |= CAN_RF0R_RFOM0;
    return true;
}
