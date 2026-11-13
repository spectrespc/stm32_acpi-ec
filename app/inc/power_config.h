/* app/inc/power_config.h
 *
 * Пороги конечного автомата питания и кнопки. Все значения —
 * compile-time. Используются common/power_fsm.c и приложением.
 */

#ifndef POWER_CONFIG_H
#define POWER_CONFIG_H

/* Дребезг кнопки: уровень должен быть стабилен столько мс. */
#define POWER_DEBOUNCE_MS 30U

/* Короткое нажатие: отпускание раньше этого порога. */
#define POWER_SHORT_MAX_MS 1000U

/* Долгое нажатие срабатывает, пока кнопка удерживается. */
#define POWER_LONG_MS      3000U
#define POWER_VERY_LONG_MS 8000U

/* Окно ожидания второго короткого нажатия. */
#define POWER_DOUBLE_WINDOW_MS 400U

/* Ожидание graceful-подтверждения (SET_POWER/переход состояния). */
#define POWER_REQUEST_TIMEOUT_MS 5000U

/* BOOTING: ожидание PWR_GOOD и первого кадра хоста. */
#define POWER_PWR_GOOD_TIMEOUT_MS 500U
#define POWER_BOOT_TIMEOUT_MS     30000U
#define POWER_MAX_BOOT_ATTEMPTS   3U

/* Пауза обесточивания при reset (T_off) и максимум ожидания PWR_GOOD=0. */
#define POWER_TOFF_MS             1000U
#define POWER_OFF_GOOD_TIMEOUT_MS 2000U

/* Идле-таймаут до входа в Stop и таймаут «тишины хоста» (SUSPENDED). */
#define POWER_IDLE_STOP_MS    3000U
#define POWER_HOST_SILENCE_MS 3000U

/* Порог программного watchdog-а шины: если PE=1 и транзакция не
 * продвигается столько мс, периферия переконфигурируется (PE=0/1) и
 * отпускает SCL. Меньше таймаута rk3x-i2c (~20-30 мс), чтобы хост не
 * успел объявить ошибку. Легитимная растяжка EC — единицы микросекунд. */
#define POWER_BUS_HANG_MS 15U

/* Импульс HOST_WAKE. */
#define POWER_HOST_WAKE_PULSE_MS 150U

/* Авто-включение хоста при подаче питания: 0 — ждать кнопку. */
#ifndef POWER_ON_RESTORE
#define POWER_ON_RESTORE 0
#endif

#endif /* POWER_CONFIG_H */