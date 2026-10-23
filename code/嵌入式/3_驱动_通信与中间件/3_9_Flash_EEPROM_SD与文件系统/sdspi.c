#include "sdspi.h"
#include "stm32f1xx.h"

#ifndef SPI_CR1_BR
#define SPI_CR1_BR   (0x7UL << 3)
#define SPI_CR1_BR_0 (0x1UL << 3)
#endif
#ifndef SPI_SR_BSY
#define SPI_SR_BSY (1UL << 7)
#endif
#ifndef GPIO_BSRR_BS1
#define GPIO_BSRR_BS1 (1UL << 1)
#define GPIO_BSRR_BR1 (1UL << 17)
#define GPIO_BSRR_BS4 (1UL << 4)
#endif

extern uint8_t spi1_xfer(uint8_t tx);

static sd_card_type_t g_card_type = SD_TYPE_UNKNOWN;

static void sd_cs_low(void) {
    GPIOB->BSRR = GPIO_BSRR_BR1;
}

static void sd_cs_high(void) {
    while ((SPI1->SR & SPI_SR_BSY) != 0u) {
    }
    GPIOB->BSRR = GPIO_BSRR_BS1;
    spi1_xfer(0xFFu); /* 释放 CS 后补 8 个时钟让卡释放 MISO */
}

static void spi1_set_prescaler(uint32_t br_bits) {
    while ((SPI1->SR & SPI_SR_BSY) != 0u) {
    }
    SPI1->CR1 = (SPI1->CR1 & ~SPI_CR1_BR) | br_bits;
}

static bool sd_wait_ready(uint32_t max_tries) {
    for (uint32_t i = 0; i < max_tries; ++i) {
        if (spi1_xfer(0xFFu) == 0xFFu) {
            return true;
        }
    }
    return false;
}

static uint8_t sd_send_cmd(uint8_t cmd, uint32_t arg, uint8_t crc) {
    if (cmd != 0u && !sd_wait_ready(5000u)) {
        return 0xFFu;
    }
    spi1_xfer((uint8_t)(0x40u | cmd));
    spi1_xfer((uint8_t)((arg >> 24) & 0xFFu));
    spi1_xfer((uint8_t)((arg >> 16) & 0xFFu));
    spi1_xfer((uint8_t)((arg >> 8) & 0xFFu));
    spi1_xfer((uint8_t)(arg & 0xFFu));
    spi1_xfer(crc);

    /* 等待 R1 响应（最高位为 0），最多等 10 字节 */
    for (uint32_t i = 0; i < 10u; ++i) {
        uint8_t r1 = spi1_xfer(0xFFu);
        if ((r1 & 0x80u) == 0u) {
            return r1;
        }
    }
    return 0xFFu;
}

