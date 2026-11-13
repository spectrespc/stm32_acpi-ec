/* hal/f1/flash.c
 *
 * STM32F103 medium density: страница 1 KB, программирование
 * полусловами по 2 байта.
 *
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
    while (FLASH->SR & FLASH_SR_BSY)
    {
        ;
    }
}

void flash_read(uint32_t addr, void *dst, uint32_t len)
{
    memcpy(dst, (const void *)addr, len);
}

void flash_unlock(void)
{
    if (FLASH->CR & FLASH_CR_LOCK)
    {
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
    FLASH->AR = addr;
    FLASH->CR |= FLASH_CR_STRT;

    flash_wait_busy();

    FLASH->CR &= ~FLASH_CR_PER;

    return 0;
}

int flash_program_buffer_raw(uint32_t addr, const uint8_t *data, uint16_t len)
{
    for (uint16_t i = 0; i < len; i += 2U)
    {
        flash_wait_busy();

        uint16_t hw = (uint16_t)data[i];

        if ((uint16_t)(i + 1U) < len)
        {
            hw |= (uint16_t)((uint16_t)data[i + 1U] << 8);
        }
        else
        {
            hw |= 0xFF00U;
        }

        FLASH->CR |= FLASH_CR_PG;

        *(__IO uint16_t *)(addr + i) = hw;

        flash_wait_busy();

        FLASH->CR &= ~FLASH_CR_PG;

        /* cppcheck-suppress knownConditionTrueFalse
         * Чтение регистра Flash после программирования — аппаратное
         * значение, cppcheck не учитывает изменение памяти. */
        if (*(__IO uint16_t *)(addr + i) != hw)
        {
            return -1;
        }
    }

    return 0;
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

    int rc = flash_program_buffer_raw(addr, data, len);

    flash_lock();

    i2c_resume();

    return rc;
}
#endif /* BOOT_BUILD */