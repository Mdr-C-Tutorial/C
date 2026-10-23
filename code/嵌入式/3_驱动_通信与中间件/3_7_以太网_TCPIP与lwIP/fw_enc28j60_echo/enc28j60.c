#include "enc28j60.h"
#include "stm32f1xx.h"

#define ENC_RX_BUF_START 0x0000u
#define ENC_RX_BUF_END   0x19FFu
#define ENC_TX_BUF_START 0x1A00u

static uint8_t g_enc_bank = 0xFFu;
static uint16_t g_next_pkt_ptr = ENC_RX_BUF_START;

static void spi2_cs_low(void) {
    GPIOB->BSRR = GPIO_BSRR_BR12;
}

static void spi2_cs_high(void) {
    while ((SPI2->SR & SPI_SR_BSY) != 0u) {
    }
    GPIOB->BSRR = GPIO_BSRR_BS12;
}

static uint8_t spi2_xfer(uint8_t tx) {
    while ((SPI2->SR & SPI_SR_TXE) == 0u) {
    }
    SPI2->DR = tx;
    while ((SPI2->SR & SPI_SR_RXNE) == 0u) {
    }
    return (uint8_t)SPI2->DR;
}

static uint8_t enc_read_op(uint8_t op, uint8_t addr) {
    spi2_cs_low();
    spi2_xfer((uint8_t)(op | (addr & ENC_ADDR_MASK)));
    uint8_t val = spi2_xfer(0xFFu);
    spi2_cs_high();
    return val;
}

static void enc_write_op(uint8_t op, uint8_t addr, uint8_t data) {
    spi2_cs_low();
    spi2_xfer((uint8_t)(op | (addr & ENC_ADDR_MASK)));
    spi2_xfer(data);
    spi2_cs_high();
}

static void enc_set_bank(uint8_t reg) {
    uint8_t addr = (uint8_t)(reg & ENC_ADDR_MASK);
    if (addr >= 0x1Bu) {
        return; /* 0x1B～0x1F 在所有 Bank 通用 */
    }
    uint8_t bank = (uint8_t)((reg & ENC_BANK_MASK) >> 5);
    if (bank != g_enc_bank) {
        enc_write_op(ENC_OP_BFC, ENC_ECON1, 0x03u);
        enc_write_op(ENC_OP_BFS, ENC_ECON1, bank);
        g_enc_bank = bank;
    }
}

static uint8_t enc_read_reg(uint8_t reg) {
    enc_set_bank(reg);
    return enc_read_op(ENC_OP_RCR, reg);
}

static void enc_write_reg(uint8_t reg, uint8_t val) {
    enc_set_bank(reg);
    enc_write_op(ENC_OP_WCR, reg, val);
}

void enc28j60_write_buf(const void *data, size_t len) {
    const uint8_t *src = (const uint8_t *)data;
    spi2_cs_low();
    spi2_xfer(ENC_OP_WBM);
    for (size_t i = 0u; i < len; ++i) {
        spi2_xfer(src[i]);
    }
    spi2_cs_high();
}

void enc28j60_read_buf(void *data, size_t len) {
    uint8_t *dst = (uint8_t *)data;
    spi2_cs_low();
    spi2_xfer(ENC_OP_RBM);
    for (size_t i = 0u; i < len; ++i) {
        dst[i] = spi2_xfer(0xFFu);
    }
    spi2_cs_high();
}

