/* tests/test_power_fsm.c
 *
 * Хостовые тесты конечного автомата питания: все жесты кнопки во всех
 * состояниях, graceful-таймауты, power-cycle, boot-fail, idle→Stop и
 * правила безопасности §6 плана.
 */

#include "unity.h"

#include "power.h"
#include "power_config.h"
#include "power_fsm.h"

static power_fsm_t fsm;
static power_actions_t act;

#define DEBOUNCE (POWER_DEBOUNCE_MS + 1U)

static bool has_action(const power_actions_t *a, uint8_t type, uint8_t arg)
{
    for (uint8_t i = 0; i < a->count; i++)
    {
        if (a->item[i].type == type && a->item[i].arg == arg)
        {
            return true;
        }
    }

    return false;
}

static int action_index(const power_actions_t *a, uint8_t type, uint8_t arg)
{
    for (uint8_t i = 0; i < a->count; i++)
    {
        if (a->item[i].type == type && a->item[i].arg == arg)
        {
            return (int)i;
        }
    }

    return -1;
}

static void init_off(void)
{
    power_fsm_init(&fsm, 1000U, false, false, false, 0, 0, RESET_CAUSE_POR, false, POWER_WAKE_RESET,
                   0, &act);
}

static void init_awake(void)
{
    /* Backup: pwr_en=1, состояние AWAKE. */
    power_fsm_init(&fsm, 1000U, true, true, true, HOST_STATE_AWAKE, 0, RESET_CAUSE_SFT, false,
                   POWER_WAKE_RESET, 0, &act);
}

/* Нажать и отпустить кнопку короче short-порога; событие short
 * срабатывает после окна двойного нажатия. Возвращает время события. */
static uint32_t do_short_press(uint32_t t)
{
    power_fsm_tick(&fsm, t, true, true, &act);
    power_fsm_tick(&fsm, t + DEBOUNCE, true, true, &act);
    power_fsm_tick(&fsm, t + DEBOUNCE + 100U, false, true, &act);
    uint32_t fire = t + DEBOUNCE + 100U + DEBOUNCE;
    power_fsm_tick(&fsm, fire, false, true, &act);
    uint32_t after = fire + POWER_DOUBLE_WINDOW_MS + 1U;
    power_fsm_tick(&fsm, after, false, true, &act);
    return after;
}

/* Удержание до LONG без отпускания. */
static void hold_long(uint32_t t)
{
    power_fsm_tick(&fsm, t, true, true, &act);
    power_fsm_tick(&fsm, t + DEBOUNCE, true, true, &act);
    power_fsm_tick(&fsm, t + DEBOUNCE + POWER_LONG_MS, true, true, &act);
}

static void hold_very_long(uint32_t t)
{
    power_fsm_tick(&fsm, t, true, true, &act);
    power_fsm_tick(&fsm, t + DEBOUNCE, true, true, &act);
    power_fsm_tick(&fsm, t + DEBOUNCE + POWER_VERY_LONG_MS, true, true, &act);
}

static void release_button(uint32_t t)
{
    power_fsm_tick(&fsm, t, false, true, &act);
    power_fsm_tick(&fsm, t + DEBOUNCE, false, true, &act);
}

/* ========================================================================
 * Инициализация
 * ======================================================================== */

static void test_init_off_when_no_power(void)
{
    init_off();

    TEST_ASSERT_EQUAL_INT(HOST_STATE_OFF, fsm.state);
    TEST_ASSERT_FALSE(fsm.pwr_en);
    TEST_ASSERT_TRUE(has_action(&act, POWER_ACT_SET_PWR_EN, 0));
    TEST_ASSERT_TRUE(has_action(&act, POWER_ACT_WAKE_HIGHZ, 0));

    int highz = action_index(&act, POWER_ACT_WAKE_HIGHZ, 0);
    int en = action_index(&act, POWER_ACT_SET_PWR_EN, 0);

    TEST_ASSERT_TRUE(highz >= 0 && en >= 0 && highz < en);
}

