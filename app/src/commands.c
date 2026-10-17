/* app/src/commands.c
 *
 * Обработчики I2C-команд приложения (ACPI-EC эмулятор батареи).
 * Фрейминг и общие константы — в common/protocol.h.
 */

#include "commands.h"

#include <stdint.h>

#include "app_config.h"
#include "battery.h"
#include "i2c.h"
#include "meta.h"
#include "protocol.h"
#include "system.h"

/* ========================================================================
 * Обработчики команд батареи
 * ======================================================================== */

/*
 * Команда 0x01 — GET_INFO.
 * Единый формат с загрузчиком: по нему утилита прошивки определяет,
 * кто отвечает на шине, и проверяет подтверждение запуска.
 */
static void handle_get_info(void)
{
    MetaConfig m;
    bool have_meta = meta_read_current(&m, NULL);

    uint8_t out[INFO_PAYLOAD_LEN];

    out[0] = DEVICE_TYPE_APP;
    out[1] = PROTO_VER;
    proto_put_le32(&out[2], FW_VERSION);
    proto_put_le32(&out[6], have_meta ? m.img_size : 0U);
    out[10] = (have_meta && (m.flags & META_FLAG_PENDING))
                  ? INFO_FLAG_PENDING : 0U;
    out[11] = BAT_NUM_CELLS;

    i2c_set_response(ST_OK, out, sizeof(out));
}

/*
 * Команда 0x10 — GET_BATTERY_DATA.
 * Возвращает все данные батареи одним пакетом.
 */
static void handle_get_battery_data(void)
{
    const BatteryData *bat = battery_get_data();
    uint8_t out[14];

    proto_put_le16(&out[0], bat->voltage_mv);
    proto_put_le16(&out[2], (uint16_t)bat->current_ma);
    proto_put_le16(&out[4], (uint16_t)bat->temperature_c10);
    out[6] = bat->soc_percent;
    out[7] = bat->status;
    proto_put_le16(&out[8],  bat->cell_mv[0]);
    proto_put_le16(&out[10], bat->cell_mv[1]);
    proto_put_le16(&out[12], bat->cell_mv[2]);

    i2c_set_response(ST_OK, out, sizeof(out));
}

/*
 * Команда 0x11 — GET_CELL_VOLTAGES.
 */
static void handle_get_cell_voltages(void)
{
    const BatteryData *bat = battery_get_data();
    uint8_t out[6];

    proto_put_le16(&out[0], bat->cell_mv[0]);
    proto_put_le16(&out[2], bat->cell_mv[1]);
    proto_put_le16(&out[4], bat->cell_mv[2]);

    i2c_set_response(ST_OK, out, sizeof(out));
}

/*
 * Команда 0x12 — GET_TEMPERATURE.
 */
static void handle_get_temperature(void)
{
    const BatteryData *bat = battery_get_data();
    uint8_t out[2];

    proto_put_le16(&out[0], (uint16_t)bat->temperature_c10);

    i2c_set_response(ST_OK, out, sizeof(out));
}

/* ========================================================================
 * Служебные команды
 * ======================================================================== */

/* Небольшая задержка, чтобы подготовленный ответ успел уйти на шину
 * до сброса (мастер читает ответ сразу после записи команды). */
static void drain_delay(void)
{
    for (volatile uint32_t i = 0; i < 200000UL; i++) {
        ;
    }
}

/*
 * Команда 0x08 — RESET.
 * Аппаратный сброс контроллера.
 */
static void handle_reset(void)
{
    i2c_set_status(ST_OK);
    drain_delay();
    system_reset();
}

/*
 * Команда 0x0E — ENTER_UPDATE_MODE.
 *
 *   1. Отправляем подтверждение по I2C.
 *   2. Ждём, пока ответ уйдёт на шину.
 *   3. Пишем магическое число в зарезервированную область RAM.
 *   4. NVIC_SystemReset(); загрузчик увидит флаг и останется в режиме OTA.
 */
static void handle_enter_update_mode(void)
{
    i2c_set_status(ST_OK);
    drain_delay();

    volatile uint32_t *flag = (volatile uint32_t *)UPDATE_FLAG_ADDR;
    *flag = UPDATE_MAGIC;

    system_reset();
}

/* ========================================================================
 * Основной обработчик I2C запросов
 * ======================================================================== */

void app_process_i2c_request(void)
{
    uint16_t frame_len;
    uint8_t  cmd;

    if (!protocol_parse_request((const uint8_t *)rx_buf, rx_count, I2C_RX_BUF_SIZE,
                                &frame_len, &cmd, NULL)) {
        (void)frame_len;
        i2c_set_status(ST_ERR_LEN);
        return;
    }

    switch (cmd) {
        case CMD_GET_INFO:
            handle_get_info();
            break;

        case BAT_CMD_GET_DATA:
            battery_update(); /* обновить данные перед отправкой */
            handle_get_battery_data();
            break;

        case BAT_CMD_GET_CELL_VOLT:
            handle_get_cell_voltages();
            break;

        case BAT_CMD_GET_TEMP:
            handle_get_temperature();
            break;

        case CMD_RESET:
            /* Функция не возвращает управление — будет сброс */
            handle_reset();
            break;

        case CMD_ENTER_UPDATE:
            /* Функция не возвращает управление — будет сброс */
            handle_enter_update_mode();
            break;

        default:
            i2c_set_status(ST_ERR_CMD);
            break;
    }
}