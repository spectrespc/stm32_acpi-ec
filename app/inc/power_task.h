/* app/inc/power_task.h
 *
 * Мост между конечным автоматом питания (common/power_fsm.c) и HAL:
 * события кнопки/хоста → FSM, действия FSM → пины, I2C, Stop, LED.
 */

#ifndef POWER_TASK_H
#define POWER_TASK_H

#include <stdint.h>
#include <stdbool.h>

#include "protocol.h"

void power_task_init(void);
void power_task_tick(void);

/* Любой валидный кадр от хоста (сбрасывает таймаут тишины). */
void power_task_host_frame(void);

/* CMD_SET_POWER: host_state и ack_flags. */
void power_task_set_host_state(uint8_t host_state, uint8_t ack_flags);

/* CMD_GET_POWER: 8 байт ответа. */
void power_task_get_status(uint8_t out[POWER_PAYLOAD_LEN]);

/* false — платформа без управления питанием (F103): приложение
 * сохраняет прежнее поведение (LED 500 мс, I2C всегда включён). */
bool power_task_supported(void);

#endif /* POWER_TASK_H */