static void test_init_keeps_host_on_when_pwr_good(void)
{
    power_fsm_init(&fsm, 1000U, true, false, false, 0, 0, RESET_CAUSE_POR, false, POWER_WAKE_RESET,
                   0, &act);

    TEST_ASSERT_EQUAL_INT(HOST_STATE_BOOTING, fsm.state);
    TEST_ASSERT_TRUE(fsm.pwr_en);
    TEST_ASSERT_TRUE(has_action(&act, POWER_ACT_SET_PWR_EN, 1));
    TEST_ASSERT_FALSE(has_action(&act, POWER_ACT_SET_PWR_EN, 0));
    TEST_ASSERT_FALSE(fsm.host_state_valid);
}

static void test_init_pwr_good_zero_forces_off_even_with_backup(void)
{
    /* Хост обесточен, но backup сохранил pwr_en=1: сброс EC не должен
     * включать рейл (план §6: PWR_GOOD=0 ⇒ OFF). */
    power_fsm_init(&fsm, 1000U, false, true, true, HOST_STATE_AWAKE, 0, RESET_CAUSE_SFT, false,
                   POWER_WAKE_RESET, 0, &act);

    TEST_ASSERT_EQUAL_INT(HOST_STATE_OFF, fsm.state);
    TEST_ASSERT_FALSE(fsm.pwr_en);
    TEST_ASSERT_TRUE(has_action(&act, POWER_ACT_SET_PWR_EN, 0));
    TEST_ASSERT_FALSE(has_action(&act, POWER_ACT_SET_PWR_EN, 1));
}

static void test_init_backup_restores_awake_without_request(void)
{
    init_awake();

    TEST_ASSERT_EQUAL_INT(HOST_STATE_AWAKE, fsm.state);
    TEST_ASSERT_TRUE(fsm.pwr_en);
    TEST_ASSERT_EQUAL_INT(POWER_REQ_NONE, fsm.power_request);
    TEST_ASSERT_FALSE(fsm.host_state_valid);
}

static void test_reset_does_not_touch_power_or_request(void)
{
    init_awake();

    TEST_ASSERT_FALSE(has_action(&act, POWER_ACT_SET_PWR_EN, 0));
    TEST_ASSERT_FALSE(has_action(&act, POWER_ACT_SET_REQUEST, POWER_REQ_SLEEP));
    TEST_ASSERT_FALSE(has_action(&act, POWER_ACT_SET_REQUEST, POWER_REQ_SHUTDOWN));
    TEST_ASSERT_FALSE(has_action(&act, POWER_ACT_SET_REQUEST, POWER_REQ_REBOOT));

    uint8_t status[POWER_PAYLOAD_LEN];

    power_fsm_get_status(&fsm, status);
    TEST_ASSERT_EQUAL_INT(0, status[1] & POWER_FLAG_HOST_STATE);
}

/* ========================================================================
 * Жесты
 * ======================================================================== */

static void test_off_short_turns_host_on(void)
{
    init_off();

    uint32_t t = do_short_press(2000U);

    (void)t;
    TEST_ASSERT_EQUAL_INT(HOST_STATE_BOOTING, fsm.state);
    TEST_ASSERT_TRUE(fsm.pwr_en);
    TEST_ASSERT_TRUE(has_action(&act, POWER_ACT_SET_PWR_EN, 1));
}

static void test_off_long_is_reported_but_does_nothing(void)
{
    init_off();

    hold_long(2000U);
    release_button(2000U + DEBOUNCE + POWER_LONG_MS + 10U);

    TEST_ASSERT_EQUAL_INT(HOST_STATE_OFF, fsm.state);
    TEST_ASSERT_EQUAL_INT(POWER_EVENT_LONG_IN_OFF, fsm.last_event);
    TEST_ASSERT_FALSE(fsm.pwr_en);
}

