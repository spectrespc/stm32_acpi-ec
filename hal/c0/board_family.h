/* hal/c0/board_family.h
 *
 * STM32C031F6P6 (Cortex-M0+, TSSOP20, HSI 48 МГц / HSIDIV=4 → 12 МГц):
 * пины, тактирование и карта памяти, зависящие от семейства.
 *
 * Карта выводов — docs/hardware.md (DS13867, TSSOP20):
 *   PB6  I2C1_SCL      PA0  BTN_PWR (EXTI0)
 *   PB7  I2C1_SDA      PA1  PWR_EN
 *   PA2  PWR_GOOD      PA3  HOST_WAKE (open-drain)
 *   PA4  LED_STATUS    PA13/PA14 SWDIO/SWCLK
 */

#ifndef BOARD_FAMILY_H
#define BOARD_FAMILY_H

#include <stdint.h>

#include "stm32c0xx.h"

/* ========================================================================
 * Светодиод (PA4, active-high)
 * ======================================================================== */

#define LED_GPIO_PORT   GPIOA
#define LED_GPIO_PIN    (1U << 4)
#define LED_ACTIVE_HIGH 1

/* ========================================================================
 * Кнопка питания (PA0, замыкает на GND, внутренний pull-up)
 * ======================================================================== */

#define BTN_GPIO_PORT           GPIOA
#define BTN_GPIO_PIN            (1U << 0)
#define BTN_EXTI_LINE           0U
#define BTN_IRQn                EXTI0_1_IRQn
#define BOARD_BUTTON_ACTIVE_LOW 1

/* ========================================================================
 * Управление питанием хоста
 * ======================================================================== */

#define PWR_EN_GPIO_PORT GPIOA
#define PWR_EN_GPIO_PIN  (1U << 1)

#define PWR_GOOD_GPIO_PORT GPIOA
#define PWR_GOOD_GPIO_PIN  (1U << 2)

#define HOST_WAKE_GPIO_PORT GPIOA
#define HOST_WAKE_GPIO_PIN  (1U << 3)

/* ========================================================================
 * Тактирование и карта памяти
 * ======================================================================== */

/* HSI 48 МГц с HSIDIV=/4. После выхода из Stop SYSCLK тоже 12 МГц. */
#define EC_SYSCLK_HZ 12000000UL

/* C031: страница Flash 2 KB, программирование double-word (8 байт). */
#define FLASH_PAGE_SIZE    2048U
#define FLASH_PROGRAM_UNIT 8U

/* SRAM: C011F6 — 6 KB, C031F6 — 12 KB (EC_RAM_SIZE_KB из CMake).
 * Верхние 16 байт исключены из стека. */
#ifndef EC_RAM_SIZE_KB
#define EC_RAM_SIZE_KB 12U
#endif

#define EC_RAM_BASE 0x20000000UL
#define EC_RAM_TOP  (EC_RAM_BASE + ((uint32_t)EC_RAM_SIZE_KB * 1024U))

/* Тактирование (hal/c0/clock.c). */
void clock_init(void);
void clock_restore_after_stop(void);

#endif /* BOARD_FAMILY_H */