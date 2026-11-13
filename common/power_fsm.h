/* common/power_fsm.h
 *
 * Конечный автомат кнопки и питания EC. Чистая логика: не трогает
 * регистры и пины, поэтому полностью тестируется на хосте
 * (tests/power_fsm_test.c).
 *
 * Входы: время (tick_ms), уровень кнопки, PWR_GOOD, состояние хоста из
 * SET_POWER. Выходы: упорядоченный список действий и новое состояние.
 */

#ifndef POWER_FSM_H
#define POWER_FSM_H

#include <stdint.h>
#include <stdbool.h>

#include "protocol.h"

#define POWER_FSM_MAX_ACTIONS 8U

typedef enum
{
    POWER_ACT_WAKE_HIGHZ = 1, /* отпустить HOST_WAKE (High-Z) */
    POWER_ACT_SET_PWR_EN,     /* arg: 0/1 */
    POWER_ACT_PULSE_WAKE,     /* импульс HOST_WAKE 150 мс */
    POWER_ACT_SET_REQUEST,    /* arg: POWER_REQ_* */
    POWER_ACT_SET_LED,        /* arg: power_led_pattern_t */
    POWER_ACT_ENTER_STOP      /* разрешён вход в Stop */
} power_action_type_t;

typedef enum
{
    POWER_LED_OFF = 0,
    POWER_LED_BOOT,
    POWER_LED_AWAKE,
    POWER_LED_SUSPENDED,
    POWER_LED_ERROR,
    POWER_LED_STOP
} power_led_pattern_t;

typedef struct
{
    uint8_t type;
    uint8_t arg;
} power_action_t;

typedef struct
{
    uint8_t count;
    power_action_t item[POWER_FSM_MAX_ACTIONS];
} power_actions_t;

typedef struct
{
    host_state_t state;

    uint8_t power_request;
    uint8_t last_event;
    uint8_t event_count;
    uint8_t reset_cause;
    uint8_t last_wake_src;
    uint8_t wake_count;

    bool pwr_en;
    bool pwr_good;
    bool host_state_valid; /* flags.b2 */
    bool link_lost;        /* flags.b3 */
    bool boot_fail;        /* flags.b4 */
    bool lse_ok;           /* flags.b5 */

    uint32_t state_since;
    uint32_t last_activity;
    uint32_t request_since;
    uint32_t boot_since;
    uint32_t boot_off_until;
    bool boot_good_seen;

    uint8_t boot_attempts;

    bool request_acked;
    bool stop_armed;

    /* Кнопка */
    bool btn_raw;
    bool btn_pressed;
    bool second_press;
    bool long_fired;
    bool very_long_fired;
    bool pending_short;
    uint32_t btn_change_ms;
    uint32_t btn_press_ms;
    uint32_t pending_short_ms;

    power_led_pattern_t led;
} power_fsm_t;

/* Инициализация после сброса EC.
 * bkp_* — состояние из backup-регистров (valid=false, если их нет).
 * wake_src/wake_count — из HAL (пробуждение из Stop до старта). */
void power_fsm_init(power_fsm_t *fsm, uint32_t now, bool pwr_good, bool bkp_valid, bool bkp_pwr_en,
                    uint8_t bkp_host_state, uint8_t bkp_boot_attempts, uint8_t reset_cause,
                    bool lse_ok, uint8_t wake_src, uint8_t wake_count, power_actions_t *out);

/* Периодический вызов из главного цикла. */
void power_fsm_tick(power_fsm_t *fsm, uint32_t now, bool button_level, bool pwr_good,
                    power_actions_t *out);

/* Любой валидный кадр от хоста. */
void power_fsm_host_frame(power_fsm_t *fsm, uint32_t now);

/* CMD_SET_POWER: состояние хоста и ack_flags. */
void power_fsm_set_host_state(power_fsm_t *fsm, uint32_t now, uint8_t host_state, uint8_t ack_flags,
                              power_actions_t *out);

/* Пробуждение из Stop (вызывает приложение после power_enter_stop). */
void power_fsm_on_wake(power_fsm_t *fsm, uint32_t now, uint8_t wake_src);

/* Разрешён ли вход в Stop прямо сейчас. */
bool power_fsm_can_stop(const power_fsm_t *fsm, uint32_t now);

/* Заполнить 8 байт ответа CMD_GET_POWER. */
void power_fsm_get_status(const power_fsm_t *fsm, uint8_t out[POWER_PAYLOAD_LEN]);

const char *power_fsm_state_name(host_state_t state);

#endif /* POWER_FSM_H */