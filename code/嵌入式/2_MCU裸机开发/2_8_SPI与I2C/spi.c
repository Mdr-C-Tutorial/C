#include "spi.h"
#include "stm32f1xx.h"

void spi1_init(void) {
    /* 1. 开启 GPIOA 与 SPI1 时钟（均挂在 APB2，PCLK2 = 72 MHz） */
    RCC->APB2ENR |= RCC_APB2ENR_IOPAEN | RCC_APB2ENR_SPI1EN;

    /* 2. 先把 PA4（CS）输出锁存置高，避免配置为输出瞬间产生假片选脉冲 */
    GPIOA->BSRR = GPIO_BSRR_BS4;

    /* 3. 配置 PA4～PA7（CRL[31:16]）：
     *    PA4（[19:16]）：50 MHz 通用推挽输出（CNF=00, MODE=11 -> 0x3）
     *    PA5（[23:20]）：50 MHz 复用推挽输出（CNF=10, MODE=11 -> 0xB，SCK）
     *    PA6（[27:24]）：浮空输入            （CNF=01, MODE=00 -> 0x4，MISO）
     *    PA7（[31:28]）：50 MHz 复用推挽输出（CNF=10, MODE=11 -> 0xB，MOSI）
     */
    GPIOA->CRL = (GPIOA->CRL & ~(0xFFFFu << 16)) | (0xB4B3u << 16);

    /* 4. 配置 SPI1_CR1：
     *    模式 0（CPOL=0, CPHA=0）、主机模式（MSTR=1）、
     *    波特率 f_PCLK2 / 4 = 18 MHz（BR=001，已达 STM32F103 上限）、
     *    8 位数据帧（DFF=0）、高位先发（LSBFIRST=0）、
     *    软件管理 NSS（SSM=1, SSI=1），最后置 SPE=1 使能外设。
     */
    SPI1->CR2 = 0;
    SPI1->CR1 = SPI_CR1_SSM | SPI_CR1_SSI | SPI_CR1_BR_0 |
                SPI_CR1_MSTR | SPI_CR1_SPE;
}

void spi1_cs_low(void) {
    GPIOA->BSRR = GPIO_BSRR_BR4;
}

void spi1_cs_high(void) {
    while ((SPI1->SR & SPI_SR_BSY) != 0) {
    }
    GPIOA->BSRR = GPIO_BSRR_BS4;
}

uint8_t spi1_xfer(uint8_t tx) {
    while ((SPI1->SR & SPI_SR_TXE) == 0) {
    }
    SPI1->DR = tx;
    while ((SPI1->SR & SPI_SR_RXNE) == 0) {
    }
    return (uint8_t)SPI1->DR;
}

void w25q_read_jedec_id(uint8_t id[3]) {
    spi1_cs_low();
    spi1_xfer(0x9Fu);
    id[0] = spi1_xfer(0xFFu);
    id[1] = spi1_xfer(0xFFu);
    id[2] = spi1_xfer(0xFFu);
    spi1_cs_high();
}

uint8_t w25q_read_sr1(void) {
    spi1_cs_low();
    spi1_xfer(0x05u);
    uint8_t sr1 = spi1_xfer(0xFFu);
    spi1_cs_high();
    return sr1;
}

void w25q_read(uint32_t addr, void *buf, size_t len) {
    uint8_t *dst = (uint8_t *)buf;
    spi1_cs_low();
    spi1_xfer(0x03u);
    spi1_xfer((uint8_t)((addr >> 16) & 0xFFu));
    spi1_xfer((uint8_t)((addr >> 8) & 0xFFu));
    spi1_xfer((uint8_t)(addr & 0xFFu));
    for (size_t i = 0; i < len; ++i) {
        dst[i] = spi1_xfer(0xFFu);
    }
    spi1_cs_high();
}
