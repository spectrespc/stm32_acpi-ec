/* app/inc/bus_watchdog.h
 *
 * Программный watchdog I2C-шины: отпускает SCL, если мастер исчез
 * посреди кадра. Подробности — app/src/bus_watchdog.c.
 */

#ifndef BUS_WATCHDOG_H
#define BUS_WATCHDOG_H

#include <stdint.h>

void bus_watchdog_init(uint32_t now_ms);
void bus_watchdog_tick(uint32_t now_ms);

#endif /* BUS_WATCHDOG_H */
