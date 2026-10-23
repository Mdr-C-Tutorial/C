#ifndef W25Q_H
#define W25Q_H

#include <stddef.h>
#include <stdint.h>

#define W25Q_PAGE_SIZE   256u
#define W25Q_SECTOR_SIZE 4096u
#define W25Q_SECTOR_CNT  2048u

#define W25Q_SR1_BUSY (1u << 0)
#define W25Q_SR1_WEL  (1u << 1)

void w25q_write_enable(void);
uint8_t w25q_read_sr1(void);
void w25q_wait_busy(void);
void w25q_erase_sector(uint32_t addr);
void w25q_page_program(uint32_t addr, const uint8_t *data, size_t len);
void nor_write(uint32_t addr, const void *buf, size_t len);
void w25q_read(uint32_t addr, void *buf, size_t len);

#endif /* W25Q_H */