static void test_awake_short_requests_sleep(void)
{
    init_awake();

    do_short_press(2000U);

    TEST_ASSERT_EQUAL_INT(HOST_STATE_SUSPENDING, fsm.state);
    TEST_ASSERT_EQUAL_INT(POWER_REQ_SLEEP, fsm.power_request);
    TEST_ASSERT_TRUE(has_action(&act, POWER_ACT_SET_REQUEST, POWER_REQ_SLEEP));
}

static void test_sleep_without_ack_returns_awake_without_power_change(void)
{
    init_awake();

    uint32_t t = do_short_press(2000U);

    power_fsm_tick(&fsm, t + POWER_REQUEST_TIMEOUT_MS, false, true, &act);

    TEST_ASSERT_EQUAL_INT(HOST_STATE_AWAKE, fsm.state);
    TEST_ASSERT_EQUAL_INT(POWER_REQ_NONE, fsm.power_request);
    TEST_ASSERT_TRUE(fsm.pwr_en);
    TEST_ASSERT_FALSE(has_action(&act, POWER_ACT_SET_PWR_EN, 0));
}

static void test_sleep_ack_then_host_suspended(void)
{
    init_awake();

    do_short_press(2000U);

    power_fsm_set_host_state(&fsm, 2500U, HOST_STATE_SUSPENDING, POWER_ACK_REQUEST, &act);
    TEST_ASSERT_EQUAL_INT(HOST_STATE_SUSPENDING, fsm.state);
    TEST_ASSERT_TRUE(fsm.request_acked);

    power_fsm_set_host_state(&fsm, 2600U, HOST_STATE_SUSPENDED, 0, &act);
    TEST_ASSERT_EQUAL_INT(HOST_STATE_SUSPENDED, fsm.state);
    TEST_ASSERT_EQUAL_INT(POWER_REQ_NONE, fsm.power_request);
}

static void test_host_suspending_state_acks_sleep_request(void)
{
    init_awake();

    uint32_t t = do_short_press(2000U);

    /* Хост сообщил SUSPENDING без ack_flags — это уже подтверждение. */
    power_fsm_set_host_state(&fsm, t + 10U, HOST_STATE_SUSPENDING, 0, &act);
    TEST_ASSERT_TRUE(fsm.request_acked);

    power_fsm_tick(&fsm, t + 10U + POWER_REQUEST_TIMEOUT_MS, false, true, &act);

    /* Запрос реализован: тишина хоста переводит EC в SUSPENDED и
     * снимает power_request. */
    TEST_ASSERT_EQUAL_INT(HOST_STATE_SUSPENDED, fsm.state);
    TEST_ASSERT_EQUAL_INT(POWER_REQ_NONE, fsm.power_request);
}

static void test_awake_long_shutdown_after_timeout(void)
{
    init_awake();

    hold_long(2000U);

    TEST_ASSERT_EQUAL_INT(HOST_STATE_SHUTTING_DOWN, fsm.state);
    TEST_ASSERT_EQUAL_INT(POWER_REQ_SHUTDOWN, fsm.power_request);

    uint32_t t = 2000U + DEBOUNCE + POWER_LONG_MS + 1U;
    power_fsm_tick(&fsm, t + POWER_REQUEST_TIMEOUT_MS, true, true, &act);

    TEST_ASSERT_EQUAL_INT(HOST_STATE_OFF, fsm.state);
    TEST_ASSERT_FALSE(fsm.pwr_en);
    TEST_ASSERT_TRUE(has_action(&act, POWER_ACT_SET_PWR_EN, 0));

    int highz = action_index(&act, POWER_ACT_WAKE_HIGHZ, 0);
    int en = action_index(&act, POWER_ACT_SET_PWR_EN, 0);

    TEST_ASSERT_TRUE(highz >= 0 && en >= 0 && highz < en);
}

