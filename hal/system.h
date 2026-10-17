/* hal/system.h
 *
 * Системные операции Cortex-M3: сброс, VTOR, прерывания и SysTick.
 */

#ifndef SYSTEM_H
#define SYSTEM_H

#include <stdint.h>

/* Аппаратный сброс контроллера (NVIC_SystemReset). */
void system_reset(void);

/* Установить таблицу векторов и синхронизировать конвейер. */
void system_set_vtor(uint32_t addr);

void system_disable_irqs(void);
void system_enable_irqs(void);

/* Снять все IRQ (ICER) и очистить ожидающие (ICPR). */
void system_clear_irqs(void);

/* Остановить SysTick. */
void system_stop_systick(void);

#endif /* SYSTEM_H */