/* app/inc/battery.h
 *
 * Эмуляция контроллера батареи 3S Li-ion и команды протокола.
 */

#ifndef BATTERY_H
#define BATTERY_H

#include <stdint.h>

/* Команды протокола контроллера батареи */
#define BAT_CMD_GET_INFO        0x01
#define BAT_CMD_GET_DATA        0x10
#define BAT_CMD_GET_CELL_VOLT   0x11
#define BAT_CMD_GET_TEMP        0x12

/* Статусы батареи (битовые флаги) */
#define BAT_STATUS_DISCHARGING  (1U << 0)
#define BAT_STATUS_CHARGING     (1U << 1)
#define BAT_STATUS_FULL         (1U << 2)
#define BAT_STATUS_ERROR        (1U << 3)

/* Количество ячеек */
#define BAT_NUM_CELLS           3

/* Структура данных батареи */
typedef struct {
    uint16_t voltage_mv;       /* Напряжение батареи, мВ */
    int16_t  current_ma;       /* Ток, мА (+ заряд, - разряд) */
    int16_t  temperature_c10;  /* Температура, 0.1°C */
    uint8_t  soc_percent;      /* Уровень заряда, % */
    uint8_t  status;           /* Флаги статуса */
    uint16_t cell_mv[BAT_NUM_CELLS]; /* Напряжение ячеек, мВ */
} BatteryData;

void battery_init(void);
void battery_update(void);
const BatteryData *battery_get_data(void);

#endif /* BATTERY_H */