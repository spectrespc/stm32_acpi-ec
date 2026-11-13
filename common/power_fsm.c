/* common/power_fsm.c
 *
 * Чистый конечный автомат кнопки и питания. Никаких регистров: все
 * действия возвращаются приложению упорядоченным списком, поэтому
 * логика полностью покрывается хостовыми тестами.
 */

#include <string.h>

#include "power_fsm.h"
#include "power_config.h"

/* ========================================================================
 * Вспомогательные функции
 * ======================================================================== */

static void actions_reset(power_actions_t *out)
{
    out->count = 0;
}

static void act(power_actions_t *out, uint8_t type, uint8_t arg)
{
    if (out->count < POWER_FSM_MAX_ACTIONS)
    {
        out->item[out->count].type = type;
        out->item[out->count].arg = arg;
        out->count++;
    }
}

static void set_led(power_fsm_t *fsm, power_actions_t *out, power_led_pattern_t pattern)
{
    if (fsm->led != pattern)
    {
        fsm->led = pattern;
        act(out, POWER_ACT_SET_LED, (uint8_t)pattern);
    }
}

static void set_state(power_fsm_t *fsm, host_state_t state, uint32_t now)
{
    fsm->state = state;
    fsm->state_since = now;
}

static void clear_request(power_fsm_t *fsm, power_actions_t *out)
{
    if (fsm->power_request != POWER_REQ_NONE)
    {
        fsm->power_request = POWER_REQ_NONE;
        fsm->request_acked = false;
        act(out, POWER_ACT_SET_REQUEST, POWER_REQ_NONE);
    }
}

static void power_off(power_fsm_t *fsm, uint32_t now, power_actions_t *out)
{
    if (fsm->pwr_en)
    {
        /* Порядок важен: сначала HOST_WAKE в High-Z, затем PWR_EN=0. */
        act(out, POWER_ACT_WAKE_HIGHZ, 0);
        act(out, POWER_ACT_SET_PWR_EN, 0);
        fsm->pwr_en = false;
    }

    clear_request(fsm, out);

    set_state(fsm, HOST_STATE_OFF, now);
    fsm->last_activity = now;
    fsm->host_state_valid = false;
    fsm->link_lost = false;
    fsm->boot_fail = false;
    fsm->boot_off_until = 0;
    fsm->boot_attempts = 0;
    fsm->boot_good_seen = false;
    fsm->stop_armed = false;

    set_led(fsm, out, POWER_LED_OFF);
}

static void power_on(power_fsm_t *fsm, uint32_t now, power_actions_t *out)
{
    if (!fsm->pwr_en)
    {
        act(out, POWER_ACT_SET_PWR_EN, 1);
        fsm->pwr_en = true;
    }

    clear_request(fsm, out);

    set_state(fsm, HOST_STATE_BOOTING, now);
    fsm->last_activity = now;
    fsm->boot_since = now;
    fsm->boot_off_until = 0;
    fsm->boot_good_seen = false;
    fsm->boot_attempts = 0;
    fsm->host_state_valid = false;
    fsm->link_lost = false;
    fsm->boot_fail = false;
    fsm->stop_armed = false;

    set_led(fsm, out, POWER_LED_BOOT);
}

static void begin_request(power_fsm_t *fsm, uint8_t request, host_state_t state, uint32_t now,
                          power_actions_t *out)
{
    fsm->power_request = request;
    fsm->request_since = now;
    fsm->request_acked = false;
    fsm->last_activity = now;
    fsm->stop_armed = false;

    set_state(fsm, state, now);
    act(out, POWER_ACT_SET_REQUEST, request);
}

static void reboot_cycle(power_fsm_t *fsm, uint32_t now, power_actions_t *out)
{
    clear_request(fsm, out);

    if (fsm->pwr_en)
    {
        act(out, POWER_ACT_WAKE_HIGHZ, 0);
        act(out, POWER_ACT_SET_PWR_EN, 0);
        fsm->pwr_en = false;
    }

    fsm->boot_off_until = now + POWER_TOFF_MS;
    fsm->boot_good_seen = false;
    fsm->boot_attempts = 0;
    fsm->host_state_valid = false;
    fsm->link_lost = false;
    fsm->boot_fail = false;
    fsm->stop_armed = false;
    fsm->last_activity = now;

    set_state(fsm, HOST_STATE_BOOTING, now);
    set_led(fsm, out, POWER_LED_BOOT);
}

