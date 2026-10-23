#include "w25q.h"

/* 复用 2.8 节 spi.c 的 SPI1 底层收发、片选与读函数（PA4 为 W25Q64 CS） */
extern void spi1_cs_low(void);
extern void spi1_cs_high(void);
extern uint8_t spi1_xfer(uint8_t tx);

static void w25q_send_addr24(uint32_t addr) {
    spi1_xfer((uint8_t)((addr >> 16) & 0xFFu));
    spi1_xfer((uint8_t)((addr >> 8) & 0xFFu));
    spi1_xfer((uint8_t)(addr & 0xFFu));
}

void w25q_write_enable(void) {
    spi1_cs_low();
    spi1_xfer(0x06u);
    spi1_cs_high();
}

void w25q_wait_busy(void) {
    while ((w25q_read_sr1() & W25Q_SR1_BUSY) != 0u) {
    }
}

void w25q_erase_sector(uint32_t addr) {
    w25q_write_enable();
    spi1_cs_low();
    spi1_xfer(0x20u);
    w25q_send_addr24(addr);
    spi1_cs_high();
    w25q_wait_busy();
}

/* 单页编程：写入范围不得跨越 256 字节页边界，每次拉高 CS 后 WEL 自动清零 */
void w25q_page_program(uint32_t addr, const uint8_t *data, size_t len) {
    if (len == 0u || len > W25Q_PAGE_SIZE) {
        return;
    }
    w25q_write_enable();
    spi1_cs_low();
    spi1_xfer(0x02u);
    w25q_send_addr24(addr);
    for (size_t i = 0; i < len; ++i) {
        spi1_xfer(data[i]);
    }
    spi1_cs_high();
    w25q_wait_busy();
}

/* 任意长度写入（调用前目标扇区须已擦除）：按 256 字节页边界拆分 */
void nor_write(uint32_t addr, const void *buf, size_t len) {
    const uint8_t *src = (const uint8_t *)buf;
    while (len > 0u) {
        size_t page_rem = W25Q_PAGE_SIZE - (addr & (W25Q_PAGE_SIZE - 1u));
        size_t chunk = (len < page_rem) ? len : page_rem;
        w25q_page_program(addr, src, chunk);
        addr += (uint32_t)chunk;
        src += chunk;
        len -= chunk;
    }
}
