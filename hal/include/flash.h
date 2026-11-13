/* hal/include/flash.h
 *
 * Операции с внутренней Flash. Семейство MCU задаёт размер страницы
 * (FLASH_PAGE_SIZE в board_family.h) и способ программирования:
 *   - F103: полуслова по 2 байта, страница 1 KB;
 *   - C031: double-word по 8 байт, страница 2 KB.
 *
 * Функции *_raw не управляют ни блокировкой Flash, ни паузой I2C —
 * вызывающий код сам решает, когда разблокировать Flash и приостановить
 * обслуживание I2C (стирание страницы занимает ~20-40 мс).
 */

#ifndef FLASH_H
#define FLASH_H

#include <stdint.h>

#ifndef FLASH_KEY1
#define FLASH_KEY1 0x45670123UL
#endif

#ifndef FLASH_KEY2
#define FLASH_KEY2 0xCDEF89ABUL
#endif

/* Чтение из Flash через memcpy-совместимый доступ
 * (может быть подменено моком в хостовых тестах). */
void flash_read(uint32_t addr, void *dst, uint32_t len);

void flash_unlock(void);
void flash_lock(void);

int flash_erase_page_raw(uint32_t addr);

/* Записать буфер. Для C031 длина дополняется до кратности 8 байтам
 * значением 0xFF (double-word programming), адрес должен быть
 * выровнен на 8 байт. */
int flash_program_buffer_raw(uint32_t addr, const uint8_t *data, uint16_t len);

/* Операции «верхнего уровня». В загрузчике они приостанавливают I2C,
 * в приложении — нет (запись выполняется до инициализации I2C). */
int flash_erase_page(uint32_t addr);

#ifdef BOOT_BUILD
/* Запись произвольного буфера (загрузчик: с паузой I2C). */
int flash_program_buffer(uint32_t addr, const uint8_t *data, uint16_t len);
#endif

#endif /* FLASH_H */