static void boot_fail_cycle(power_fsm_t *fsm, uint32_t now, power_actions_t *out)
{
    fsm->boot_attempts++;

    if (fsm->boot_attempts >= POWER_MAX_BOOT_ATTEMPTS)
    {
        fsm->boot_fail = true;
        fsm->last_activity = now;
        set_state(fsm, HOST_STATE_ERROR, now);
        set_led(fsm, out, POWER_LED_ERROR);
        return;
    }

    if (fsm->pwr_en)
    {
        act(out, POWER_ACT_WAKE_HIGHZ, 0);
        act(out, POWER_ACT_SET_PWR_EN, 0);
        fsm->pwr_en = false;
    }

    fsm->boot_off_until = now + POWER_TOFF_MS;
    fsm->boot_good_seen = false;
    fsm->host_state_valid = false;
    fsm->last_activity = now;
    fsm->stop_armed = false;

    set_state(fsm, HOST_STATE_BOOTING, now);
    set_led(fsm, out, POWER_LED_BOOT);
}

/* ========================================================================
 * Кнопка
 * ======================================================================== */

static void handle_event(power_fsm_t *fsm, uint8_t event, uint32_t now, power_actions_t *out)
{
    fsm->event_count++;
    fsm->last_event = event;
    fsm->last_activity = now;
    fsm->stop_armed = false;

    switch (fsm->state)
    {
    case HOST_STATE_OFF:
        if (event == POWER_EVENT_SHORT)
        {
            power_on(fsm, now, out);
        }
        else if (event == POWER_EVENT_LONG)
        {
            /* Жест распознан, но в OFF ничего не делает (см. §6 плана). */
            fsm->last_event = POWER_EVENT_LONG_IN_OFF;
        }
        break;

    case HOST_STATE_AWAKE:
        if (event == POWER_EVENT_SHORT)
        {
            begin_request(fsm, POWER_REQ_SLEEP, HOST_STATE_SUSPENDING, now, out);
        }
        else if (event == POWER_EVENT_LONG)
        {
            begin_request(fsm, POWER_REQ_SHUTDOWN, HOST_STATE_SHUTTING_DOWN, now, out);
        }
        else if (event == POWER_EVENT_DOUBLE)
        {
            begin_request(fsm, POWER_REQ_REBOOT, HOST_STATE_REBOOTING, now, out);
        }
        else
        {
            power_off(fsm, now, out);
        }
        break;

    case HOST_STATE_SUSPENDED:
        if (event == POWER_EVENT_SHORT)
        {
            act(out, POWER_ACT_PULSE_WAKE, 0);
            fsm->link_lost = false;
            set_state(fsm, HOST_STATE_AWAKE, now);
            set_led(fsm, out, POWER_LED_AWAKE);
        }
        else if (event == POWER_EVENT_LONG)
        {
            begin_request(fsm, POWER_REQ_SHUTDOWN, HOST_STATE_SHUTTING_DOWN, now, out);
        }
        else if (event == POWER_EVENT_DOUBLE)
        {
            begin_request(fsm, POWER_REQ_REBOOT, HOST_STATE_REBOOTING, now, out);
        }
        else
        {
            power_off(fsm, now, out);
        }
        break;

    case HOST_STATE_BOOTING:
        if (event == POWER_EVENT_LONG || event == POWER_EVENT_VERY_LONG)
        {
            power_off(fsm, now, out);
        }
        break;

    case HOST_STATE_ERROR:
        if (event == POWER_EVENT_DOUBLE)
        {
            if (!fsm->pwr_en)
            {
                act(out, POWER_ACT_SET_PWR_EN, 1);
                fsm->pwr_en = true;
            }

            fsm->boot_fail = false;
            fsm->boot_attempts = 0;
            fsm->boot_since = now;
            fsm->boot_good_seen = false;
            fsm->boot_off_until = 0;
            fsm->host_state_valid = false;
            set_state(fsm, HOST_STATE_BOOTING, now);
            set_led(fsm, out, POWER_LED_BOOT);
        }
        else
        {
            power_off(fsm, now, out);
        }
        break;

    case HOST_STATE_SUSPENDING:
    case HOST_STATE_SHUTTING_DOWN:
    case HOST_STATE_REBOOTING:
    default:
        if (event == POWER_EVENT_VERY_LONG)
        {
            power_off(fsm, now, out);
        }
        break;
    }
}

