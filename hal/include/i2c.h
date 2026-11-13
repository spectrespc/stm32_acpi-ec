/* hal/include/i2c.h
 *
 * I2C-slave загрузчика и приложения. Пауза/возобновление нужны
 * загрузчику и приложению: во время стирания/записи Flash обслуживание
 * I2C невозможно (ядро занято ожиданием готовности Flash), поэтому
 * периферию приостанавливают на время операции.
 */

#ifndef I2C_H
#define I2C_H

#include <stdint.h>
#include <stdbool.h>

#define I2C_RX_BUF_SIZE 260U
#define I2C_TX_BUF_SIZE 260U

extern volatile uint8_t rx_buf[I2C_RX_BUF_SIZE];
extern volatile uint16_t rx_count;
extern volatile uint8_t i2c_process_request;

/* Состояние slave для программного watchdog-а шины. */
typedef struct
{
    bool enabled;   /* PE = 1: периферия обслуживает шину */
    bool busy;      /* идёт транзакция (ISR.BUSY) */
    bool tx_active; /* включён TXIE: ответ ещё не вычитан мастером */
    uint16_t tx_pos;
    uint16_t rx_count;
} i2c_bus_state_t;

void i2c_init(void);

/* Выключить/включить периферию I2C (PE=0/1). В выключенном состоянии
 * линии остаются High-Z, что исключает back-power выключенного хоста. */
void i2c_set_enabled(bool enabled);

/* Полностью освободить I2C перед запуском приложения. */
void i2c_deinit(void);

/* Пауза/возобновление I2C на время операции с Flash. PE=0 отпускает
 * линии, поэтому незавершённый кадр не удерживает SCL. */
void i2c_pause(void);
void i2c_resume(void);

/* Принудительно переконфигурировать периферию (PE=0/1): снимает
 * застрявший TXDR и clock stretching. Используется таймерным
 * bus-watchdog и обработчиком TIMEOUT. */
void i2c_recover(void);

/* Снимок состояния для bus-watchdog (см. app/src/bus_watchdog.c). */
void i2c_get_bus_state(i2c_bus_state_t *state);

/* true — платформа поддерживает таймерное освобождение шины. */
bool i2c_bus_watchdog_supported(void);

void i2c_set_response(uint8_t status, const uint8_t *payload, uint16_t plen);
void i2c_set_status(uint8_t status);

/* true — с момента предыдущего вызова был адрес на шине (используется
 * для определения пробуждения из Stop по адресу I2C). */
bool i2c_take_wake_flag(void);

/* WUPEN: включать только перед WFI и снимать после пробуждения. */
void i2c_wakeup_arm(void);
void i2c_wakeup_disarm(void);

#endif /* I2C_H */