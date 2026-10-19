/* boot/src/request.c
 *
 * Диспетчер I2C-запросов загрузчика (протокол v2, один слот).
 * Фрейминг и константы — в common/protocol.h. Обработчики команд
 * сохранены 1:1 с исходной реализацией (важно для OTA-таймингов).
 */

#include <stdint.h>

#include "board.h"
#include "crc.h"
#include "flash.h"
#include "i2c.h"
#include "meta.h"
#include "protocol.h"
#include "system.h"

static uint8_t upd_state = UPD_IDLE;
static uint32_t upd_len = 0;
static uint32_t upd_crc = 0;
static uint32_t upd_version = 0;
static uint32_t upd_next_offset = 0;

static uint8_t last_status = ST_OK;

static void set_response(uint8_t status, const uint8_t *payload, uint16_t plen)
{
    last_status = status;
    i2c_set_response(status, payload, plen);
}

static void set_status(uint8_t status)
{
    set_response(status, NULL, 0);
}

void process_i2c_request(void)
{
    uint16_t frame_len;
    uint8_t cmd;
    uint16_t plen;

    if (!protocol_parse_request((const uint8_t *)rx_buf, rx_count, I2C_RX_BUF_SIZE, &frame_len,
                                &cmd, &plen))
    {
        (void)frame_len;
        set_status(ST_ERR_LEN);
        return;
    }

    const volatile uint8_t *p = &rx_buf[3];

    switch (cmd)
    {

    case CMD_GET_INFO:
    {
        uint8_t out[INFO_PAYLOAD_LEN];

        out[0] = DEVICE_TYPE_BOOT;
        out[1] = PROTO_VER;

        proto_put_le32(&out[2], meta.version);
        proto_put_le32(&out[6], meta.img_size);

        out[10] = (meta.flags & META_FLAG_PENDING) ? INFO_FLAG_PENDING : 0U;
        out[11] = 0; /* reserved */

        set_response(ST_OK, out, sizeof(out));
        break;
    }

    case CMD_BEGIN_UPDATE:
    {
        /* payload: len(4) + crc(4) + version(4) */
        if (plen != 12U)
        {
            set_status(ST_ERR_LEN);
            break;
        }

        uint32_t len = proto_get_le32((const uint8_t *)&p[0]);
        uint32_t crc = proto_get_le32((const uint8_t *)&p[4]);
        uint32_t ver = proto_get_le32((const uint8_t *)&p[8]);

        if (len == 0U || len > APP_SIZE)
        {
            set_status(ST_ERR_PARAM);
            break;
        }

        upd_len = len;
        upd_crc = crc;
        upd_version = ver;
        upd_next_offset = 0;
        upd_state = UPD_ACTIVE;

        set_status(ST_OK);
        break;
    }

    case CMD_WRITE_DATA:
    {
        if (upd_state == UPD_IDLE)
        {
            set_status(ST_ERR_STATE);
            break;
        }

        if (plen < 5U)
        {
            set_status(ST_ERR_LEN);
            break;
        }

        uint32_t offset = proto_get_le32((const uint8_t *)&p[0]);
        uint8_t dlen = p[4];

        if (plen != (uint16_t)(5U + dlen))
        {
            set_status(ST_ERR_LEN);
            break;
        }

        if (dlen == 0U || dlen > PROTO_MAX_DATA_LEN)
        {
            set_status(ST_ERR_PARAM);
            break;
        }

        if (offset != upd_next_offset)
        {
            set_status(ST_ERR_SEQ);
            break;
        }

        if ((offset & 1U) != 0U)
        {
            set_status(ST_ERR_PARAM);
            break;
        }

        if ((uint64_t)offset + dlen > upd_len)
        {
            set_status(ST_ERR_PARAM);
            break;
        }

        /*
         * Нечётный последний байт допускается только в конце образа.
         */
        if ((dlen & 1U) && ((offset + dlen) != upd_len))
        {
            set_status(ST_ERR_PARAM);
            break;
        }

        if ((offset % FLASH_PAGE_SIZE) == 0U)
        {
            flash_erase_page(APP_ADDR + offset);
        }

        uint8_t tmp[PROTO_MAX_DATA_LEN];

        for (uint16_t i = 0; i < dlen; i++)
        {
            tmp[i] = p[5 + i];
        }

        if (flash_program_buffer(APP_ADDR + offset, tmp, dlen) != 0)
        {
            set_status(ST_ERR_FLASH);
            break;
        }

        upd_next_offset += dlen;

        set_status(ST_OK);
        break;
    }

    case CMD_VERIFY_UPDATE:
    {
        if (upd_state == UPD_IDLE)
        {
            set_status(ST_ERR_STATE);
            break;
        }

        if (plen != 0U)
        {
            set_status(ST_ERR_LEN);
            break;
        }

        uint32_t crc = crc32_sw((const uint8_t *)APP_ADDR, upd_len);

        if (crc != upd_crc)
        {
            set_status(ST_ERR_CRC);
            break;
        }

        upd_state = UPD_VERIFIED;

        set_status(ST_OK);
        break;
    }

    case CMD_COMMIT:
    {
        if (upd_state != UPD_VERIFIED)
        {
            set_status(ST_ERR_STATE);
            break;
        }

        if (plen != 0U)
        {
            set_status(ST_ERR_LEN);
            break;
        }

        MetaConfig m = meta;

        m.img_size = upd_len;
        m.img_crc = upd_crc;
        m.version = upd_version;

        m.flags |= META_FLAG_VALID;
        m.flags |= META_FLAG_PENDING;
        m.boot_attempts = 0;

        if (!meta_save(&m))
        {
            set_status(ST_ERR_FLASH);
            break;
        }

        upd_state = UPD_IDLE;

        set_status(ST_OK);
        break;
    }

    case CMD_GET_STATUS:
    {
        uint8_t out[7];

        out[0] = last_status;
        out[1] = upd_state;
        out[2] = 0; /* reserved */

        proto_put_le32(&out[3], upd_next_offset);

        set_response(ST_OK, out, sizeof(out));
        break;
    }

    case CMD_RESET:
    {
        /*
         * Мастер может не ожидать ответ после этой команды.
         */
        for (volatile uint32_t i = 0; i < 200000UL; i++)
        {
            ;
        }

        system_reset();
        break;
    }

    case CMD_ENTER_UPDATE:
    {
        /* Подтверждение того, что устройство уже в режиме обновления. */
        set_status(ST_OK);
        break;
    }

    default:
    {
        set_status(ST_ERR_CMD);
        break;
    }
    }
}
