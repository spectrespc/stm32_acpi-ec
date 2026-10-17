/* boot/inc/boot.h
 *
 * Загрузчик: валидация образа, переход в приложение и диспетчер
 * I2C-запросов.
 */

#ifndef BOOT_H
#define BOOT_H

#include <stdint.h>
#include <stdbool.h>

/* Проверить образ приложения (SP/PC/размер/CRC32). */
bool app_validate(void);

/* Перейти в приложение по адресу addr (не возвращает управление). */
void boot_app(uint32_t addr);

/* Проверить и запустить выбранный образ; при ошибке — вернуться. */
void try_boot_selected(void);

/* Обработать один I2C-запрос из rx_buf (см. common/protocol.h). */
void process_i2c_request(void);

#endif /* BOOT_H */