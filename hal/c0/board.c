/* hal/c0/board.c
 *
 * Плата STM32C031F6P6: светодиод PA4, кнопка PA0 (EXTI0), SysTick.
 *
 * PWR_EN (PA1) здесь НЕ конфигурируется: загрузчик не трогает питание
 * хоста, линию держит внешний pull-up. Приложение настраивает PA1 в
 * power_init() (hal/c0/power.c).
 */

#include "board.h"

static volatile uint32_t ms_ticks;

void SysTick_Handler(void)
{
    ms_ticks++;
}

static void gpio_init(void)
{
    RCC->IOPENR |= (RCC_IOPENR_GPIOAEN | RCC_IOPENR_GPIOBEN);

    /* LED PA4: push-pull output, низкий уровень (выключен). */
    GPIOA->BSRR = LED_GPIO_PIN << 16U;
    GPIOA->OTYPER &= ~LED_GPIO_PIN;
    GPIOA->PUPDR &= ~(3U << (4U * 2U));
    GPIOA->MODER = (GPIOA->MODER & ~(3U << (4U * 2U))) | (1U << (4U * 2U));

    /* Кнопка PA0: input, pull-up. */
    GPIOA->PUPDR = (GPIOA->PUPDR & ~(3U << (0U * 2U))) | (1U << (0U * 2U));
    GPIOA->MODER &= ~(3U << (0U * 2U));

    /* PWR_GOOD PA2: input, pull-down (0 при обесточенном рейле). */
    GPIOA->PUPDR = (GPIOA->PUPDR & ~(3U << (2U * 2U))) | (2U << (2U * 2U));
    GPIOA->MODER &= ~(3U << (2U * 2U));

    /* HOST_WAKE PA3: open-drain, High-Z (MODER=input, ODR=0). */
    GPIOA->BSRR = HOST_WAKE_GPIO_PIN << 16U;
    GPIOA->OTYPER |= HOST_WAKE_GPIO_PIN;
    GPIOA->PUPDR &= ~(3U << (3U * 2U));
    GPIOA->MODER &= ~(3U << (3U * 2U));

    /* EXTI0 от PA0, падающий фронт (кнопка замыкает на GND). */
    EXTI->EXTICR[0] &= ~EXTI_EXTICR1_EXTI0;
    EXTI->FTSR1 |= EXTI_FTSR1_FT0;
    EXTI->RTSR1 &= ~EXTI_RTSR1_RT0;
    EXTI->FPR1 = EXTI_FPR1_FPIF0;
    EXTI->RPR1 = EXTI_RPR1_RPIF0;
    EXTI->IMR1 |= EXTI_IMR1_IM0;

    NVIC_SetPriority(BTN_IRQn, 1);
    NVIC_EnableIRQ(BTN_IRQn);
}

void led_init(void)
{
    led_off();
}

void led_on(void)
{
#if LED_ACTIVE_HIGH
    GPIOA->BSRR = LED_GPIO_PIN;
#else
    GPIOA->BSRR = LED_GPIO_PIN << 16U;
#endif
}

void led_off(void)
{
#if LED_ACTIVE_HIGH
    GPIOA->BSRR = LED_GPIO_PIN << 16U;
#else
    GPIOA->BSRR = LED_GPIO_PIN;
#endif
}

void led_toggle(void)
{
    if (GPIOA->ODR & LED_GPIO_PIN)
    {
        led_off();
    }
    else
    {
        led_on();
    }
}

bool board_button_pressed(void)
{
    return (GPIOA->IDR & BTN_GPIO_PIN) == 0U;
}

uint32_t millis(void)
{
    return ms_ticks;
}

#ifdef BOOT_BUILD
void board_delay_ms(uint32_t ms)
{
    uint32_t start = ms_ticks;

    while ((uint32_t)(ms_ticks - start) < ms)
    {
        __NOP();
    }
}

void led_blink(uint32_t count, uint32_t ms)
{
    for (uint32_t i = 0; i < count; i++)
    {
        led_on();
        board_delay_ms(ms);
        led_off();
        board_delay_ms(ms);
    }
}
#endif /* BOOT_BUILD */

void board_init(void)
{
    clock_init();

    gpio_init();
    led_init();

    __enable_irq();
}