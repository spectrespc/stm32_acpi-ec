/* boot/src/main.c
 *
 * Точка входа загрузчика:
 *   1. Проверить флаг входа в режим обновления в RAM.
 *   2. Если флага нет — попытаться запустить приложение.
 *   3. Иначе остаться в режиме обновления и обслуживать I2C.
 */

#include <string.h>

#include "board.h"
#include "boot.h"
#include "flash.h"
#include "i2c.h"
#include "meta.h"
#include "protocol.h"

/* ========================================================================
 * Проверка флага обновления
 * ======================================================================== */

/*
 * Проверяет, записано ли магическое число в RAM.
 * Если да — стирает его и возвращает 1 (остаться в режиме обновления).
 * Если нет — возвращает 0 (запустить прошивку).
 *
 * UPDATE_MAGIC и символ __ec_update_flag — общий контракт с
 * приложением (common/protocol.h). Верхние 16 байт RAM исключены из
 * стека в обоих линкер-скриптах, поэтому флаг не затирается при старте.
 */
static int check_update_flag(void)
{
    volatile uint32_t *flag = &__ec_update_flag;

    if (*flag == UPDATE_MAGIC)
    {
        *flag = 0; /* Стираем флаг, чтобы при следующем старте он не сработал */
        return 1;  /* Остаться в режиме обновления */
    }

    return 0;
}

#ifdef BOOT_SELFTEST
/* ========================================================================
 * Диагностика Flash без I2C-мастера.
 *
 * Стирает последнюю страницу области приложения, пишет контрольный
 * double-word и читает обратно. Результат кодируется LED (см. README /
 * docs/architecture.md): PASS — 3 вспышки, FAIL — непрерывное мигание
 * 100 мс. После проверки загрузчик не запускает приложение.
 * ======================================================================== */

static bool boot_selftest_flash(void)
{
    const uint8_t pattern[8] = {0x5AU, 0xA5U, 0x00U, 0xFFU, 0x12U, 0x34U, 0x56U, 0x78U};
    uint32_t addr = APP_ADDR + APP_SIZE - FLASH_PAGE_SIZE;
    uint8_t readback[8];

    if (flash_erase_page(addr) != 0)
    {
        return false;
    }

    if (flash_program_buffer(addr, pattern, (uint16_t)sizeof(pattern)) != 0)
    {
        return false;
    }

    flash_read(addr, readback, sizeof(readback));

    return memcmp(readback, pattern, sizeof(pattern)) == 0;
}

static void boot_selftest_report(bool ok)
{
    while (1)
    {
        if (ok)
        {
            led_blink(3, 100U);
            board_delay_ms(1000U);
        }
        else
        {
            led_toggle();
            board_delay_ms(100U);
        }
    }
}
#endif /* BOOT_SELFTEST */

/* ========================================================================
 * main
 * ======================================================================== */

int main(void)
{
    board_init();
    i2c_init();
    meta_load();

#ifdef BOOT_SELFTEST
    boot_selftest_report(boot_selftest_flash());
#endif

    /* Проверяем, не запрошен ли вход в режим обновления */
    int force_update = check_update_flag();

    if (!force_update)
    {
        /* Пытаемся запустить прошивку.
         * Если образ валиден, функция выполнит переход в приложение
         * и не вернётся. */
        try_boot_selected();
    }

    /* Если дошли сюда — остались в режиме обновления. */

    uint32_t last_led = millis();

    while (1)
    {
        /* Обрабатываем I2C запросы максимально быстро */
        if (i2c_process_request)
        {
            i2c_process_request = 0;
            process_i2c_request();
        }

        /* Мигание LED раз в 500 мс (неблокирующим способом) */
        uint32_t now = millis();
        if ((uint32_t)(now - last_led) >= 500U)
        {
            last_led = now;
            led_toggle();
        }
    }
}