static void test_awake_very_long_cuts_power_immediately(void)
{
    init_awake();

    hold_very_long(2000U);

    TEST_ASSERT_EQUAL_INT(HOST_STATE_OFF, fsm.state);
    TEST_ASSERT_FALSE(fsm.pwr_en);

    int highz = action_index(&act, POWER_ACT_WAKE_HIGHZ, 0);
    int en = action_index(&act, POWER_ACT_SET_PWR_EN, 0);

    TEST_ASSERT_TRUE(highz >= 0 && en >= 0 && highz < en);
    TEST_ASSERT_EQUAL_INT(POWER_EVENT_VERY_LONG, fsm.last_event);
}

static void test_awake_double_requests_reboot(void)
{
    init_awake();

    /* Два коротких нажатия в окне двойного. */
    power_fsm_tick(&fsm, 3000U, true, true, &act);
    power_fsm_tick(&fsm, 3000U + DEBOUNCE, true, true, &act);
    power_fsm_tick(&fsm, 3100U, false, true, &act);
    power_fsm_tick(&fsm, 3100U + DEBOUNCE, false, true, &act);
    power_fsm_tick(&fsm, 3200U, true, true, &act);
    power_fsm_tick(&fsm, 3200U + DEBOUNCE, true, true, &act);
    power_fsm_tick(&fsm, 3300U, false, true, &act);
    power_fsm_tick(&fsm, 3300U + DEBOUNCE, false, true, &act);

    TEST_ASSERT_EQUAL_INT(HOST_STATE_REBOOTING, fsm.state);
    TEST_ASSERT_EQUAL_INT(POWER_REQ_REBOOT, fsm.power_request);
    TEST_ASSERT_EQUAL_INT(POWER_EVENT_DOUBLE, fsm.last_event);
}

static void test_reboot_power_cycle_with_off_delay(void)
{
    init_awake();

    power_fsm_tick(&fsm, 3000U, true, true, &act);
    power_fsm_tick(&fsm, 3000U + DEBOUNCE, true, true, &act);
    power_fsm_tick(&fsm, 3100U, false, true, &act);
    power_fsm_tick(&fsm, 3100U + DEBOUNCE, false, true, &act);
    power_fsm_tick(&fsm, 3200U, true, true, &act);
    power_fsm_tick(&fsm, 3200U + DEBOUNCE, true, true, &act);
    power_fsm_tick(&fsm, 3300U, false, true, &act);
    power_fsm_tick(&fsm, 3300U + DEBOUNCE, false, true, &act);

    TEST_ASSERT_EQUAL_INT(HOST_STATE_REBOOTING, fsm.state);

    uint32_t timeout = 3300U + DEBOUNCE + POWER_REQUEST_TIMEOUT_MS;
    power_fsm_tick(&fsm, timeout, false, true, &act);

    TEST_ASSERT_EQUAL_INT(HOST_STATE_BOOTING, fsm.state);
    TEST_ASSERT_FALSE(fsm.pwr_en);
    TEST_ASSERT_TRUE(has_action(&act, POWER_ACT_SET_PWR_EN, 0));

    power_fsm_tick(&fsm, timeout + POWER_TOFF_MS + 1U, false, true, &act);

    TEST_ASSERT_TRUE(fsm.pwr_en);
    TEST_ASSERT_TRUE(has_action(&act, POWER_ACT_SET_PWR_EN, 1));
}

static void test_suspended_short_pulses_wake(void)
{
    init_awake();

    power_fsm_set_host_state(&fsm, 2000U, HOST_STATE_SUSPENDED, 0, &act);

    TEST_ASSERT_EQUAL_INT(HOST_STATE_SUSPENDED, fsm.state);

    do_short_press(3000U);

    TEST_ASSERT_EQUAL_INT(HOST_STATE_AWAKE, fsm.state);
    TEST_ASSERT_TRUE(has_action(&act, POWER_ACT_PULSE_WAKE, 0));
}

