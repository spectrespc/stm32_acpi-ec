/* app/src/power_task.c
 *
 * Прикладной мост питания:
 *   - читает backup-регистры и инициализирует FSM;
 *   - каждый тик отдаёт FSM кнопку/PWR_GOOD и исполняет действия;
 *   - показывает состояние LED-кодами (docs/architecture.md §LED);
 *   - входит в Stop по idle-таймауту и восстанавливает контекст.
 */

#include "power_task.h"

#include "board.h"
#include "i2c.h"
#include "power.h"
#include "power_config.h"
#include "power_fsm.h"

/* Backup-регистры: BKP0 — магия/флаги, BKP1 — boot attempts + state. */
#define BKP_MAGIC_LOW  0x05U
#define BKP_MAGIC_HIGH 0xA0U
#define BKP0_PWR_EN    0x10U

typedef struct
{
    uint16_t on_ms;
    uint16_t off_ms;
} led_step_t;

static const led_step_t LED_OFF_SEQ[] = {{100, 1900}};
static const led_step_t LED_BOOT_SEQ[] = {{100, 100}, {100, 200}};
static const led_step_t LED_AWAKE_SEQ[] = {{500, 500}};
static const led_step_t LED_SUSPENDED_SEQ[] = {{2000, 2000}};
static const led_step_t LED_ERROR_SEQ[] = {{100, 100}};

typedef struct
{
    const led_step_t *steps;
    uint8_t count;
} led_sequence_t;

/* Отладочные зеркала состояния: доступны в GDB как p g_host_state и т.п.
 * Основное состояние живёт в fsm; эти переменные обновляются каждый тик. */
volatile uint8_t g_host_state;
volatile uint8_t g_last_event;
volatile uint8_t g_last_wake_src;
volatile uint8_t g_wake_count;

static power_fsm_t fsm;
static bool initialized;
static bool last_i2c_enabled;
static uint32_t backup_cache;

/* LED-планировщик */
static uint8_t led_seq_pos;
static uint8_t led_level;
static uint32_t led_next;
static uint8_t event_flashes;

static const led_sequence_t *led_sequence_for(power_led_pattern_t pattern)
{
    static const led_sequence_t off = {LED_OFF_SEQ, 1};
    static const led_sequence_t boot = {LED_BOOT_SEQ, 2};
    static const led_sequence_t awake = {LED_AWAKE_SEQ, 1};
    static const led_sequence_t suspended = {LED_SUSPENDED_SEQ, 1};
    static const led_sequence_t error = {LED_ERROR_SEQ, 1};
    static const led_sequence_t stop = {LED_OFF_SEQ, 1};

    switch (pattern)
    {
    case POWER_LED_BOOT:
        return &boot;
    case POWER_LED_AWAKE:
        return &awake;
    case POWER_LED_SUSPENDED:
        return &suspended;
    case POWER_LED_ERROR:
        return &error;
    case POWER_LED_STOP:
        return &stop;
    case POWER_LED_OFF:
    default:
        return &off;
    }
}

static void led_service(uint32_t now)
{
    if ((int32_t)(now - led_next) < 0)
    {
        return;
    }

    /* Сначала доигрываем вспышки последнего жеста: N = код события. */
    if (event_flashes > 0U)
    {
        if (led_level != 0U)
        {
            led_off();
            led_level = 0;
            led_next = now + 150U;
            event_flashes--;
        }
        else
        {
            led_on();
            led_level = 1;
            led_next = now + 100U;
        }

        return;
    }

    const led_sequence_t *seq = led_sequence_for(fsm.led);
    const led_step_t *step = &seq->steps[led_seq_pos % seq->count];

    if (led_level != 0U)
    {
        led_off();
        led_level = 0;
        led_next = now + step->off_ms;
        led_seq_pos = (uint8_t)((led_seq_pos + 1U) % seq->count);
    }
    else
    {
        led_on();
        led_level = 1;
        led_next = now + step->on_ms;
    }
}

static void led_reset(void)
{
    led_off();
    led_level = 0;
    led_seq_pos = 0;
    led_next = millis();
}

static void led_note_event(void)
{
    uint8_t flashes = fsm.last_event;

    if (flashes == 0U)
    {
        return;
    }

    if (flashes == POWER_EVENT_LONG_IN_OFF)
    {
        flashes = 2U;
    }

    event_flashes = flashes;
    led_level = 0;
    led_next = millis();
}

static void backup_restore(void)
{
    uint8_t bkp0 = power_backup_read(0);
    uint8_t bkp1 = power_backup_read(1);
    backup_cache = ((uint32_t)bkp0 << 8) | bkp1;
}

static void backup_save(void)
{
    uint8_t bkp0 = (uint8_t)(BKP_MAGIC_HIGH | BKP_MAGIC_LOW);

    if (fsm.pwr_en)
    {
        bkp0 |= BKP0_PWR_EN;
    }

    uint8_t bkp1 = (uint8_t)((fsm.boot_attempts << 4) | ((uint8_t)fsm.state & 0x0FU));

    uint32_t packed = ((uint32_t)bkp0 << 8) | bkp1;

    if (packed != backup_cache)
    {
        power_backup_write(0, bkp0);
        power_backup_write(1, bkp1);
        backup_cache = packed;
    }
}

