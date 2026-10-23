#include "at24c02.h"
#include "flash_cfg.h"
#include "flash_stm32.h"
#include "sdspi.h"
#include "stm32f1xx.h"
#include "w25q.h"

extern const uint8_t _sconfig[];
extern const uint8_t _econfig[];

static bool stm32_cfg_erase(uint32_t page_offset) {
    uint32_t addr = (uint32_t)(uintptr_t)_sconfig + page_offset;
    if (!flash_stm32_unlock()) {
        return false;
    }
    bool ok = flash_stm32_erase_page(addr);
    flash_stm32_lock();
    return ok;
}

static bool stm32_cfg_prog_hw(uint32_t offset, uint16_t halfword) {
    uint32_t addr = (uint32_t)(uintptr_t)_sconfig + offset;
    if (!flash_stm32_unlock()) {
        return false;
    }
    bool ok = flash_stm32_program_halfword(addr, halfword);
    flash_stm32_lock();
    return ok;
}

static void stm32_cfg_read(uint32_t offset, void *buf, size_t len) {
    const uint8_t *src = _sconfig + offset;
    uint8_t *dst = (uint8_t *)buf;
    for (size_t i = 0; i < len; ++i) {
        dst[i] = src[i];
    }
}

static const flash_ops_t g_stm32_flash_ops = {
    .erase_page = stm32_cfg_erase,
    .program_halfword = stm32_cfg_prog_hw,
    .read_bytes = stm32_cfg_read,
};

int main(void) {
    cfg_record_t cfg;
    if (!flash_cfg_load(&g_stm32_flash_ops, &cfg)) {
        cfg.node_addr = 1u;
        cfg.report_sec = 10u;
        cfg.calib_offset = 0;
        cfg.calib_gain = 1000;
        (void)flash_cfg_save(&g_stm32_flash_ops, &cfg);
    }

    uint8_t eeprom_buf[16] = {0};
    (void)at24c02_write(0x04u, eeprom_buf, sizeof(eeprom_buf));
    (void)at24c02_read(0x04u, eeprom_buf, sizeof(eeprom_buf));

    w25q_erase_sector(0x000000u);
    nor_write(0x0000F0u, eeprom_buf, sizeof(eeprom_buf));

    sd_card_type_t card_type = SD_TYPE_UNKNOWN;
    if (sd_spi_init(&card_type)) {
        uint8_t sector0[SD_BLOCK_SIZE];
        (void)sd_read_block(0u, sector0);
    }

    (void)_econfig;
    for (;;) {
    }
}