bool sd_spi_init(sd_card_type_t *out_type) {
    /* 1. 开启 GPIOB 时钟，将 PB1（SD_CS）与 PA4（W25Q64_CS）均拉高 */
    RCC->APB2ENR |= RCC_APB2ENR_IOPBEN;
    GPIOA->BSRR = GPIO_BSRR_BS4;
    GPIOB->BSRR = GPIO_BSRR_BS1;
    GPIOB->CRL = (GPIOB->CRL & ~(0xFu << 4)) | (0x3u << 4);

    /* 2. 初始化阶段把 SPI1 时钟降到 72 MHz / 256 = 281.25 kHz（<= 400 kHz） */
    spi1_set_prescaler(SPI_CR1_BR);

    /* 3. CS 保持高电平，发送 10 字节（80 个时钟，>= 74）上电同步脉冲 */
    for (uint32_t i = 0; i < 10u; ++i) {
        spi1_xfer(0xFFu);
    }

    /* 4. CMD0：进入 SPI Idle 状态，期望 R1 = 0x01 */
    sd_cs_low();
    uint8_t r1 = sd_send_cmd(0u, 0u, 0x95u);
    sd_cs_high();
    if (r1 != 0x01u) {
        return false;
    }

    /* 5. CMD8：检查 2.7~3.6 V 电压范围与 0xAA 回显，确认 SD v2.0 */
    sd_cs_low();
    r1 = sd_send_cmd(8u, 0x000001AAUL, 0x87u);
    uint8_t r7[4] = {0};
    for (int i = 0; i < 4; ++i) {
        r7[i] = spi1_xfer(0xFFu);
    }
    sd_cs_high();
    if (r1 != 0x01u || r7[2] != 0x01u || r7[3] != 0xAAu) {
        return false;
    }

    /* 6. 循环发送 CMD55 + ACMD41(HCS=1)，直到 R1 = 0x00 退出 Idle */
    bool ready = false;
    for (uint32_t tries = 0; tries < 1000u; ++tries) {
        sd_cs_low();
        (void)sd_send_cmd(55u, 0u, 0x01u);
        sd_cs_high();

        sd_cs_low();
        r1 = sd_send_cmd(41u, 0x40000000UL, 0x01u);
        sd_cs_high();
        if (r1 == 0x00u) {
            ready = true;
            break;
        }
    }
    if (!ready) {
        return false;
    }

    /* 7. CMD58：读取 OCR 寄存器，检查第 30 位 CCS 判断是否按块寻址 */
    sd_cs_low();
    r1 = sd_send_cmd(58u, 0u, 0x01u);
    uint8_t ocr[4] = {0};
    for (int i = 0; i < 4; ++i) {
        ocr[i] = spi1_xfer(0xFFu);
    }
    sd_cs_high();
    if (r1 != 0x00u) {
        return false;
    }

    g_card_type = ((ocr[0] & 0x40u) != 0u) ? SD_TYPE_SDHC : SD_TYPE_SDSC_V2;
    if (out_type != 0) {
        *out_type = g_card_type;
    }

    /* 8. 初始化完成，将 SPI1 提速至 72 MHz / 4 = 18 MHz */
    spi1_set_prescaler(SPI_CR1_BR_0);
    return true;
}

bool sd_read_block(uint32_t lba, uint8_t buf[SD_BLOCK_SIZE]) {
    uint32_t addr = (g_card_type == SD_TYPE_SDHC) ? lba : (lba * SD_BLOCK_SIZE);

    sd_cs_low();
    if (sd_send_cmd(17u, addr, 0x01u) != 0x00u) {
        sd_cs_high();
        return false;
    }

    /* 等待数据起始令牌 0xFE */
    bool got_token = false;
    for (uint32_t i = 0; i < 50000u; ++i) {
        if (spi1_xfer(0xFFu) == 0xFEu) {
            got_token = true;
            break;
        }
    }
    if (!got_token) {
        sd_cs_high();
        return false;
    }

    for (uint32_t i = 0; i < SD_BLOCK_SIZE; ++i) {
        buf[i] = spi1_xfer(0xFFu);
    }
    /* 读出并丢弃 2 字节 CRC-16 */
    (void)spi1_xfer(0xFFu);
    (void)spi1_xfer(0xFFu);
    sd_cs_high();
    return true;
}

bool sd_write_block(uint32_t lba, const uint8_t buf[SD_BLOCK_SIZE]) {
    uint32_t addr = (g_card_type == SD_TYPE_SDHC) ? lba : (lba * SD_BLOCK_SIZE);

    sd_cs_low();
    if (sd_send_cmd(24u, addr, 0x01u) != 0x00u) {
        sd_cs_high();
        return false;
    }

    spi1_xfer(0xFFu); /* 间隔 1 字节 */
    spi1_xfer(0xFEu); /* 单块写起始令牌 */
    for (uint32_t i = 0; i < SD_BLOCK_SIZE; ++i) {
        spi1_xfer(buf[i]);
    }
    spi1_xfer(0xFFu); /* 伪 CRC 高字节 */
    spi1_xfer(0xFFu); /* 伪 CRC 低字节 */

    /* 数据响应令牌：低 5 位为 00101 (0x05) 表示接收成功 */
    uint8_t resp = spi1_xfer(0xFFu);
    if ((resp & 0x1Fu) != 0x05u) {
        sd_cs_high();
        return false;
    }

    /* 等待卡内部编程完成（MISO 从低电平恢复为 0xFF） */
    bool ok = sd_wait_ready(100000u);
    sd_cs_high();
    return ok;
}