static void test_booting_long_cuts_power(void)
{
    power_fsm_init(&fsm, 1000U, true, false, false, 0, 0, RESET_CAUSE_POR, false, POWER_WAKE_RESET,
                   0, &act);
    TEST_ASSERT_EQUAL_INT(HOST_STATE_BOOTING, fsm.state);

    hold_long(1100U);
    release_button(1100U + DEBOUNCE + POWER_LONG_MS + 10U);

    TEST_ASSERT_EQUAL_INT(HOST_STATE_OFF, fsm.state);
    TEST_ASSERT_FALSE(fsm.pwr_en);
}

/* ========================================================================
 * BOOTING
 * ======================================================================== */

static void test_boot_fail_three_cycles_then_error(void)
{
    /* OFF → короткое нажатие, но PWR_GOOD так и не появляется. */
    init_off();

    power_fsm_tick(&fsm, 2000U, true, false, &act);
    power_fsm_tick(&fsm, 2000U + DEBOUNCE, true, false, &act);
    power_fsm_tick(&fsm, 2100U, false, false, &act);
    power_fsm_tick(&fsm, 2100U + DEBOUNCE, false, false, &act);

    uint32_t t = 2100U + DEBOUNCE + POWER_DOUBLE_WINDOW_MS + 1U;
    power_fsm_tick(&fsm, t, false, false, &act);

    TEST_ASSERT_EQUAL_INT(HOST_STATE_BOOTING, fsm.state);
    TEST_ASSERT_TRUE(fsm.pwr_en);

    for (int i = 0; i < (int)POWER_MAX_BOOT_ATTEMPTS; i++)
    {
        t += POWER_PWR_GOOD_TIMEOUT_MS + 1U;
        power_fsm_tick(&fsm, t, false, false, &act);

        if (i < (int)POWER_MAX_BOOT_ATTEMPTS - 1)
        {
            TEST_ASSERT_FALSE(fsm.pwr_en);
            t += POWER_TOFF_MS + 1U;
            power_fsm_tick(&fsm, t, false, false, &act);
            TEST_ASSERT_TRUE(fsm.pwr_en);
        }
    }

    TEST_ASSERT_EQUAL_INT(HOST_STATE_ERROR, fsm.state);
    TEST_ASSERT_TRUE(fsm.boot_fail);

    uint8_t status[POWER_PAYLOAD_LEN];

    power_fsm_get_status(&fsm, status);
    TEST_ASSERT_EQUAL_INT(POWER_FLAG_BOOT_FAIL, status[1] & POWER_FLAG_BOOT_FAIL);
}

static void test_boot_timeout_without_host_frame(void)
{
    power_fsm_init(&fsm, 1000U, true, false, false, 0, 0, RESET_CAUSE_POR, false, POWER_WAKE_RESET,
                   0, &act);

    power_fsm_tick(&fsm, 1000U + POWER_BOOT_TIMEOUT_MS + 1U, false, true, &act);

    TEST_ASSERT_EQUAL_INT(1, fsm.boot_attempts);
    TEST_ASSERT_FALSE(fsm.pwr_en);
}

static void test_boot_frame_moves_to_awake(void)
{
    power_fsm_init(&fsm, 1000U, true, false, false, 0, 0, RESET_CAUSE_POR, false, POWER_WAKE_RESET,
                   0, &act);

    power_fsm_host_frame(&fsm, 1500U);
    power_fsm_tick(&fsm, 1510U, false, true, &act);

    TEST_ASSERT_EQUAL_INT(HOST_STATE_AWAKE, fsm.state);
    TEST_ASSERT_EQUAL_INT(0, fsm.boot_attempts);
}

/* ========================================================================
 * Тишина хоста, Stop, ERROR
 * ======================================================================== */

