/**
 * stubs.c — stub implementations for CMSIS/HAL functions
 * not provided by the minimal HAL bundle
 */
#include "stm32f1xx_hal.h"

/* ── FLASH_PageErase — required by HAL flash driver ── */
void FLASH_PageErase(uint32_t PageAddress)
{
    while (FLASH->SR & FLASH_SR_BSY);

    if (FLASH->CR & FLASH_CR_LOCK) {
        FLASH->KEYR = 0x45670123;
        FLASH->KEYR = 0xCDEF89AB;
    }

    FLASH->CR |= FLASH_CR_PER;
    FLASH->AR   = PageAddress;
    FLASH->CR  |= FLASH_CR_STRT;

    while (FLASH->SR & FLASH_SR_BSY);

    if (FLASH->SR & FLASH_SR_EOP) {
        FLASH->SR = FLASH_SR_EOP;
    }

    FLASH->CR &= ~FLASH_CR_PER;
    FLASH->CR |= FLASH_CR_LOCK;
}

/* ── __CLZ — Count Leading Zeros (CMSIS intrinsic) ── */
uint8_t __CLZ(uint32_t data)
{
    uint8_t cnt = 0;
    if (data == 0) return 32;
    while ((data & 0x80000000) == 0) {
        data <<= 1;
        cnt++;
    }
    return cnt;
}
