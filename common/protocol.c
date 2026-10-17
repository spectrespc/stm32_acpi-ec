/* common/protocol.c
 *
 * Чистая (без привязки к железу) логика фрейминга протокола v2.
 */

#include "protocol.h"

bool protocol_parse_request(const uint8_t *buf, uint16_t count, uint16_t max_len,
                            uint16_t *frame_len, uint8_t *cmd, uint16_t *payload_len)
{
    uint16_t len = (uint16_t)((uint16_t)buf[0] | ((uint16_t)buf[1] << 8));

    if (len < 1U) {
        return false;
    }

    if ((uint16_t)(len + 2U) > count) {
        return false;
    }

    if ((uint16_t)(len + 2U) > max_len) {
        return false;
    }

    if (frame_len != NULL) {
        *frame_len = len;
    }

    if (cmd != NULL) {
        *cmd = buf[2];
    }

    if (payload_len != NULL) {
        *payload_len = (uint16_t)(len - 1U);
    }

    return true;
}