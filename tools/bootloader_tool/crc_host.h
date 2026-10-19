/* tools/bootloader_tool/crc_host.h
 *
 * Хостовая реализация CRC32. Полностью совпадает с common/crc.c
 * (crc32_sw) и с CRC32 образа в устройстве. Хранится отдельно, чтобы
 * утилита собиралась нативным компилятором без include-путей SDK.
 */

#ifndef CRC_HOST_H
#define CRC_HOST_H

#include <stdint.h>

uint32_t crc32_sw(const uint8_t *data, uint32_t len);

#endif /* CRC_HOST_H */
