/* common/crc.h
 *
 * Программные реализации CRC32 (IEEE 802.3, полином 0xEDB88320) и
 * CRC16-CCITT (полином 0x1021, init 0xFFFF), независимые от платформы.
 */

#ifndef CRC_H
#define CRC_H

#include <stdint.h>

uint32_t crc32_sw(const uint8_t *data, uint32_t len);
uint16_t crc16_ccitt(const uint8_t *data, uint32_t len);

#endif /* CRC_H */
