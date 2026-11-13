/* app/src/bus_watchdog.c
 *
 * Программный watchdog I2C-шины (F1b).
 *
 * Slave-transmitter держит SCL, пока TXDR не заполнен. Если мастер
 * исчез посреди чтения (reboot RV1106), STOPF/NACKF не приходят, и EC
 * может удерживать линию вечно. Аппаратный tTIMEOUT (hal/c0/i2c.c)
 * закрывает это первым; этот watchdog — страховка на случай, если
 * таймер не сработал или сработал позже хоста.
 *
 * Критерий зависания: PE=1, идёт транзакция (BUSY или TXIE), но
 * tx_pos/rx_count не меняются дольше POWER_BUS_HANG_MS. Легитимный
 * stretch у EC короткий (ISR пишет TXDR сразу), поэтому порог 15 мс
 * безопасен и ниже таймаута rk3x-i2c (~20-30 мс).
 *
 * Ограничение: в Stop счётчика нет, но в Stop периферия выключена и
 * линию EC не держит.
 */

#include "bus_watchdog.h"

#include "i2c.h"
#include "i2c_debug.h"
#include "power_config.h"

static bool s_armed;
static uint16_t s_last_tx_pos;
static uint16_t s_last_rx_count;
static uint32_t s_last_progress_ms;

void bus_watchdog_init(uint32_t now_ms)
{
    s_armed = false;
    s_last_tx_pos = 0;
    s_last_rx_count = 0;
    s_last_progress_ms = now_ms;
}

void bus_watchdog_tick(uint32_t now_ms)
{
    if (!i2c_bus_watchdog_supported())
    {
        return;
    }

    i2c_bus_state_t state;

    i2c_get_bus_state(&state);

    if (!state.enabled || (!state.busy && !state.tx_active))
    {
        s_armed = false;
        return;
    }

    if (!s_armed)
    {
        s_armed = true;
        s_last_tx_pos = state.tx_pos;
        s_last_rx_count = state.rx_count;
        s_last_progress_ms = now_ms;
        return;
    }

    if ((state.tx_pos != s_last_tx_pos) || (state.rx_count != s_last_rx_count))
    {
        s_last_tx_pos = state.tx_pos;
        s_last_rx_count = state.rx_count;
        s_last_progress_ms = now_ms;
        return;
    }

    uint32_t stalled_ms = (uint32_t)(now_ms - s_last_progress_ms);

    if (stalled_ms >= POWER_BUS_HANG_MS)
    {
        i2c_debug_note_hang(stalled_ms);
        i2c_recover();
        s_armed = false;
    }
}