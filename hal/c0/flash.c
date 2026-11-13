/* hal/c0/flash.c
 *
 * STM32C031F6P6: страница 2 KB, программирование double-word (8 байт),
 * PNB — номер страницы (0..15).
 *
 * Во время стирания/программирования ядро останавливается (нет
 * доступа к Flash), поэтому обёртки приостанавливают I2C (PE=0) до и
 * после операции — и в загрузчике, и в приложении. Хост выдерживает
 * паузу I2C_WRITE_READ_DELAY_US (100 мс), которая перекрывает erase
 * ~22 мс.
 */

#include <string.h>

#include "flash.h"
#include "board.h"
#include "i2c.h"

/* Карта памяти из board.h должна совпадать с таблицей docs/flash_map.md. */
_Static_assert(APP_SIZE == (20U * 1024U), "C031 app region must be 20 KB");
_Static_assert(META_PAGE0_ADDR == 0x08007000UL, "C031 meta page 0");
_Static_assert(META_PAGE1_ADDR == 0x08007800UL, "C031 meta page 1");
_Static_assert(FLASH_PAGE_SIZE == 2048U, "C031 flash page must be 2 KB");

#define FLASH_ERRORS                                                                               \
    (FLASH_SR_PROGERR | FLASH_SR_PGAERR | FLASH_SR_SIZERR | FLASH_SR_PGSERR | FLASH_SR_WRPERR)

static void flash_wait_busy(void)
{
    while (FLASH->SR & FLASH_SR_BSY1)
    {
        ;
    }
}

static int flash_clear_status(void)
{
    uint32_t sr = FLASH->SR & (FLASH_SR_EOP | FLASH_ERRORS);

    FLASH->SR = sr;

    return (sr & FLASH_ERRORS) ? -1 : 0;
}

void flash_read(uint32_t addr, void *dst, uint32_t len)
{
    memcpy(dst, (const void *)addr, len);
}

void flash_unlock(void)
{
    RCC->AHBENR |= RCC_AHBENR_FLASHEN;

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
    uint32_t page = (addr - FLASH_BASE_ADDR) / FLASH_PAGE_SIZE;

    if (page >= (FLASH_TOTAL_SIZE / FLASH_PAGE_SIZE))
    {
        return -1;
    }

    flash_wait_busy();

    FLASH->CR &= ~(FLASH_CR_PER | FLASH_CR_PNB);
    FLASH->CR |= FLASH_CR_PER | (page << FLASH_CR_PNB_Pos);
    FLASH->CR |= FLASH_CR_STRT;

    flash_wait_busy();

    FLASH->CR &= ~(FLASH_CR_PER | FLASH_CR_PNB);

    return flash_clear_status();
}

int flash_program_buffer_raw(uint32_t addr, const uint8_t *data, uint16_t len)
{
    if ((addr % FLASH_PROGRAM_UNIT) != 0U)
    {
        return -1;
    }

    for (uint16_t i = 0; i < len; i += FLASH_PROGRAM_UNIT)
    {
        uint32_t lo = 0xFFFFFFFFUL;
        uint32_t hi = 0xFFFFFFFFUL;

        for (uint32_t b = 0; b < 4U; b++)
        {
            if ((uint16_t)(i + b) < len)
            {
                lo &= ~(0xFFUL << (8U * b));
                lo |= (uint32_t)data[i + b] << (8U * b);
            }

            if ((uint16_t)(i + 4U + b) < len)
            {
                hi &= ~(0xFFUL << (8U * b));
                hi |= (uint32_t)data[i + 4U + b] << (8U * b);
            }
        }

        flash_wait_busy();

        FLASH->CR |= FLASH_CR_PG;

        /* Double-word: сначала младшее слово, затем старшее. */
        *(__IO uint32_t *)(addr + i) = lo;
        *(__IO uint32_t *)(addr + i + 4U) = hi;

        flash_wait_busy();

        FLASH->CR &= ~FLASH_CR_PG;

        if (flash_clear_status() != 0)
        {
            return -1;
        }

        uint16_t chunk = (uint16_t)(len - i);

        if (chunk > (uint16_t)FLASH_PROGRAM_UNIT)
        {
            chunk = (uint16_t)FLASH_PROGRAM_UNIT;
        }

        if (memcmp((const void *)(addr + i), data + i, chunk) != 0)
        {
            return -1;
        }
    }

    return 0;
}

int flash_erase_page(uint32_t addr)
{
    /* И в загрузчике, и в приложении: PE=0 отпускает линии, пока ядро
     * занято стиранием страницы (~20 мс). */
    i2c_pause();
    flash_unlock();
    int rc = flash_erase_page_raw(addr);
    flash_lock();
    i2c_resume();
    return rc;
}

int flash_program_buffer(uint32_t addr, const uint8_t *data, uint16_t len)
{
    i2c_pause();

    flash_unlock();

    int rc = flash_program_buffer_raw(addr, data, len);

    flash_lock();

    i2c_resume();

    return rc;
}