static void sync_i2c(void)
{
    bool want = fsm.pwr_en && fsm.pwr_good;

    if (want != last_i2c_enabled)
    {
        i2c_set_enabled(want);
        last_i2c_enabled = want;
    }
}

static void apply_actions(const power_actions_t *actions, bool *enter_stop)
{
    for (uint8_t i = 0; i < actions->count; i++)
    {
        const power_action_t *a = &actions->item[i];

        switch (a->type)
        {
        case POWER_ACT_WAKE_HIGHZ:
            power_set_host_wake(false);
            break;

        case POWER_ACT_SET_PWR_EN:
            power_set_pwr_en(a->arg != 0U);
            break;

        case POWER_ACT_PULSE_WAKE:
            power_pulse_host_wake();
            break;

        case POWER_ACT_SET_REQUEST:
            /* Запрос читается хостом из GET_POWER. */
            break;

        case POWER_ACT_SET_LED:
            led_reset();
            break;

        case POWER_ACT_ENTER_STOP:
            *enter_stop = true;
            break;

        default:
            break;
        }
    }
}

/* ========================================================================
 * API
 * ======================================================================== */

bool power_task_supported(void)
{
    return power_supported();
}

void power_task_init(void)
{
    if (!power_supported())
    {
        initialized = false;
        return;
    }

    power_init();

    backup_restore();

    uint8_t bkp0 = power_backup_read(0);
    bool valid = (bkp0 & 0x0FU) == BKP_MAGIC_LOW;

    uint8_t bkp1 = power_backup_read(1);
    bool bkp_pwr_en = (bkp0 & BKP0_PWR_EN) != 0U;
    uint8_t bkp_state = (uint8_t)(bkp1 & 0x0FU);
    uint8_t bkp_attempts = (uint8_t)((bkp1 >> 4) & 0x0FU);

    power_actions_t actions;
    uint32_t now = millis();

    power_fsm_init(&fsm, now, power_get_pwr_good(), valid, bkp_pwr_en, bkp_state, bkp_attempts,
                   power_reset_cause(), power_lse_ok(), power_last_wake_src(), power_wake_count(),
                   &actions);

    last_i2c_enabled = true; /* i2c_init() уже выполнен в main */
    bool enter_stop = false;

    g_host_state = (uint8_t)fsm.state;
    g_last_event = fsm.last_event;
    g_last_wake_src = fsm.last_wake_src;
    g_wake_count = fsm.wake_count;

    apply_actions(&actions, &enter_stop);
    sync_i2c();
    backup_save();
    led_reset();

    initialized = true;
}

void power_task_tick(void)
{
    if (!initialized)
    {
        return;
    }

    uint32_t now = millis();
    uint8_t events_before = fsm.last_event;

    power_actions_t actions;

    power_fsm_tick(&fsm, now, board_button_pressed(), power_get_pwr_good(), &actions);

    if (fsm.last_event != events_before)
    {
        led_note_event();
    }

    bool enter_stop = false;

    apply_actions(&actions, &enter_stop);
    sync_i2c();
    backup_save();

    g_host_state = (uint8_t)fsm.state;
    g_last_event = fsm.last_event;
    g_last_wake_src = fsm.last_wake_src;
    g_wake_count = fsm.wake_count;

    if (enter_stop)
    {
        led_off();
        led_level = 0;

        if (!power_enter_stop(0))
        {
            /* Событие пришло на границе Stop: не спали, обработаем
             * кнопку/кадр на следующей итерации. */
            fsm.stop_armed = false;
            fsm.last_activity = millis();
            led_reset();
            return;
        }

        power_fsm_on_wake(&fsm, millis(), power_last_wake_src());

        /* После Stop принудительно перерисовать LED и синхронизировать I2C. */
        fsm.led = (power_led_pattern_t)0xFFU;
        last_i2c_enabled = !(fsm.pwr_en && fsm.pwr_good);
        sync_i2c();
        led_reset();
        return;
    }

    led_service(now);
}

void power_task_host_frame(void)
{
    if (!initialized)
    {
        return;
    }

    power_fsm_host_frame(&fsm, millis());
}

void power_task_set_host_state(uint8_t host_state, uint8_t ack_flags)
{
    if (!initialized)
    {
        return;
    }

    power_actions_t actions;

    power_fsm_set_host_state(&fsm, millis(), host_state, ack_flags, &actions);

    bool enter_stop = false;

    apply_actions(&actions, &enter_stop);
    sync_i2c();
    backup_save();
}

void power_task_get_status(uint8_t out[POWER_PAYLOAD_LEN])
{
    if (!initialized)
    {
        for (uint8_t i = 0; i < POWER_PAYLOAD_LEN; i++)
        {
            out[i] = 0;
        }

        return;
    }

    power_fsm_get_status(&fsm, out);
}