static void button_step(power_fsm_t *fsm, uint32_t now, bool level, power_actions_t *out)
{
    if (level != fsm->btn_raw)
    {
        fsm->btn_raw = level;
        fsm->btn_change_ms = now;
    }

    bool stable = (uint32_t)(now - fsm->btn_change_ms) >= POWER_DEBOUNCE_MS;

    if (stable && (level != fsm->btn_pressed))
    {
        if (level)
        {
            fsm->btn_pressed = true;
            fsm->btn_press_ms = now;
            fsm->long_fired = false;
            fsm->very_long_fired = false;
            fsm->last_activity = now;
            fsm->stop_armed = false;

            if (fsm->pending_short &&
                (uint32_t)(now - fsm->pending_short_ms) <= POWER_DOUBLE_WINDOW_MS)
            {
                fsm->second_press = true;
                fsm->pending_short = false;
            }
            else
            {
                fsm->second_press = false;
            }
        }
        else
        {
            fsm->btn_pressed = false;

            uint32_t duration = (uint32_t)(now - fsm->btn_press_ms);

            if (!fsm->long_fired && !fsm->very_long_fired && duration < POWER_SHORT_MAX_MS)
            {
                if (fsm->second_press)
                {
                    fsm->second_press = false;
                    handle_event(fsm, POWER_EVENT_DOUBLE, now, out);
                }
                else
                {
                    fsm->pending_short = true;
                    fsm->pending_short_ms = now;
                }
            }
        }
    }

    if (fsm->btn_pressed)
    {
        uint32_t duration = (uint32_t)(now - fsm->btn_press_ms);

        if (!fsm->very_long_fired && duration >= POWER_VERY_LONG_MS)
        {
            fsm->very_long_fired = true;
            handle_event(fsm, POWER_EVENT_VERY_LONG, now, out);
        }
        else if (!fsm->long_fired && duration >= POWER_LONG_MS)
        {
            fsm->long_fired = true;
            handle_event(fsm, POWER_EVENT_LONG, now, out);
        }
    }
    else if (fsm->pending_short && (uint32_t)(now - fsm->pending_short_ms) > POWER_DOUBLE_WINDOW_MS)
    {
        fsm->pending_short = false;
        handle_event(fsm, POWER_EVENT_SHORT, now, out);
    }
}

/* ========================================================================
 * API
 * ======================================================================== */