static void test_host_silence_marks_suspended(void)
{
    init_awake();

    power_fsm_tick(&fsm, 1000U + POWER_HOST_SILENCE_MS + 1U, false, true, &act);

    TEST_ASSERT_EQUAL_INT(HOST_STATE_SUSPENDED, fsm.state);
    TEST_ASSERT_TRUE(fsm.link_lost);

    uint8_t status[POWER_PAYLOAD_LEN];

    power_fsm_get_status(&fsm, status);
    TEST_ASSERT_EQUAL_INT(POWER_FLAG_LINK_LOST, status[1] & POWER_FLAG_LINK_LOST);
}

static void test_any_frame_returns_awake(void)
{
    init_awake();

    power_fsm_tick(&fsm, 1000U + POWER_HOST_SILENCE_MS + 1U, false, true, &act);
    TEST_ASSERT_EQUAL_INT(HOST_STATE_SUSPENDED, fsm.state);

    power_fsm_host_frame(&fsm, 5000U);
    TEST_ASSERT_EQUAL_INT(HOST_STATE_AWAKE, fsm.state);
    TEST_ASSERT_FALSE(fsm.link_lost);
}

static void test_stop_requested_when_idle(void)
{
    init_off();

    power_fsm_tick(&fsm, 1000U + POWER_IDLE_STOP_MS + 1U, false, false, &act);

    TEST_ASSERT_TRUE(has_action(&act, POWER_ACT_ENTER_STOP, 0));
    TEST_ASSERT_TRUE(fsm.stop_armed);

    /* Повторно не запрашивается, пока не будет активности. */
    power_fsm_tick(&fsm, 1000U + POWER_IDLE_STOP_MS + 100U, false, false, &act);
    TEST_ASSERT_FALSE(has_action(&act, POWER_ACT_ENTER_STOP, 0));
}

static void test_gesture_blocks_stop(void)
{
    init_off();

    power_fsm_tick(&fsm, 4000U, true, false, &act);
    power_fsm_tick(&fsm, 4000U + DEBOUNCE, true, false, &act);
    power_fsm_tick(&fsm, 4000U + POWER_LONG_MS + DEBOUNCE, true, false, &act);

    TEST_ASSERT_FALSE(has_action(&act, POWER_ACT_ENTER_STOP, 0));
}

static void test_stop_blocked_while_request_pending(void)
{
    init_awake();

    do_short_press(2000U);
    TEST_ASSERT_EQUAL_INT(POWER_REQ_SLEEP, fsm.power_request);

    uint32_t t = 2000U + DEBOUNCE + 100U + DEBOUNCE + POWER_DOUBLE_WINDOW_MS + 1U;

    power_fsm_tick(&fsm, t + POWER_IDLE_STOP_MS + 1U, false, true, &act);

    TEST_ASSERT_FALSE(has_action(&act, POWER_ACT_ENTER_STOP, 0));
}

static void test_error_double_retries_boot(void)
{
    power_fsm_init(&fsm, 1000U, true, false, false, 0, 0, RESET_CAUSE_POR, false, POWER_WAKE_RESET,
                   0, &act);

    fsm.state = HOST_STATE_ERROR;
    fsm.boot_fail = true;
    fsm.pwr_en = true;

    /* Двойное нажатие в ERROR. */
    power_fsm_tick(&fsm, 2000U, true, true, &act);
    power_fsm_tick(&fsm, 2000U + DEBOUNCE, true, true, &act);
    power_fsm_tick(&fsm, 2100U, false, true, &act);
    power_fsm_tick(&fsm, 2100U + DEBOUNCE, false, true, &act);
    power_fsm_tick(&fsm, 2200U, true, true, &act);
    power_fsm_tick(&fsm, 2200U + DEBOUNCE, true, true, &act);
    power_fsm_tick(&fsm, 2300U, false, true, &act);
    power_fsm_tick(&fsm, 2300U + DEBOUNCE, false, true, &act);

    TEST_ASSERT_EQUAL_INT(HOST_STATE_BOOTING, fsm.state);
    TEST_ASSERT_FALSE(fsm.boot_fail);
    TEST_ASSERT_EQUAL_INT(POWER_EVENT_DOUBLE, fsm.last_event);
}

