/* hal/board.h
 *
 * Плата: тактирование (HSI 8 МГц), SysTick, светодиод, питание
 * host-устройства (только загрузчик) и карта Flash.
 *
 * Различающийся между образами код включается по BOOT_BUILD.
 */

#ifndef BOARD_H
#define BOARD_H

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
 * I2C-адрес (7 бит), общий для загрузчика и приложения
 * ======================================================================== */

#define I2C_ADDR7 0x42U

/* ========================================================================
 * Карта Flash для STM32F103 (один слот приложения).
 *
 *   Bootloader :  8 KB @ 0x08000000
 *   App        : остаток минус 2 KB метаданных
 *   Meta 0     :  1 KB (предпоследняя страница)
 *   Meta 1     :  1 KB (последняя страница)
 *
 * Для C8T6  (64 KB):  App 54 KB @ 0x08002000, Meta 0x0800F800/0x0800FC00
 * Для CBT6 (128 KB): App 120 KB @ 0x08002000, Meta 0x0801F800/0x0801FC00
 * Оба чипа — medium density, страница Flash = 1 KB.
 * ======================================================================== */

#if !defined(FLASH_TOTAL_KB)
#error "FLASH_TOTAL_KB is not defined. Define it in the build system."
#endif

#define FLASH_BASE_ADDR  0x08000000UL
#define FLASH_TOTAL_SIZE ((uint32_t)(FLASH_TOTAL_KB) * 1024U)
#define FLASH_PAGE_SIZE  1024U

#define BOOT_SIZE (8U * 1024U)
#define META_SIZE (2U * FLASH_PAGE_SIZE)

#define APP_SIZE (FLASH_TOTAL_SIZE - BOOT_SIZE - META_SIZE)
#define APP_ADDR (FLASH_BASE_ADDR + BOOT_SIZE)

#define META_PAGE1_ADDR (FLASH_BASE_ADDR + FLASH_TOTAL_SIZE - FLASH_PAGE_SIZE)
#define META_PAGE0_ADDR (META_PAGE1_ADDR - FLASH_PAGE_SIZE)

/* ========================================================================
 * API
 * ======================================================================== */

void board_init(void);
void led_init(void);
void led_on(void);
void led_off(void);
void led_toggle(void);
uint32_t millis(void);

#ifdef BOOT_BUILD
void led_blink(uint32_t count, uint32_t delay_ms);
#endif

#endif /* BOARD_H */
