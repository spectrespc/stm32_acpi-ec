/* hal/board.c
 *
 * Объединённая реализация для загрузчика и приложения.
 *
 * Отличия по целям (BOOT_BUILD):
 *   - загрузчик инициализирует пин питания host-устройства (PA1) и
 *     выдерживает паузу HOST_POWER_DELAY_MS;
 *   - загрузчик дополнительно ждёт переключения системного такта на HSI
 *     сам по себе и содержит led_blink().
 */

#include "board.h"

static volatile uint32_t ms_ticks;

void SysTick_Handler(void)
{
    ms_ticks++;
}

static void clock_init_hsi(void)
{
    RCC->CR |= RCC_CR_HSION;

    while ((RCC->CR & RCC_CR_HSIRDY) == 0U)
    {
        ;
    }

    RCC->CFGR = 0;

#ifndef BOOT_BUILD
    /* Дожидаемся фактического переключения системного такта на HSI
     * (SWS = 00) и только потом отключаем PLL/HSE — иначе при запуске
     * из окружения с активным PLL системный такт пропадёт. */
    while ((RCC->CFGR & RCC_CFGR_SWS) != 0U)
    {
        ;
    }
#endif

    RCC->CR &= ~(RCC_CR_PLLON | RCC_CR_CSSON | RCC_CR_HSEON);
}

#ifdef BOOT_BUILD
static void delay_init(void)
{
    SysTick_Config(8000000UL / 1000UL);
}

static void delay_ms(uint32_t ms)
{
    uint32_t start = ms_ticks;

    while ((uint32_t)(ms_ticks - start) < ms)
    {
        __NOP();
    }
}

static void host_power_init(void)
{
    LL_APB2_GRP1_EnableClock(HOST_PWR_GPIO_CLK);

    LL_GPIO_SetPinMode(HOST_PWR_GPIO_PORT, HOST_PWR_GPIO_PIN, LL_GPIO_MODE_OUTPUT);

    LL_GPIO_SetPinOutputType(HOST_PWR_GPIO_PORT, HOST_PWR_GPIO_PIN, LL_GPIO_OUTPUT_PUSHPULL);

    LL_GPIO_SetPinSpeed(HOST_PWR_GPIO_PORT, HOST_PWR_GPIO_PIN, LL_GPIO_SPEED_FREQ_LOW);

#if HOST_PWR_ACTIVE_HIGH
    LL_GPIO_SetOutputPin(HOST_PWR_GPIO_PORT, HOST_PWR_GPIO_PIN);
#else
    LL_GPIO_ResetOutputPin(HOST_PWR_GPIO_PORT, HOST_PWR_GPIO_PIN);
#endif
}
#endif /* BOOT_BUILD */

void led_init(void)
{
    LL_APB2_GRP1_EnableClock(LED_GPIO_CLK);

    LL_GPIO_SetPinMode(LED_GPIO_PORT, LED_GPIO_PIN, LL_GPIO_MODE_OUTPUT);

    LL_GPIO_SetPinOutputType(LED_GPIO_PORT, LED_GPIO_PIN, LL_GPIO_OUTPUT_PUSHPULL);

    LL_GPIO_SetPinSpeed(LED_GPIO_PORT, LED_GPIO_PIN, LL_GPIO_SPEED_FREQ_LOW);

    led_off();
}

void led_on(void)
{
#if LED_ACTIVE_HIGH
    LL_GPIO_SetOutputPin(LED_GPIO_PORT, LED_GPIO_PIN);
#else
    LL_GPIO_ResetOutputPin(LED_GPIO_PORT, LED_GPIO_PIN);
#endif
}

void led_off(void)
{
#if LED_ACTIVE_HIGH
    LL_GPIO_ResetOutputPin(LED_GPIO_PORT, LED_GPIO_PIN);
#else
    LL_GPIO_SetOutputPin(LED_GPIO_PORT, LED_GPIO_PIN);
#endif
}

void led_toggle(void)
{
    if (LL_GPIO_IsOutputPinSet(LED_GPIO_PORT, LED_GPIO_PIN))
    {
        led_off();
    }
    else
    {
        led_on();
    }
}

#ifdef BOOT_BUILD
void led_blink(uint32_t count, uint32_t ms)
{
    for (uint32_t i = 0; i < count; i++)
    {
        led_on();
        delay_ms(ms);
        led_off();
        delay_ms(ms);
    }
}
#endif /* BOOT_BUILD */

uint32_t millis(void)
{
    return ms_ticks;
}

void board_init(void)
{
    clock_init_hsi();

#ifdef BOOT_BUILD
    host_power_init();
#endif

    led_init();

    __enable_irq();

#ifdef BOOT_BUILD
    delay_init();
    delay_ms(HOST_POWER_DELAY_MS);
#else
    SysTick_Config(8000000UL / 1000UL);
#endif
}
