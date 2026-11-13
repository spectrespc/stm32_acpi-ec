/* hal/include/board.h
 *
 * Общая часть описания платы: карта Flash, светодиод, SysTick и
 * кнопка питания. Всё, что зависит от семейства MCU (такты, имена
 * пинов, размер страницы Flash), находится в board_family.h,
 * который выбирается CMake-переменной EC_HAL_DIR (hal/f1, hal/c0).
 *
 * Различающийся между образами код включается по BOOT_BUILD.
 */

#ifndef BOARD_H
#define BOARD_H

#include <stdint.h>
#include <stdbool.h>

#include "board_family.h"

/* ========================================================================
 * I2C-адрес (7 бит), общий для загрузчика и приложения
 * ======================================================================== */

#define I2C_ADDR7      0x42U
#define I2C_SLAVE_ADDR I2C_ADDR7

/* ========================================================================
 * Карта Flash.
 *
 *   Bootloader :  8 KB @ 0x08000000
 *   App        : остаток минус 2 страницы метаданных
 *   Meta 0     :  предпоследняя страница
 *   Meta 1     :  последняя страница
 *
 * F103 (страница 1 KB):
 *   C8T6  (64 KB):  App 54 KB @ 0x08002000,  Meta 0x0800F800/0x0800FC00
 *   CBT6 (128 KB):  App 120 KB @ 0x08002000, Meta 0x0801F800/0x0801FC00
 * C031 (страница 2 KB):
 *   C031x6 (32 KB): App 20 KB @ 0x08002000,  Meta 0x08007000/0x08007800
 * ======================================================================== */

#if !defined(FLASH_TOTAL_KB)
#error "FLASH_TOTAL_KB is not defined. Define it in the build system."
#endif

#ifndef FLASH_PAGE_SIZE
#error "FLASH_PAGE_SIZE must be defined in board_family.h"
#endif

#define FLASH_BASE_ADDR  0x08000000UL
#define FLASH_TOTAL_SIZE ((uint32_t)(FLASH_TOTAL_KB) * 1024U)

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

/* true — кнопка питания нажата (активный уровень — в board_family.h). */
bool board_button_pressed(void);

uint32_t millis(void);

#ifdef BOOT_BUILD
void led_blink(uint32_t count, uint32_t delay_ms);
void board_delay_ms(uint32_t ms);
#endif

#endif /* BOARD_H */