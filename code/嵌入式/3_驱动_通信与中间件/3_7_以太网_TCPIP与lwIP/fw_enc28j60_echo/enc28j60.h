#ifndef ENC28J60_H
#define ENC28J60_H

#include <stddef.h>
#include <stdint.h>

/* ENC28J60 SPI 指令集（高 3 位为操作码，低 5 位为地址或固定常量 0x1A/0x1F） */
#define ENC_OP_RCR  0x00u /* 000 aaaaa：读控制寄存器 */
#define ENC_OP_RBM  0x3Au /* 001 11010：读缓冲区存储器 */
#define ENC_OP_WCR  0x40u /* 010 aaaaa：写控制寄存器 */
#define ENC_OP_WBM  0x7Au /* 011 11010：写缓冲区存储器 */
#define ENC_OP_BFS  0x80u /* 100 aaaaa：位域置 1（仅 ETH 寄存器） */
#define ENC_OP_BFC  0xA0u /* 101 aaaaa：位域清 0（仅 ETH 寄存器） */
#define ENC_OP_SRC  0xFFu /* 111 11111：系统软件复位 */

/* 常用寄存器编码：低 5 位为地址，[6:5] 位为所属 Bank */
#define ENC_ADDR_MASK   0x1Fu
#define ENC_BANK_MASK   0x60u
#define ENC_EIR         0x1Cu
#define ENC_ESTAT       0x1Du
#define ENC_ECON2       0x1Eu
#define ENC_ECON1       0x1Fu
#define ENC_ERDPTL      (0x00u | 0x00u)
#define ENC_ERDPTH      (0x00u | 0x01u)
#define ENC_EWRPTL      (0x00u | 0x02u)
#define ENC_EWRPTH      (0x00u | 0x03u)
#define ENC_ETXSTL      (0x00u | 0x04u)
#define ENC_ETXSTH      (0x00u | 0x05u)
#define ENC_ETXNDL      (0x00u | 0x06u)
#define ENC_ETXNDH      (0x00u | 0x07u)
#define ENC_ERXSTL      (0x00u | 0x08u)
#define ENC_ERXSTH      (0x00u | 0x09u)
#define ENC_ERXRDPTL    (0x00u | 0x0Cu)
#define ENC_ERXRDPTH    (0x00u | 0x0Du)
#define ENC_EPKTCNT     (0x20u | 0x19u) /* Bank 1：未处理接收帧计数 */

#define ENC_ECON1_RXEN  0x04u
#define ENC_ECON1_TXRTS 0x08u
#define ENC_ECON2_PKTDEC 0x40u

void enc28j60_init(const uint8_t mac[6]);
uint8_t enc28j60_pending_packets(void);

void enc28j60_begin_tx(void);
void enc28j60_write_buf(const void *data, size_t len);
void enc28j60_end_tx(uint16_t tot_len);

uint16_t enc28j60_begin_rx(void);
void enc28j60_read_buf(void *data, size_t len);
void enc28j60_end_rx(void);

#endif /* ENC28J60_H */
