/* boot/src/boot.c
 *
 * Валидация образа приложения и переход в него.
 */

#include "board.h"
#include "boot.h"
#include "boot_config.h"
#include "crc.h"
#include "meta.h"
#include "system.h"

bool app_validate(void)
{
    if ((meta.flags & META_FLAG_VALID) == 0U)
    {
        return false;
    }

    uint32_t len = meta.img_size;

    if (len < 8U || len > APP_SIZE)
    {
        return false;
    }

    uint32_t sp = *(__IO uint32_t *)APP_ADDR;
    uint32_t pc = *(__IO uint32_t *)(APP_ADDR + 4U);

    /* Initial SP должен находиться в RAM 20 KB */
    if (sp < 0x20000000UL || sp > 0x20005000UL)
    {
        return false;
    }

    /* Reset vector должен быть Thumb и находиться внутри образа */
    if ((pc & 1UL) == 0U)
    {
        return false;
    }

    uint32_t entry = pc & ~1UL;

    if (entry < APP_ADDR || entry >= APP_ADDR + len)
    {
        return false;
    }

    uint32_t crc = crc32_sw((const uint8_t *)APP_ADDR, len);

    return crc == meta.img_crc;
}

void boot_app(uint32_t addr)
{
    system_disable_irqs();

    /*
     * Выключаем I2C, который использовал bootloader.
     * Пин питания host-устройства специально не трогаем.
     */
    I2C1->CR1 &= ~I2C_CR1_PE;
    RCC->APB1ENR &= ~RCC_APB1ENR_I2C1EN;

    system_stop_systick();
    system_clear_irqs();

    system_set_vtor(addr);

    uint32_t sp = *(__IO uint32_t *)addr;
    uint32_t pc = *(__IO uint32_t *)(addr + 4U);

    __set_MSP(sp);

    system_enable_irqs();

    void (*app_reset)(void) = (void (*)(void))pc;
    app_reset();

    while (1)
    {
        ;
    }
}

void try_boot_selected(void)
{
    if (!app_validate())
    {
        /* Образ отсутствует или повреждён — остаёмся в загрузчике */
        return;
    }

    if (meta.flags & META_FLAG_PENDING)
    {
        if (meta.boot_attempts >= MAX_BOOT_ATTEMPTS)
        {
            /* Прошивка не подтвердила запуск несколько раз подряд —
             * помечаем её как невалидную. */
            MetaConfig m = meta;

            m.flags &= (uint16_t)~META_FLAG_PENDING;
            m.flags &= (uint16_t)~META_FLAG_VALID;

            meta_save(&m);
            return;
        }

        MetaConfig m = meta;

        m.boot_attempts++;

        meta_save(&m);
    }

    boot_app(APP_ADDR);
}
