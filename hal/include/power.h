/* hal/include/power.h
 *
 * Управление питанием хоста и сном EC.
 *
 * На STM32C031 (hal/c0) реализованы выходы PWR_EN/HOST_WAKE, чтение
 * PWR_GOOD, backup-регистры PWR, вход в Stop и определение причины
 * пробуждения. На STM32F103 (hal/f1) все функции — заглушки, а
 * power_supported() возвращает false: поведение старой платы не
 * меняется.
 */

#ifndef POWER_H
#define POWER_H

#include <stdint.h>
#include <stdbool.h>

/* Источник последнего пробуждения (поле last_wake_src в GET_POWER).
 * Значение 3 (LPTIM) зарезервировано протоколом: в STM32C031 нет
 * LPTIM, поэтому оно не выдаётся. */
typedef enum
{
    WAKE_SRC_NONE = 0,
    WAKE_SRC_BUTTON = 1,
    WAKE_SRC_I2C = 2,
    WAKE_SRC_LPTIM = 3,
    WAKE_SRC_RESET = 4
} wake_src_t;

/* Причина сброса — битовая маска (поле reset_cause в GET_POWER). */
#define RESET_CAUSE_POR  (1U << 0)
#define RESET_CAUSE_PIN  (1U << 1)
#define RESET_CAUSE_SFT  (1U << 2)
#define RESET_CAUSE_IWDG (1U << 3)
#define RESET_CAUSE_WWDG (1U << 4)
#define RESET_CAUSE_BOR  (1U << 5)
#define RESET_CAUSE_LPWR (1U << 6)

#define POWER_BKP_NONE 0xFFU

/* true — платформа поддерживает управление питанием (C031). */
bool power_supported(void);

/* Инициализация: пины PWR_EN/PWR_GOOD/HOST_WAKE, EXTI кнопки,
 * разблокировка backup-домена. Не меняет PWR_EN до применения FSM. */
void power_init(void);

void power_set_pwr_en(bool on);
bool power_get_pwr_en(void);
bool power_get_pwr_good(void);

/* LSE запущен и готов (флаг b5 в GET_POWER). На C031 LSE не
 * используется (нет LPTIM), поэтому всегда false. */
bool power_lse_ok(void);

/* HOST_WAKE: open-drain, low = активный импульс, high-Z = отпущено. */
void power_set_host_wake(bool assert_low);
void power_pulse_host_wake(void);

/* Backup-регистры (PWR->BKP0R..). POWER_BKP_NONE — нет такого регистра.
 * 32-битные функции нужны форензике I2C (BKP2R/BKP3R): backup-домен
 * переживает NRST, поэтому состояние шины видно после сброса. */
uint8_t power_backup_read(uint8_t index);
void power_backup_write(uint8_t index, uint8_t value);
uint32_t power_backup_read32(uint8_t index);
void power_backup_write32(uint8_t index, uint32_t value);

/* Причина сброса (битовая маска RESET_CAUSE_*). */
uint8_t power_reset_cause(void);

/* Вход в Stop. timeout_ms > 0 — таймер пробуждения не используется
 * на C031 (нет LPTIM); аргумент сохраняется для совместимости API.
 * Возвращает false, если сон не поддерживается. */
bool power_enter_stop(uint32_t timeout_ms);

/* Параметры последнего пробуждения из Stop. */
uint8_t power_last_wake_src(void);
uint8_t power_wake_count(void);
void power_clear_wake_src(void);

#endif /* POWER_H */