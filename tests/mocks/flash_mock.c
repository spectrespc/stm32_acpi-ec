/* tests/mocks/flash_mock.c
 *
 * RAM-модель внутренней Flash (C8T6, 64 KB). Адреса — абсолютные
 * (как на устройстве), доступ идёт через смещение от FLASH_BASE_ADDR.
 */

#include <string.h>

#include "board.h"
#include "flash.h"

static uint8_t mem[FLASH_TOTAL_SIZE];

static uint8_t *at(uint32_t addr)
{
    return &mem[addr - FLASH_BASE_ADDR];
}

void mock_flash_reset(void)
{
    memset(mem, 0xFF, sizeof(mem));
}

void mock_flash_set(uint32_t addr, uint8_t value)
{
    *at(addr) = value;
}

void flash_read(uint32_t addr, void *dst, uint32_t len)
{
    memcpy(dst, at(addr), len);
}

void flash_unlock(void)
{
    /* no-op на хосте */
}

void flash_lock(void)
{
    /* no-op на хосте */
}

int flash_erase_page_raw(uint32_t addr)
{
    memset(at(addr), 0xFF, FLASH_PAGE_SIZE);
    return 0;
}

int flash_program_halfword_raw(uint32_t addr, uint16_t value)
{
    uint8_t *p = at(addr);
    p[0] = (uint8_t)(value & 0xFFU);
    p[1] = (uint8_t)(value >> 8);
    return 0;
}