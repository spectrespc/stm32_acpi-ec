/* common/types.h
 *
 * Общие перечисления протокола, не зависящие от платформы.
 */

#ifndef COMMON_TYPES_H
#define COMMON_TYPES_H

/* Тип устройства в ответе GET_INFO. */
typedef enum {
    DEVICE_TYPE_BOOT = 1,   /* загрузчик STM32 */
    DEVICE_TYPE_APP  = 2    /* приложение STM32 */
} device_type_t;

/* Статусы протокола v2 (совпадают с утилитой прошивки). */
typedef enum {
    ST_OK        = 0x00,
    ST_ERR_CMD   = 0x01,
    ST_ERR_PARAM = 0x02,
    ST_ERR_STATE = 0x03,
    ST_ERR_FLASH = 0x05,
    ST_ERR_CRC   = 0x06,
    ST_ERR_SEQ   = 0x07,
    ST_ERR_LEN   = 0x08
} protocol_status_t;

/* Состояние процесса обновления. */
typedef enum {
    UPD_IDLE = 0,
    UPD_ACTIVE,
    UPD_VERIFIED
} update_state_t;

#endif /* COMMON_TYPES_H */