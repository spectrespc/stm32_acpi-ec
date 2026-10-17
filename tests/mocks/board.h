/* tests/mocks/board.h
 *
 * Хостовая подмена hal/board.h: только карта Flash, нужная common/meta.
 */

#ifndef BOARD_H
#define BOARD_H

#include <stdint.h>

#define FLASH_TOTAL_KB      64

#define FLASH_BASE_ADDR     0x08000000UL
#define FLASH_TOTAL_SIZE    ((uint32_t)(FLASH_TOTAL_KB) * 1024U)
#define FLASH_PAGE_SIZE     1024U

#define BOOT_SIZE           (8U * 1024U)
#define META_SIZE           (2U * FLASH_PAGE_SIZE)

#define APP_SIZE            (FLASH_TOTAL_SIZE - BOOT_SIZE - META_SIZE)
#define APP_ADDR            (FLASH_BASE_ADDR + BOOT_SIZE)

#define META_PAGE1_ADDR     (FLASH_BASE_ADDR + FLASH_TOTAL_SIZE - FLASH_PAGE_SIZE)
#define META_PAGE0_ADDR     (META_PAGE1_ADDR - FLASH_PAGE_SIZE)

#endif /* BOARD_H */