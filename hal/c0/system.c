/* hal/c0/system.c
 *
 * Небольшие обёртки над регистрами ядра Cortex-M0+ — отделены, чтобы
 * их можно было переиспользовать в загрузчике и приложении.
 */

#include "stm32c0xx.h"
#include "system.h"

void system_reset(void)
{
    NVIC_SystemReset();
}

void system_set_vtor(uint32_t addr)
{
    SCB->VTOR = addr;
    __DSB();
    __ISB();
}

void system_disable_irqs(void)
{
    __disable_irq();
}

void system_enable_irqs(void)
{
    __enable_irq();
}

void system_clear_irqs(void)
{
    uint32_t count = (uint32_t)(sizeof(NVIC->ICER) / sizeof(NVIC->ICER[0]));

    for (uint32_t i = 0; i < count; i++)
    {
        NVIC->ICER[i] = 0xFFFFFFFFUL;
        NVIC->ICPR[i] = 0xFFFFFFFFUL;
    }
}

void system_stop_systick(void)
{
    SysTick->CTRL = 0;
    SysTick->LOAD = 0;
    SysTick->VAL = 0;
}