static void test_error_short_cuts_power(void)
{
    init_awake();

    fsm.state = HOST_STATE_ERROR;
    fsm.boot_fail = true;

    do_short_press(3000U);

    TEST_ASSERT_EQUAL_INT(HOST_STATE_OFF, fsm.state);
    TEST_ASSERT_FALSE(fsm.pwr_en);
    TEST_ASSERT_FALSE(fsm.boot_fail);
}

/* ========================================================================
 * SET_POWER / GET_POWER
 * ======================================================================== */

static void test_set_power_sets_host_state_flag(void)
{
    init_awake();

    power_fsm_set_host_state(&fsm, 2000U, HOST_STATE_AWAKE, 0, &act);

    TEST_ASSERT_TRUE(fsm.host_state_valid);

    uint8_t status[POWER_PAYLOAD_LEN];

    power_fsm_get_status(&fsm, status);
    TEST_ASSERT_EQUAL_INT(HOST_STATE_AWAKE, status[0]);
    TEST_ASSERT_EQUAL_INT(POWER_FLAG_HOST_STATE, status[1] & POWER_FLAG_HOST_STATE);
    TEST_ASSERT_TRUE(status[1] & POWER_FLAG_PWR_EN);
}

static void test_get_power_reports_wake_and_reset(void)
{
    power_fsm_init(&fsm, 1000U, false, false, false, 0, 0, RESET_CAUSE_PIN | RESET_CAUSE_SFT, false,
                   POWER_WAKE_RESET, 0, &act);

    power_fsm_on_wake(&fsm, 2000U, POWER_WAKE_BUTTON);

    uint8_t status[POWER_PAYLOAD_LEN];

    power_fsm_get_status(&fsm, status);

    TEST_ASSERT_EQUAL_INT(RESET_CAUSE_PIN | RESET_CAUSE_SFT, status[5]);
    TEST_ASSERT_EQUAL_INT(POWER_WAKE_BUTTON, status[6]);
    TEST_ASSERT_EQUAL_INT(1, status[7]);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_init_off_when_no_power);
    RUN_TEST(test_init_keeps_host_on_when_pwr_good);
    RUN_TEST(test_init_pwr_good_zero_forces_off_even_with_backup);
    RUN_TEST(test_init_backup_restores_awake_without_request);
    RUN_TEST(test_reset_does_not_touch_power_or_request);
    RUN_TEST(test_off_short_turns_host_on);
    RUN_TEST(test_off_long_is_reported_but_does_nothing);
    RUN_TEST(test_awake_short_requests_sleep);
    RUN_TEST(test_sleep_without_ack_returns_awake_without_power_change);
    RUN_TEST(test_sleep_ack_then_host_suspended);
    RUN_TEST(test_host_suspending_state_acks_sleep_request);
    RUN_TEST(test_awake_long_shutdown_after_timeout);
    RUN_TEST(test_awake_very_long_cuts_power_immediately);
    RUN_TEST(test_awake_double_requests_reboot);
    RUN_TEST(test_reboot_power_cycle_with_off_delay);
    RUN_TEST(test_suspended_short_pulses_wake);
    RUN_TEST(test_booting_long_cuts_power);
    RUN_TEST(test_boot_fail_three_cycles_then_error);
    RUN_TEST(test_boot_timeout_without_host_frame);
    RUN_TEST(test_boot_frame_moves_to_awake);
    RUN_TEST(test_host_silence_marks_suspended);
    RUN_TEST(test_any_frame_returns_awake);
    RUN_TEST(test_stop_requested_when_idle);
    RUN_TEST(test_gesture_blocks_stop);
    RUN_TEST(test_stop_blocked_while_request_pending);
    RUN_TEST(test_error_double_retries_boot);
    RUN_TEST(test_error_short_cuts_power);
    RUN_TEST(test_set_power_sets_host_state_flag);
    RUN_TEST(test_get_power_reports_wake_and_reset);
    return UNITY_END();
}