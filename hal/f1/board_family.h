/* hal/f1/board_family.h
 *
 * STM32F103 (Cortex-M3, HSI 8 МГц): пины, тактирование и карта памяти,
 * зависящие от семейства. Общая часть — hal/include/board.h.
 */

#ifndef BOARD_FAMILY_H
#define BOARD_FAMILY_H

#include <stdint.h>

#include "stm32f1xx.h"
#include "stm32f1xx_ll_gpio.h"
#include "stm32f1xx_ll_bus.h"

/* ========================================================================
 * Светодиод (PB12)
 * ======================================================================== */

#define LED_GPIO_PORT   GPIOB
#define LED_GPIO_PIN    LL_GPIO_PIN_12
#define LED_GPIO_CLK    LL_APB2_GRP1_PERIPH_GPIOB
#define LED_ACTIVE_HIGH 1

/* Кнопки питания на F103-плате нет. */
#define BOARD_BUTTON_ACTIVE_LOW 1

/* ========================================================================
 * Питание host-устройства (PA1) — только загрузчик
 * ======================================================================== */

#ifdef BOOT_BUILD
#define HOST_PWR_GPIO_PORT   GPIOA
#define HOST_PWR_GPIO_PIN    LL_GPIO_PIN_1
#define HOST_PWR_GPIO_CLK    LL_APB2_GRP1_PERIPH_GPIOA
#define HOST_PWR_ACTIVE_HIGH 1
#define HOST_POWER_DELAY_MS  100U
#endif

/* ========================================================================
 * Тактирование и карта памяти
 * ======================================================================== */

#define EC_SYSCLK_HZ 8000000UL

/* Medium density: страница Flash 1 KB. */
#define FLASH_PAGE_SIZE 1024U

/* Верхняя граница RAM (20 KB). */
#define EC_RAM_BASE 0x20000000UL
#define EC_RAM_TOP  0x20005000UL

#endif /* BOARD_FAMILY_H */