void power_fsm_init(power_fsm_t *fsm, uint32_t now, bool pwr_good, bool bkp_valid, bool bkp_pwr_en,
                    uint8_t bkp_host_state, uint8_t bkp_boot_attempts, uint8_t reset_cause,
                    bool lse_ok, uint8_t wake_src, uint8_t wake_count, power_actions_t *out)
{
    memset(fsm, 0, sizeof(*fsm));

    actions_reset(out);

    fsm->reset_cause = reset_cause;
    fsm->lse_ok = lse_ok;
    fsm->last_wake_src = wake_src;
    fsm->wake_count = wake_count;
    fsm->pwr_good = pwr_good;
    fsm->state_since = now;
    fsm->last_activity = now;
    fsm->btn_raw = false;
    fsm->btn_change_ms = now;

    if (!pwr_good && !POWER_ON_RESTORE)
    {
        /* Питание хоста отсутствует: FSM обязан остаться в OFF, даже
         * если backup сохранил pwr_en=1 (сброс EC не должен включать
         * обесточенный хост). POWER_ON_RESTORE — явный opt-in. */
        fsm->pwr_en = false;
        fsm->state = HOST_STATE_OFF;
    }
    else if (bkp_valid && bkp_pwr_en)
    {
        /* Ресет EC (OTA, CMD_RESET) не должен выключать хост: желаемое
         * состояние берём из backup-регистра. */
        fsm->pwr_en = true;
        fsm->boot_attempts = bkp_boot_attempts;

        if (bkp_host_state < (uint8_t)HOST_STATE_ERROR && bkp_host_state != HOST_STATE_OFF)
        {
            fsm->state = (host_state_t)bkp_host_state;
        }
        else
        {
            fsm->state = HOST_STATE_BOOTING;
        }

        if (fsm->state == HOST_STATE_BOOTING)
        {
            fsm->boot_since = now;
            fsm->boot_good_seen = pwr_good;
        }
        else
        {
            fsm->request_since = now;
        }
    }
    else
    {
        /* PWR_GOOD=1 (или явный POWER_ON_RESTORE): хост включён, но
         * backup пуст — не выключаем его молча. */
        fsm->pwr_en = true;
        fsm->state = HOST_STATE_BOOTING;
        fsm->boot_since = now;
        fsm->boot_good_seen = pwr_good;
    }

    fsm->led = (power_led_pattern_t)0xFFU; /* принудительно выдать паттерн */

    if (!fsm->pwr_en)
    {
        act(out, POWER_ACT_WAKE_HIGHZ, 0);
    }

    act(out, POWER_ACT_SET_PWR_EN, fsm->pwr_en ? 1U : 0U);
    act(out, POWER_ACT_SET_REQUEST, fsm->power_request);

    switch (fsm->state)
    {
    case HOST_STATE_OFF:
        set_led(fsm, out, POWER_LED_OFF);
        break;
    case HOST_STATE_BOOTING:
        set_led(fsm, out, POWER_LED_BOOT);
        break;
    case HOST_STATE_AWAKE:
        set_led(fsm, out, POWER_LED_AWAKE);
        break;
    case HOST_STATE_SUSPENDED:
        set_led(fsm, out, POWER_LED_SUSPENDED);
        break;
    case HOST_STATE_ERROR:
        fsm->boot_fail = true;
        set_led(fsm, out, POWER_LED_ERROR);
        break;
    default:
        set_led(fsm, out, POWER_LED_AWAKE);
        break;
    }
}

