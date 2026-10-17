/* common/protocol.h
 *
 * Фрейминг I2C-протокола v2 (один слот приложения).
 *
 *   Кадр запроса:  [LEN_LO][LEN_HI][CMD][PAYLOAD...]
 *   Кадр ответа:   [LEN_LO][LEN_HI][STATUS][PAYLOAD...]
 *   LEN = количество байт после него (CMD/STATUS + PAYLOAD).
 *
 * Здесь только константы и разбор/сборка кадров — без привязки к
 * железу, поэтому модуль тестируется на хосте.
 */

#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "types.h"

/* ========================================================================
 * Команды
 * ======================================================================== */

#define CMD_GET_INFO        0x01U
#define CMD_BEGIN_UPDATE    0x02U
#define CMD_WRITE_DATA      0x03U
#define CMD_VERIFY_UPDATE   0x04U
#define CMD_COMMIT          0x05U
#define CMD_GET_STATUS      0x07U
#define CMD_RESET           0x08U
#define CMD_ENTER_UPDATE    0x0EU

/* ========================================================================
 * Ответ GET_INFO (12 байт, единый для загрузчика и приложения):
 *   [0]     device_type (device_type_t)
 *   [1]     версия протокола
 *   [2..5]  версия прошивки, LE32
 *   [6..9]  размер образа, LE32
 *   [10]    флаги: бит 0 = pending
 *   [11]    дополнительно (у приложения — число ячеек батареи)
 * ======================================================================== */

#define PROTO_VER           2
#define INFO_PAYLOAD_LEN    12
#define INFO_FLAG_PENDING   (1U << 0)

/* Максимальный размер данных в одном кадре WRITE_DATA. */
#define PROTO_MAX_DATA_LEN  128U

/* ========================================================================
 * Контракт входа в режим обновления.
 *
 * Приложение перед сбросом пишет магическое число в зарезервированную
 * область в конце RAM; загрузчик проверяет её при старте. Верхние
 * 16 байт RAM исключены из стека/кучи в обоих линкер-скриптах.
 * ======================================================================== */

#define UPDATE_MAGIC        0x55AA55AAUL
#define UPDATE_FLAG_ADDR    0x20004FFCUL

/* ========================================================================
 * Little-endian упаковка/распаковка
 * ======================================================================== */

static inline void proto_put_le16(uint8_t *p, uint16_t v)
{
    p[0] = (uint8_t)(v & 0xFFU);
    p[1] = (uint8_t)(v >> 8);
}

static inline void proto_put_le32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)(v & 0xFFU);
    p[1] = (uint8_t)(v >> 8);
    p[2] = (uint8_t)(v >> 16);
    p[3] = (uint8_t)(v >> 24);
}

static inline uint16_t proto_get_le16(const uint8_t *p)
{
    return (uint16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8));
}

static inline uint32_t proto_get_le32(const uint8_t *p)
{
    return (uint32_t)p[0] |
           ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) |
           ((uint32_t)p[3] << 24);
}

/* ========================================================================
 * Разбор заголовка кадра запроса.
 *
 * buf     — буфер принятых байт
 * count   — сколько байт принято
 * max_len — вместимость буфера (для проверки LEN)
 *
 * Возврат: true — кадр корректен, поля заполнены.
 * ======================================================================== */

bool protocol_parse_request(const uint8_t *buf, uint16_t count, uint16_t max_len,
                            uint16_t *frame_len, uint8_t *cmd, uint16_t *payload_len);

/* Диспетчер запросов загрузчика (реализация — boot/src/request.c). */
void process_i2c_request(void);

#endif /* PROTOCOL_H */