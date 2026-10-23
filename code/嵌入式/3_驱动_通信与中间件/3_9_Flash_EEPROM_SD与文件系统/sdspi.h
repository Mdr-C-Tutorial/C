#ifndef SDSPI_H
#define SDSPI_H

#include <stdbool.h>
#include <stdint.h>

#define SD_BLOCK_SIZE 512u

typedef enum {
    SD_TYPE_UNKNOWN = 0,
    SD_TYPE_SDSC_V2 = 1,
    SD_TYPE_SDHC    = 2
} sd_card_type_t;

bool sd_spi_init(sd_card_type_t *out_type);
bool sd_read_block(uint32_t lba, uint8_t buf[SD_BLOCK_SIZE]);
bool sd_write_block(uint32_t lba, const uint8_t buf[SD_BLOCK_SIZE]);

#endif /* SDSPI_H */
