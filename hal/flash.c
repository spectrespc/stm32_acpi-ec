/* hal/flash.c
 *
 * Объединённая реализация для загрузчика и приложения.
 * Ядро (flash_*_raw) не трогает блокировку Flash и I2C; обёртки
 * flash_erase_page()/flash_program_buffer() в загрузчике (BOOT_BUILD)
 * приостанавливают обслуживание I2C на время операции.
 */

#include <string.h>

#include "flash.h"
#include "board.h"

#ifdef BOOT_BUILD
#include "i2c.h"
#endif

static void flash_wait_busy(void)
{
    while (FLASH->SR & FLASH_SR_BSY) {
        ;
    }
}

void flash_read(uint32_t addr, void *dst, uint32_t len)
{
    memcpy(dst, (const void *)addr, len);
}

void flash_unlock(void)
{
    if (FLASH->CR & FLASH_CR_LOCK) {
        FLASH->KEYR = FLASH_KEY1;
        FLASH->KEYR = FLASH_KEY2;
    }
}

void flash_lock(void)
{
    FLASH->CR |= FLASH_CR_LOCK;
}

int flash_erase_page_raw(uint32_t addr)
{
    flash_wait_busy();

    FLASH->CR |= FLASH_CR_PER;
    FLASH->AR  = addr;
    FLASH->CR |= FLASH_CR_STRT;

    flash_wait_busy();

    FLASH->CR &= ~FLASH_CR_PER;

    return 0;
}

int flash_program_halfword_raw(uint32_t addr, uint16_t value)
{
    flash_wait_busy();

    FLASH->CR |= FLASH_CR_PG;

    *(__IO uint16_t *)addr = value;

    flash_wait_busy();

    FLASH->CR &= ~FLASH_CR_PG;

    if (*(__IO uint16_t *)addr != value) {
        return -1;
    }

    return 0;
}

int flash_program_halfword(uint32_t addr, uint16_t value)
{
    return flash_program_halfword_raw(addr, value);
}

int flash_erase_page(uint32_t addr)
{
#ifdef BOOT_BUILD
    i2c_pause();
    flash_unlock();
    flash_erase_page_raw(addr);
    flash_lock();
    i2c_resume();
    return 0;
#else
    return flash_erase_page_raw(addr);
#endif
}

#ifdef BOOT_BUILD
int flash_program_buffer(uint32_t addr, const uint8_t *data, uint16_t len)
{
    i2c_pause();

    flash_unlock();

    int rc = 0;

    for (uint16_t i = 0; i < len; ) {
        uint16_t hw;

        if ((i + 1U) < len) {
            hw = (uint16_t)data[i] | ((uint16_t)data[i + 1U] << 8);
        } else {
            hw = (uint16_t)data[i] | 0xFF00U;
        }

        if (flash_program_halfword_raw(addr + i, hw) != 0) {
            rc = -1;
            break;
        }

        i += 2U;
    }

    flash_lock();

    i2c_resume();

    return rc;
}
#endif /* BOOT_BUILD */