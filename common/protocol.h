/* common/protocol.h
 *
 * Фрейминг I2C-протокола v3 (один слот приложения).
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

#define CMD_GET_INFO      0x01U
#define CMD_BEGIN_UPDATE  0x02U
#define CMD_WRITE_DATA    0x03U
#define CMD_VERIFY_UPDATE 0x04U
#define CMD_COMMIT        0x05U
#define CMD_GET_STATUS    0x07U
#define CMD_RESET         0x08U
#define CMD_ENTER_UPDATE  0x0EU
#define CMD_GET_POWER     0x20U
#define CMD_SET_POWER     0x21U

/* Отладочные команды (только сборка с -DDEBUG_BUILD; в проде — 0xFF).
 * 0x28 — снимок счётчиков I2C (I2C_DEBUG_PAYLOAD_LEN байт, layout —
 * docs/protocol.md); 0x29 — воспроизвести удержание SCL: payload
 * [0]=1 включает режим, [0]=0 выключает. */
#define CMD_GET_DEBUG      0x28U
#define CMD_DEBUG_HOLD_SCL 0x29U

/* ========================================================================
 * Ответ GET_INFO (12 байт, единый для загрузчика и приложения):
 *   [0]     device_type (device_type_t)
 *   [1]     версия протокола
 *   [2..5]  версия прошивки, LE32
 *   [6..9]  размер образа, LE32
 *   [10]    флаги: бит 0 = pending
 *   [11]    дополнительно (у приложения — число ячеек батареи)
 * ======================================================================== */

#define PROTO_VER         3
#define INFO_PAYLOAD_LEN  12
#define INFO_FLAG_PENDING (1U << 0)

/* Максимальный размер данных в одном кадре WRITE_DATA. */
#define PROTO_MAX_DATA_LEN 128U

/* ========================================================================
 * Контракт входа в режим обновления.
 *
 * Приложение перед сбросом пишет магическое число в зарезервированную
 * область в конце RAM; загрузчик проверяет её при старте. Верхние
 * 16 байт RAM исключены из стека/кучи в обоих линкер-скриптах, а адрес
 * задаётся символом __ec_update_flag из линкер-скрипта (0x20004FFC для
 * F103, 0x20002FFC для C031).
 * ======================================================================== */

#define UPDATE_MAGIC 0x55AA55AAUL

extern volatile uint32_t __ec_update_flag;

/* ========================================================================
 * CMD_GET_POWER (0x20) — 8 байт ответа; CMD_SET_POWER (0x21) — 2 байта
 * запроса. Значения полей — общий контракт с хостом (docs/protocol.md).
 * ======================================================================== */

#define POWER_PAYLOAD_LEN     8U
#define POWER_SET_PAYLOAD_LEN 2U

typedef enum
{
    HOST_STATE_OFF = 0,
    HOST_STATE_BOOTING = 1,
    HOST_STATE_AWAKE = 2,
    HOST_STATE_SUSPENDING = 3,
    HOST_STATE_SUSPENDED = 4,
    HOST_STATE_SHUTTING_DOWN = 5,
    HOST_STATE_REBOOTING = 6,
    HOST_STATE_ERROR = 7
} host_state_t;

/* flags (байт 1 ответа GET_POWER). */
#define POWER_FLAG_PWR_EN     (1U << 0)
#define POWER_FLAG_PWR_GOOD   (1U << 1)
#define POWER_FLAG_HOST_STATE (1U << 2)
#define POWER_FLAG_LINK_LOST  (1U << 3)
#define POWER_FLAG_BOOT_FAIL  (1U << 4)
#define POWER_FLAG_LSE_OK     (1U << 5)

/* power_request (байт 2). */
#define POWER_REQ_NONE     0U
#define POWER_REQ_SLEEP    1U
#define POWER_REQ_SHUTDOWN 2U
#define POWER_REQ_REBOOT   3U

/* Последний распознанный жест (байт 3). */
#define POWER_EVENT_NONE        0U
#define POWER_EVENT_SHORT       1U
#define POWER_EVENT_LONG        2U
#define POWER_EVENT_VERY_LONG   3U
#define POWER_EVENT_DOUBLE      4U
#define POWER_EVENT_LONG_IN_OFF 5U

/* last_wake_src (байт 6). */
#define POWER_WAKE_NONE   0U
#define POWER_WAKE_BUTTON 1U
#define POWER_WAKE_I2C    2U
#define POWER_WAKE_LPTIM  3U
#define POWER_WAKE_RESET  4U

/* ack_flags в CMD_SET_POWER: бит 0 — power_request принят. */
#define POWER_ACK_REQUEST (1U << 0)

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
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
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
