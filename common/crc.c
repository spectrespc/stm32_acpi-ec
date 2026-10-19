/* common/crc.c */

#include "crc.h"

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

uint16_t crc16_ccitt(const uint8_t *data, uint32_t len)
{
    uint16_t crc = 0xFFFF;

    while (len--)
    {
        crc ^= (uint16_t)*data++ << 8;

        for (int i = 0; i < 8; i++)
        {
            if (crc & 0x8000U)
            {
                crc = (uint16_t)((crc << 1) ^ 0x1021U);
            }
            else
            {
                crc = (uint16_t)(crc << 1);
            }
        }
    }

    return crc;
}
