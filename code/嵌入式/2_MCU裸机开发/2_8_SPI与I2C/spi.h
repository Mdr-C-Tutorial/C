#ifndef SPI_H
#define SPI_H

#include <stddef.h>
#include <stdint.h>

void spi1_init(void);
void spi1_cs_low(void);
void spi1_cs_high(void);
uint8_t spi1_xfer(uint8_t tx);

void w25q_read_jedec_id(uint8_t id[3]);
uint8_t w25q_read_sr1(void);
void w25q_read(uint32_t addr, void *buf, size_t len);

#endif /* SPI_H */
