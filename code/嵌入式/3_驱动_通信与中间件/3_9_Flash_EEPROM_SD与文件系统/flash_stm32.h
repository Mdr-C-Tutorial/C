#ifndef FLASH_STM32_H
#define FLASH_STM32_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define STM32_FLASH_PAGE_SIZE 1024u

bool flash_stm32_unlock(void);
void flash_stm32_lock(void);
bool flash_stm32_erase_page(uint32_t page_addr);
bool flash_stm32_program_halfword(uint32_t addr, uint16_t data);
bool flash_stm32_program_buf(uint32_t addr, const void *buf, size_t len);

#endif /* FLASH_STM32_H */
