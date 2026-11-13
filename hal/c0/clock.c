/* hal/c0/clock.c
 *
 * Тактирование STM32C031F6P6.
 *
 * После сброса и после выхода из Stop SYSCLK = HSI / HSIDIV = 48/4 =
 * 12 МГц. HSIKERON=1 оставляет такт HSI для периферии в Stop — это
 * требование пробуждения I2C по адресу (WUPEN). I2C1 тактируется от
 * HSIKER (RCC->CCIPR.I2C1SEL = 10), а не от PCLK/SYSCLK: с PCLK
 * периферия в Stop не тактируется и адрес не будит ядро.
 *
 * LSE/LPTIM на STM32C031 отсутствуют, RTC не включается. Таймауты
 * считаются по SysTick, пока EC не в Stop; в Stop EC просыпается
 * только кнопкой (EXTI) или адресом I2C.
 */

#include "board.h"

#define HSI_HSIDIV_DIV4 2U /* HSIDIV: 000=/1, 001=/2, 010=/4, 011=/8 */

/* Вызывается из startup до копирования .data — только регистры. */
void SystemInit(void)
{
}

static void clock_hsi_setup(void)
{
    RCC->CR |= RCC_CR_HSION;

    while ((RCC->CR & RCC_CR_HSIRDY) == 0U)
    {
        ;
    }

    /* SYSCLK = HSI (SW=0) и явный делитель /4. */
    RCC->CFGR &= ~RCC_CFGR_SW;
    RCC->CR = (RCC->CR & ~RCC_CR_HSIDIV) | (HSI_HSIDIV_DIV4 << RCC_CR_HSIDIV_Pos);

    while ((RCC->CFGR & RCC_CFGR_SWS) != RCC_CFGR_SWS_HSI)
    {
        ;
    }

    /* HSI продолжает работать в Stop для I2C (kernel clock).
     * HSIKERDIV=0: kernel clock 48 МГц (используется I2C1). */
    RCC->CR |= RCC_CR_HSIKERON;
    RCC->CR &= ~RCC_CR_HSIKERDIV;

    /* 12 МГц — 0 wait states. */
    RCC->AHBENR |= RCC_AHBENR_FLASHEN;
    FLASH->ACR &= ~FLASH_ACR_LATENCY;

    /* RTC не используется: LSE/RTC выключены. */
    RCC->CSR1 &= ~(RCC_CSR1_RTCSEL | RCC_CSR1_RTCEN);

    /* I2C1 kernel clock = HSIKER (значение 10). Значение 00 (PCLK)
     * в Stop не тактирует I2C, поэтому WUPEN не работает, а TIMINGR
     * и tTIMEOUT считаются от 48 МГц, а не от 12 МГц. */
    RCC->CCIPR = (RCC->CCIPR & ~RCC_CCIPR_I2C1SEL) | RCC_CCIPR_I2C1SEL_1;
}

void clock_init(void)
{
    clock_hsi_setup();

    SysTick_Config(EC_SYSCLK_HZ / 1000UL);
}

void clock_restore_after_stop(void)
{
    /* После Stop SYSCLK остаётся HSI/4, но SysTick нужно взвести
     * заново: в Stop он остановлен. */
    clock_hsi_setup();
    SysTick_Config(EC_SYSCLK_HZ / 1000UL);
}