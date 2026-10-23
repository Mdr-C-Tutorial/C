#include "flash_stm32.h"
#include "stm32f1xx.h"

#ifndef FLASH_KEY1
#define FLASH_KEY1 0x45670123UL
#endif
#ifndef FLASH_KEY2
#define FLASH_KEY2 0xCDEF89ABUL
#endif

#ifndef FLASH_SR_BSY
#define FLASH_SR_BSY      (1UL << 0)
#define FLASH_SR_PGERR    (1UL << 2)
#define FLASH_SR_WRPRTERR (1UL << 4)
#define FLASH_SR_EOP      (1UL << 5)
#endif

#ifndef FLASH_CR_PG
#define FLASH_CR_PG   (1UL << 0)
#define FLASH_CR_PER  (1UL << 1)
#define FLASH_CR_MER  (1UL << 2)
#define FLASH_CR_STRT (1UL << 6)
#define FLASH_CR_LOCK (1UL << 7)
#endif

static void flash_wait_busy(void) {
    while ((FLASH->SR & FLASH_SR_BSY) != 0u) {
    }
}

static void flash_clear_flags(void) {
    /* EOP、PGERR、WRPRTERR 均为写 1 清零（rc_w1） */
    FLASH->SR = FLASH_SR_EOP | FLASH_SR_PGERR | FLASH_SR_WRPRTERR;
}

bool flash_stm32_unlock(void) {
    if ((FLASH->CR & FLASH_CR_LOCK) != 0u) {
        FLASH->KEYR = FLASH_KEY1;
        FLASH->KEYR = FLASH_KEY2;
    }
    return (FLASH->CR & FLASH_CR_LOCK) == 0u;
}

void flash_stm32_lock(void) {
    FLASH->CR |= FLASH_CR_LOCK;
}

bool flash_stm32_erase_page(uint32_t page_addr) {
    flash_wait_busy();
    flash_clear_flags();

    FLASH->CR |= FLASH_CR_PER;
    FLASH->AR = page_addr;
    FLASH->CR |= FLASH_CR_STRT;

    /* 擦除期间若 CPU 从同一块 Flash 取指，流水线会停顿直至擦除结束 */
    flash_wait_busy();
    FLASH->CR &= ~FLASH_CR_PER;

    if ((FLASH->SR & (FLASH_SR_PGERR | FLASH_SR_WRPRTERR)) != 0u) {
        return false;
    }
    return true;
}

bool flash_stm32_program_halfword(uint32_t addr, uint16_t data) {
    if ((addr & 1u) != 0u) {
        return false;
    }
    flash_wait_busy();
    flash_clear_flags();

    FLASH->CR |= FLASH_CR_PG;
    *(volatile uint16_t *)addr = data;
    flash_wait_busy();
    FLASH->CR &= ~FLASH_CR_PG;

    if ((FLASH->SR & (FLASH_SR_PGERR | FLASH_SR_WRPRTERR)) != 0u) {
        return false;
    }
    return *(volatile const uint16_t *)addr == data;
}

bool flash_stm32_program_buf(uint32_t addr, const void *buf, size_t len) {
    if ((addr & 1u) != 0u || (len & 1u) != 0u) {
        return false;
    }
    const uint8_t *src = (const uint8_t *)buf;
    for (size_t i = 0; i < len; i += 2u) {
        uint16_t hw = (uint16_t)src[i] | ((uint16_t)src[i + 1u] << 8);
        if (!flash_stm32_program_halfword(addr + (uint32_t)i, hw)) {
            return false;
        }
    }
    return true;
}