void power_fsm_tick(power_fsm_t *fsm, uint32_t now, bool button_level, bool pwr_good,
                    power_actions_t *out)
{
    actions_reset(out);

    fsm->pwr_good = pwr_good;

    button_step(fsm, now, button_level, out);

    /* --- Таймеры запросов -------------------------------------------------- */

    if (fsm->state == HOST_STATE_SUSPENDING)
    {
        if (!fsm->request_acked && (uint32_t)(now - fsm->request_since) >= POWER_REQUEST_TIMEOUT_MS)
        {
            /* SLEEP без ACK: остаёмся AWAKE, питание не трогаем. */
            clear_request(fsm, out);
            fsm->last_activity = now;
            set_state(fsm, HOST_STATE_AWAKE, now);
            set_led(fsm, out, POWER_LED_AWAKE);
        }
    }
    else if (fsm->state == HOST_STATE_SHUTTING_DOWN)
    {
        if ((uint32_t)(now - fsm->request_since) >= POWER_REQUEST_TIMEOUT_MS)
        {
            power_off(fsm, now, out);
        }
    }
    else if (fsm->state == HOST_STATE_REBOOTING)
    {
        if ((uint32_t)(now - fsm->request_since) >= POWER_REQUEST_TIMEOUT_MS)
        {
            reboot_cycle(fsm, now, out);
        }
    }

    /* --- BOOTING ----------------------------------------------------------- */

    if (fsm->state == HOST_STATE_BOOTING)
    {
        if (fsm->boot_off_until != 0U)
        {
            if ((int32_t)(now - fsm->boot_off_until) >= 0)
            {
                act(out, POWER_ACT_SET_PWR_EN, 1);
                fsm->pwr_en = true;
                fsm->boot_off_until = 0;
                fsm->boot_since = now;
                fsm->boot_good_seen = false;
                fsm->last_activity = now;
                set_led(fsm, out, POWER_LED_BOOT);
            }
        }
        else if (fsm->pwr_en)
        {
            if (pwr_good)
            {
                fsm->boot_good_seen = true;
            }

            if (!fsm->boot_good_seen)
            {
                if ((uint32_t)(now - fsm->boot_since) >= POWER_PWR_GOOD_TIMEOUT_MS)
                {
                    boot_fail_cycle(fsm, now, out);
                }
            }
            else if ((uint32_t)(now - fsm->boot_since) >= POWER_BOOT_TIMEOUT_MS)
            {
                boot_fail_cycle(fsm, now, out);
            }
        }
    }

    /* --- Тишина хоста → SUSPENDED ----------------------------------------- */

    bool gesture_active = fsm->btn_pressed || fsm->pending_short || fsm->second_press;

    if (!gesture_active &&
        (fsm->state == HOST_STATE_AWAKE || fsm->state == HOST_STATE_SUSPENDING) &&
        (fsm->power_request == POWER_REQ_NONE || fsm->request_acked) &&
        (uint32_t)(now - fsm->last_activity) >= POWER_HOST_SILENCE_MS)
    {
        fsm->link_lost = true;
        clear_request(fsm, out);
        set_state(fsm, HOST_STATE_SUSPENDED, now);
        set_led(fsm, out, POWER_LED_SUSPENDED);
    }

    /* --- LED по состоянию -------------------------------------------------- */

    switch (fsm->state)
    {
    case HOST_STATE_OFF:
        set_led(fsm, out, POWER_LED_OFF);
        break;
    case HOST_STATE_BOOTING:
        set_led(fsm, out, POWER_LED_BOOT);
        break;
    case HOST_STATE_AWAKE:
    case HOST_STATE_SUSPENDING:
        set_led(fsm, out, POWER_LED_AWAKE);
        break;
    case HOST_STATE_SUSPENDED:
        set_led(fsm, out, POWER_LED_SUSPENDED);
        break;
    case HOST_STATE_ERROR:
        set_led(fsm, out, POWER_LED_ERROR);
        break;
    default:
        set_led(fsm, out, POWER_LED_BOOT);
        break;
    }

    /* --- Вход в Stop ------------------------------------------------------- */

    if (power_fsm_can_stop(fsm, now) && !fsm->stop_armed)
    {
        fsm->stop_armed = true;
        act(out, POWER_ACT_ENTER_STOP, 0);
    }
}

void power_fsm_host_frame(power_fsm_t *fsm, uint32_t now)
{
    fsm->last_activity = now;
    fsm->link_lost = false;
    fsm->stop_armed = false;

    if (fsm->state == HOST_STATE_BOOTING)
    {
        fsm->boot_attempts = 0;
        fsm->boot_fail = false;
        set_state(fsm, HOST_STATE_AWAKE, now);
    }
    else if (fsm->state == HOST_STATE_SUSPENDED)
    {
        set_state(fsm, HOST_STATE_AWAKE, now);
    }
}

void power_fsm_set_host_state(power_fsm_t *fsm, uint32_t now, uint8_t host_state, uint8_t ack_flags,
                              power_actions_t *out)
{
    actions_reset(out);

    fsm->host_state_valid = true;
    fsm->last_activity = now;
    fsm->link_lost = false;
    fsm->stop_armed = false;

    if ((ack_flags & POWER_ACK_REQUEST) && fsm->power_request != POWER_REQ_NONE)
    {
        fsm->request_acked = true;
    }

    switch (host_state)
    {
    case HOST_STATE_OFF:
        if (!fsm->pwr_good)
        {
            power_off(fsm, now, out);
        }
        break;

    case HOST_STATE_BOOTING:
        fsm->boot_fail = false;
        fsm->boot_attempts = 0;
        fsm->boot_since = now;
        fsm->boot_good_seen = fsm->pwr_good;
        set_state(fsm, HOST_STATE_BOOTING, now);
        set_led(fsm, out, POWER_LED_BOOT);
        break;

    case HOST_STATE_AWAKE:
        if (fsm->pwr_en)
        {
            clear_request(fsm, out);
            fsm->boot_attempts = 0;
            fsm->boot_fail = false;
            set_state(fsm, HOST_STATE_AWAKE, now);
            set_led(fsm, out, POWER_LED_AWAKE);
        }
        break;

    case HOST_STATE_SUSPENDING:
        if (fsm->power_request == POWER_REQ_SLEEP)
        {
            fsm->request_acked = true;
        }

        set_state(fsm, HOST_STATE_SUSPENDING, now);
        break;

    case HOST_STATE_SUSPENDED:
        fsm->request_acked = true;
        clear_request(fsm, out);
        set_state(fsm, HOST_STATE_SUSPENDED, now);
        set_led(fsm, out, POWER_LED_SUSPENDED);
        break;

    case HOST_STATE_SHUTTING_DOWN:
        fsm->request_acked = true;
        set_state(fsm, HOST_STATE_SHUTTING_DOWN, now);
        break;

    case HOST_STATE_REBOOTING:
        fsm->request_acked = true;
        set_state(fsm, HOST_STATE_REBOOTING, now);
        break;

    case HOST_STATE_ERROR:
        fsm->boot_fail = true;
        set_state(fsm, HOST_STATE_ERROR, now);
        set_led(fsm, out, POWER_LED_ERROR);
        break;

    default:
        break;
    }
}

