/* tests/mocks/flash.h
 *
 * Хостовая подмена hal/flash.h: RAM-модель Flash плюс функции,
 * которые вызывает common/meta.c, и утилиты для тестов.
 */

#ifndef FLASH_H
#define FLASH_H

#include <stdint.h>

void flash_read(uint32_t addr, void *dst, uint32_t len);
void flash_unlock(void);
void flash_lock(void);
int flash_erase_page_raw(uint32_t addr);
int flash_program_halfword_raw(uint32_t addr, uint16_t value);

/* --- только для тестов --- */
void mock_flash_reset(void);                       /* заполнить 0xFF */
void mock_flash_set(uint32_t addr, uint8_t value); /* точечная порча */

#endif /* FLASH_H */