void enc28j60_init(const uint8_t mac[6]) {
    (void)mac;
    /* 1. 开启 GPIOB（APB2）与 SPI2（APB1，PCLK1 = 36 MHz）时钟 */
    RCC->APB2ENR |= RCC_APB2ENR_IOPBEN;
    RCC->APB1ENR |= RCC_APB1ENR_SPI2EN;

    /* 2. PB12（CS）先置高；配置 PB12 通用推挽，PB13/PB15 复用推挽，PB14 浮空输入 */
    GPIOB->BSRR = GPIO_BSRR_BS12;
    GPIOB->CRH = (GPIOB->CRH & ~(0xFFFFu << 16)) | (0xB4B3u << 16);

    /* 3. SPI2 模式 0（CPOL=0, CPHA=0），分频 PCLK1/4 = 9 MHz */
    SPI2->CR2 = 0u;
    SPI2->CR1 = SPI_CR1_SSM | SPI_CR1_SSI | SPI_CR1_BR_0 |
                SPI_CR1_MSTR | SPI_CR1_SPE;

    /* 4. 发送软件复位指令 SRC (0xFF)，初始化接收缓冲区环形边界并使能接收 */
    spi2_cs_low();
    spi2_xfer(ENC_OP_SRC);
    spi2_cs_high();

    g_next_pkt_ptr = ENC_RX_BUF_START;
    enc_write_reg(ENC_ERXSTL, (uint8_t)(ENC_RX_BUF_START & 0xFFu));
    enc_write_reg(ENC_ERXSTH, (uint8_t)(ENC_RX_BUF_START >> 8));
    enc_write_op(ENC_OP_BFS, ENC_ECON1, ENC_ECON1_RXEN);
}

uint8_t enc28j60_pending_packets(void) {
    return enc_read_reg(ENC_EPKTCNT);
}

void enc28j60_begin_tx(void) {
    enc_write_reg(ENC_EWRPTL, (uint8_t)(ENC_TX_BUF_START & 0xFFu));
    enc_write_reg(ENC_EWRPTH, (uint8_t)(ENC_TX_BUF_START >> 8));
    enc_write_reg(ENC_ETXSTL, (uint8_t)(ENC_TX_BUF_START & 0xFFu));
    enc_write_reg(ENC_ETXSTH, (uint8_t)(ENC_TX_BUF_START >> 8));
    /* 发送缓冲区首字节为包控制字节（0x00 表示使用 MACON3 默认配置） */
    uint8_t ctrl = 0x00u;
    enc28j60_write_buf(&ctrl, 1u);
}

void enc28j60_end_tx(uint16_t tot_len) {
    uint16_t end_addr = (uint16_t)(ENC_TX_BUF_START + tot_len);
    enc_write_reg(ENC_ETXNDL, (uint8_t)(end_addr & 0xFFu));
    enc_write_reg(ENC_ETXNDH, (uint8_t)(end_addr >> 8));
    enc_write_op(ENC_OP_BFS, ENC_ECON1, ENC_ECON1_TXRTS);
}

uint16_t enc28j60_begin_rx(void) {
    enc_write_reg(ENC_ERDPTL, (uint8_t)(g_next_pkt_ptr & 0xFFu));
    enc_write_reg(ENC_ERDPTH, (uint8_t)(g_next_pkt_ptr >> 8));

    /* 读取 6 字节接收状态向量：前 2 字节为下一包指针，第 3～4 字节为帧长（含 4B FCS） */
    uint8_t hdr[6];
    enc28j60_read_buf(hdr, sizeof(hdr));
    g_next_pkt_ptr = (uint16_t)(((uint16_t)hdr[1] << 8) | hdr[0]);
    uint16_t wire_len = (uint16_t)(((uint16_t)hdr[3] << 8) | hdr[2]);
    uint16_t status = (uint16_t)(((uint16_t)hdr[5] << 8) | hdr[4]);

    if ((status & 0x0080u) == 0u || wire_len < 4u) {
        return 0u;
    }
    return (uint16_t)(wire_len - 4u); /* 去掉末尾 4 字节硬件 CRC */
}

void enc28j60_end_rx(void) {
    uint16_t rd_ptr = (g_next_pkt_ptr == ENC_RX_BUF_START)
                          ? ENC_RX_BUF_END
                          : (uint16_t)(g_next_pkt_ptr - 1u);
    enc_write_reg(ENC_ERXRDPTL, (uint8_t)(rd_ptr & 0xFFu));
    enc_write_reg(ENC_ERXRDPTH, (uint8_t)(rd_ptr >> 8));
    enc_write_op(ENC_OP_BFS, ENC_ECON2, ENC_ECON2_PKTDEC);
}