void power_fsm_on_wake(power_fsm_t *fsm, uint32_t now, uint8_t wake_src)
{
    if (wake_src != POWER_WAKE_NONE)
    {
        fsm->last_wake_src = wake_src;
        fsm->wake_count++;
    }

    fsm->last_activity = now;
    fsm->stop_armed = false;

    if (wake_src == POWER_WAKE_I2C && fsm->state == HOST_STATE_SUSPENDED)
    {
        fsm->link_lost = false;
        set_state(fsm, HOST_STATE_AWAKE, now);
    }
}

bool power_fsm_can_stop(const power_fsm_t *fsm, uint32_t now)
{
    if (fsm->state != HOST_STATE_OFF && fsm->state != HOST_STATE_AWAKE &&
        fsm->state != HOST_STATE_SUSPENDED)
    {
        return false;
    }

    if (fsm->power_request != POWER_REQ_NONE)
    {
        return false;
    }

    if (fsm->boot_off_until != 0U)
    {
        return false;
    }

    if (fsm->btn_pressed || fsm->pending_short || fsm->second_press)
    {
        return false;
    }

    return (uint32_t)(now - fsm->last_activity) >= POWER_IDLE_STOP_MS;
}

void power_fsm_get_status(const power_fsm_t *fsm, uint8_t out[POWER_PAYLOAD_LEN])
{
    uint8_t flags = 0;

    if (fsm->pwr_en)
    {
        flags |= POWER_FLAG_PWR_EN;
    }

    if (fsm->pwr_good)
    {
        flags |= POWER_FLAG_PWR_GOOD;
    }

    if (fsm->host_state_valid)
    {
        flags |= POWER_FLAG_HOST_STATE;
    }

    if (fsm->link_lost)
    {
        flags |= POWER_FLAG_LINK_LOST;
    }

    if (fsm->boot_fail)
    {
        flags |= POWER_FLAG_BOOT_FAIL;
    }

    if (fsm->lse_ok)
    {
        flags |= POWER_FLAG_LSE_OK;
    }

    out[0] = (uint8_t)fsm->state;
    out[1] = flags;
    out[2] = fsm->power_request;
    out[3] = fsm->last_event;
    out[4] = fsm->event_count;
    out[5] = fsm->reset_cause;
    out[6] = fsm->last_wake_src;
    out[7] = fsm->wake_count;
}

const char *power_fsm_state_name(host_state_t state)
{
    switch (state)
    {
    case HOST_STATE_OFF:
        return "OFF";
    case HOST_STATE_BOOTING:
        return "BOOTING";
    case HOST_STATE_AWAKE:
        return "AWAKE";
    case HOST_STATE_SUSPENDING:
        return "SUSPENDING";
    case HOST_STATE_SUSPENDED:
        return "SUSPENDED";
    case HOST_STATE_SHUTTING_DOWN:
        return "SHUTTING_DOWN";
    case HOST_STATE_REBOOTING:
        return "REBOOTING";
    case HOST_STATE_ERROR:
        return "ERROR";
    default:
        return "?";
    }
}