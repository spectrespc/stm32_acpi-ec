/* hal/f1/power.c
 *
 * STM32F103: управление питанием хоста и Stop не поддерживаются.
 * Все функции — заглушки, чтобы приложение и общий power_fsm
 * собирались для всех целей без #ifdef в прикладном коде.
 */

#include "power.h"

bool power_supported(void)
{
    return false;
}

void power_init(void)
{
}

void power_set_pwr_en(bool on)
{
    (void)on;
}

bool power_get_pwr_en(void)
{
    return false;
}

bool power_get_pwr_good(void)
{
    return false;
}

bool power_lse_ok(void)
{
    return false;
}

void power_set_host_wake(bool assert_low)
{
    (void)assert_low;
}

void power_pulse_host_wake(void)
{
}

uint8_t power_backup_read(uint8_t index)
{
    (void)index;
    return 0;
}

void power_backup_write(uint8_t index, uint8_t value)
{
    (void)index;
    (void)value;
}

uint8_t power_reset_cause(void)
{
    return 0;
}

bool power_enter_stop(uint32_t timeout_ms)
{
    (void)timeout_ms;
    return false;
}

uint8_t power_last_wake_src(void)
{
    return WAKE_SRC_NONE;
}

uint8_t power_wake_count(void)
{
    return 0;
}

void power_clear_wake_src(void)
{
}