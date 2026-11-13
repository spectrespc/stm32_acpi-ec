/* app/src/main.c
 *
 * Основная прошивка EC (один слот приложения).
 *
 *   - слинкована на единственный адрес 0x08002000, релокация не нужна;
 *   - I2C slave на адресе 0x42;
 *   - эмуляция батареи 3S Li-ion со статическими данными;
 *   - подтверждение загрузки (снимает флаг PENDING);
 *   - команды входа в режим обновления (0x0E) и сброса (0x08);
 *   - FSM кнопки/питания (только на платформах с power_supported()).
 */

#include "board.h"
#include "battery.h"
#include "bus_watchdog.h"
#include "commands.h"
#include "i2c.h"
#include "meta.h"
#include "power_task.h"
#include "system.h"

/* Начало секции .isr_vector из скрипта линковки = 0x08002000 */
extern uint32_t _flash_start;

int main(void)
{
    /* Таблица векторов находится в Flash на своём месте (единственный
     * слот приложения). Выставляем VTOR явно: при запуске через
     * загрузчик это избыточно, но обязательно при запуске/отладке
     * напрямую (после аппаратного сброса VTOR = 0x08000000). */
    system_set_vtor((uint32_t)&_flash_start);

    board_init();
    battery_init();

    /* Подтверждаем загрузку: снимаем флаг PENDING.
     * Должно произойти до истечения лимита попыток загрузчика. */
    boot_confirm();

    i2c_init();
    power_task_init();

    bus_watchdog_init(millis());

    bool power_managed = power_task_supported();

    uint32_t last_led = millis();
    uint32_t last_bat_update = millis();

    while (1)
    {
        /* Обрабатываем I2C запросы максимально быстро */
        if (i2c_process_request)
        {
            i2c_process_request = 0;
            app_process_i2c_request();
        }

        uint32_t now = millis();

        /* Страховка от исчезнувшего мастера: линия не должна остаться
         * прижатой до перезагрузки EC. */
        bus_watchdog_tick(now);

        if (power_managed)
        {
            power_task_tick();
        }
        else
        {
            /* F103: прежнее поведение — мигание раз в 500 мс. */
            if ((uint32_t)(now - last_led) >= 500U)
            {
                last_led = now;
                led_toggle();
            }
        }

        /* Обновление данных батареи раз в 1 секунду */
        if ((uint32_t)(now - last_bat_update) >= 1000U)
        {
            last_bat_update = now;
            battery_update();
        }
    }
}