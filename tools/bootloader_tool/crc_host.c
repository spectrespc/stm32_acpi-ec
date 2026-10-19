/* tools/bootloader_tool/crc_host.c */

#include "crc_host.h"

uint32_t crc32_sw(const uint8_t *data, uint32_t len)
{
    uint32_t crc = 0xFFFFFFFFUL;

    while (len--)
    {
        crc ^= *data++;

        for (int i = 0; i < 8; i++)
        {
            uint32_t mask = -(uint32_t)(crc & 1U);
            crc = (crc >> 1) ^ (0xEDB88320UL & mask);
        }
    }

    return ~crc;
}
