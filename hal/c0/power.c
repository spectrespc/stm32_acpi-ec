/* hal/c0/power.c
 *
 * STM32C031F6P6: питание хоста, backup-регистры и Stop.
 *
 * Пины (docs/hardware.md):
 *   PA1 PWR_EN     push-pull, внешний pull-up (fail-on)
 *   PA2 PWR_GOOD   input, pull-down
 *   PA3 HOST_WAKE  open-drain, High-Z при PWR_EN=0
 *   PA0 BTN_PWR    input pull-up, EXTI0
 *
 * В STM32C031 нет LPTIM, а RTC не содержит wakeup-таймера, поэтому
 * таймерного пробуждения из Stop нет: EC просыпается кнопкой (EXTI)
 * или адресом I2C (WUPEN). Идле-таймаут 3 с считается по SysTick, пока
 * EC не спит.
 */

#include "power.h"
#include "board.h"
#include "i2c.h"
#include "i2c_debug.h"
#include "system.h"

#define PWR_BKP_COUNT 4U

static bool s_pwr_en;
static uint8_t s_reset_cause;
static uint8_t s_wake_src;
static uint8_t s_wake_count;
static volatile bool s_button_event;
static volatile bool s_in_stop;

/* ========================================================================
 * Пины
 * ======================================================================== */

static void pwr_en_pin_init(void)
{
    /* Сначала высокий уровень в ODR, затем выход — без глитча. */
    GPIOA->BSRR = PWR_EN_GPIO_PIN;
    GPIOA->OTYPER &= ~PWR_EN_GPIO_PIN;
    GPIOA->PUPDR &= ~(3U << (1U * 2U));
    GPIOA->MODER = (GPIOA->MODER & ~(3U << (1U * 2U))) | (1U << (1U * 2U));
}

static void host_wake_set(bool assert_low)
{
    if (assert_low)
    {
        GPIOA->BSRR = HOST_WAKE_GPIO_PIN << 16U;
        GPIOA->MODER = (GPIOA->MODER & ~(3U << (3U * 2U))) | (1U << (3U * 2U));
    }
    else
    {
        /* High-Z: линии не должно быть, пока хост обесточен. */
        GPIOA->BSRR = HOST_WAKE_GPIO_PIN << 16U;
        GPIOA->MODER &= ~(3U << (3U * 2U));
    }
}

static uint8_t read_reset_cause(void)
{
    uint32_t csr2 = RCC->CSR2;
    uint8_t cause = 0;

    if (csr2 & RCC_CSR2_PWRRSTF)
    {
        cause |= RESET_CAUSE_POR;
    }

    if (csr2 & RCC_CSR2_PINRSTF)
    {
        cause |= RESET_CAUSE_PIN;
    }

    if (csr2 & RCC_CSR2_SFTRSTF)
    {
        cause |= RESET_CAUSE_SFT;
    }

    if (csr2 & RCC_CSR2_IWDGRSTF)
    {
        cause |= RESET_CAUSE_IWDG;
    }

    if (csr2 & RCC_CSR2_WWDGRSTF)
    {
        cause |= RESET_CAUSE_WWDG;
    }

    /* BORRSTF на STM32C031 отсутствует; бит RESET_CAUSE_BOR всегда 0. */
    if (csr2 & RCC_CSR2_LPWRRSTF)
    {
        cause |= RESET_CAUSE_LPWR;
    }

    /* Флаги сбрасываются записью 1. */
    RCC->CSR2 = csr2 & (RCC_CSR2_PWRRSTF | RCC_CSR2_PINRSTF | RCC_CSR2_SFTRSTF | RCC_CSR2_IWDGRSTF |
                        RCC_CSR2_WWDGRSTF | RCC_CSR2_LPWRRSTF | RCC_CSR2_OBLRSTF);

    return cause;
}

/* ========================================================================
 * API
 * ======================================================================== */

bool power_supported(void)
{
    return true;
}

void power_init(void)
{
    RCC->IOPENR |= RCC_IOPENR_GPIOAEN;
    RCC->APBENR1 |= RCC_APBENR1_PWREN;

    pwr_en_pin_init();
    host_wake_set(false);

    /* PWR_GOOD — вход с pull-down. */
    GPIOA->PUPDR = (GPIOA->PUPDR & ~(3U << (2U * 2U))) | (2U << (2U * 2U));
    GPIOA->MODER &= ~(3U << (2U * 2U));

    EXTI->FPR1 = EXTI_FPR1_FPIF0;
    EXTI->RPR1 = EXTI_RPR1_RPIF0;

    s_reset_cause = read_reset_cause();
    s_wake_src = WAKE_SRC_RESET;
    s_wake_count = 0;
    s_pwr_en = true; /* PA1 уже высокий (см. pwr_en_pin_init) */
}

void power_set_pwr_en(bool on)
{
    if (!on)
    {
        /* Сначала отпускаем HOST_WAKE, только потом рубим питание. */
        host_wake_set(false);
    }

    if (on)
    {
        GPIOA->BSRR = PWR_EN_GPIO_PIN;
    }
    else
    {
        GPIOA->BSRR = PWR_EN_GPIO_PIN << 16U;
    }

    s_pwr_en = on;
}

bool power_get_pwr_en(void)
{
    return s_pwr_en;
}

bool power_get_pwr_good(void)
{
    return (GPIOA->IDR & PWR_GOOD_GPIO_PIN) != 0U;
}

bool power_lse_ok(void)
{
    /* LSE на C031 не используется: нет LPTIM, RTC не включён. */
    return false;
}

void power_set_host_wake(bool assert_low)
{
    host_wake_set(assert_low);
}

void power_pulse_host_wake(void)
{
    host_wake_set(true);

    uint32_t start = millis();

    while ((uint32_t)(millis() - start) < 150U)
    {
        ;
    }

    host_wake_set(false);
}

uint8_t power_backup_read(uint8_t index)
{
    return (uint8_t)(power_backup_read32(index) & 0xFFU);
}

void power_backup_write(uint8_t index, uint8_t value)
{
    power_backup_write32(index, (uint32_t)value);
}

uint32_t power_backup_read32(uint8_t index)
{
    if (index >= PWR_BKP_COUNT)
    {
        return 0;
    }

    volatile const uint32_t *bkp = &PWR->BKP0R;

    return bkp[index];
}

void power_backup_write32(uint8_t index, uint32_t value)
{
    if (index >= PWR_BKP_COUNT)
    {
        return;
    }

    volatile uint32_t *bkp = &PWR->BKP0R;

    /* BKP[15:0]: старшие 16 бит аппаратно не реализованы. */
    bkp[index] = value & 0xFFFFU;
}

uint8_t power_reset_cause(void)
{
    return s_reset_cause;
}

#ifdef DEBUG_BUILD
/* Длительность окна с замаскированными прерываниями до входа в WFI.
 * SysTick тикает с периодом 1 мс (EC_SYSCLK_HZ/1000), поэтому VAL даёт
 * разрешение EC_SYSCLK_HZ/1e6 тактов на микросекунду. */
static uint32_t systick_delta_us(uint32_t start_val)
{
    uint32_t now = SysTick->VAL;
    uint32_t reload = SysTick->LOAD + 1U;
    uint32_t elapsed;

    if (start_val >= now)
    {
        elapsed = start_val - now;
    }
    else
    {
        elapsed = (start_val + reload) - now;
    }

    return elapsed / (EC_SYSCLK_HZ / 1000000UL);
}
#endif

bool power_enter_stop(uint32_t timeout_ms)
{
    /* На C031 таймерного пробуждения нет; timeout_ms сохранён в API,
     * чтобы приложение не зависело от семейства. */
    (void)timeout_ms;

    /* Запрос уже принят I2C, идёт транзакция или кнопка удерживается —
     * спать нельзя: иначе кадр останется без ответа, а нажатие
     * потеряется. TXIE/ BUSY означают незавершённое чтение master'ом. */
    if (i2c_process_request || board_button_pressed())
    {
        return false;
    }

    if ((I2C1->CR1 & I2C_CR1_PE) && ((I2C1->ISR & I2C_ISR_BUSY) || (I2C1->CR1 & I2C_CR1_TXIE)))
    {
        return false;
    }

    (void)i2c_take_wake_flag();
    s_button_event = false;

    EXTI->FPR1 = EXTI_FPR1_FPIF0;
    EXTI->RPR1 = EXTI_RPR1_RPIF0;

    /* Событие могло прийти между проверкой и очисткой pending. */
    if (i2c_process_request || board_button_pressed() || i2c_take_wake_flag())
    {
        return false;
    }

    s_in_stop = true;

    PWR->CR1 = (PWR->CR1 & ~PWR_CR1_LPMS);
    SCB->SCR |= SCB_SCR_SLEEPDEEP_Msk;
    __DSB();

    /* Незавершённая сделка не должна уйти в Stop: отдаём кадр
     * следующей итерации main, не трогая периферию. */
    if ((I2C1->CR1 & I2C_CR1_PE) &&
        ((I2C1->ISR & (I2C_ISR_ADDR | I2C_ISR_TXIS)) || (I2C1->CR1 & I2C_CR1_TXIE)))
    {
        SCB->SCR &= ~SCB_SCR_SLEEPDEEP_Msk;
        s_in_stop = false;
        return false;
    }

#ifdef DEBUG_BUILD
    uint32_t mask_start = SysTick->VAL;
#endif

    /* Закрываем окно «проверили → WFI»: маскируем прерывания и ещё
     * раз смотрим входы/pending. WFI всё равно проснётся от любого
     * разрешённого прерывания, даже при PRIMASK=1. */
    __disable_irq();

    if (i2c_process_request || board_button_pressed() || i2c_take_wake_flag() ||
        (EXTI->FPR1 & EXTI_FPR1_FPIF0) || (EXTI->RPR1 & EXTI_RPR1_RPIF0))
    {
        __enable_irq();
        SCB->SCR &= ~SCB_SCR_SLEEPDEEP_Msk;
        s_in_stop = false;
#ifdef DEBUG_BUILD
        i2c_debug_note_irq_masked(systick_delta_us(mask_start));
#endif
        return false;
    }

    /* ADDR/TXIS могли появиться уже под маской — снимаем маску и
     * отдаём кадр ISR/следующей итерации вместо Stop. */
    if ((I2C1->CR1 & I2C_CR1_PE) &&
        ((I2C1->ISR & (I2C_ISR_ADDR | I2C_ISR_TXIS)) || (I2C1->CR1 & I2C_CR1_TXIE)))
    {
        __enable_irq();
        SCB->SCR &= ~SCB_SCR_SLEEPDEEP_Msk;
        s_in_stop = false;
        return false;
    }

#ifdef DEBUG_BUILD
    i2c_debug_note_irq_masked(systick_delta_us(mask_start));
#endif

    /* SysTick должен быть остановлен, иначе его pending-прерывание
     * немедленно выводит ядро из Stop (WFI возвращается сразу). */
    system_stop_systick();
    SCB->ICSR = SCB_ICSR_PENDSTCLR_Msk;

    i2c_wakeup_arm();

    __WFI();
    __enable_irq();

    i2c_wakeup_disarm();

    SCB->SCR &= ~SCB_SCR_SLEEPDEEP_Msk;
    s_in_stop = false;

    clock_restore_after_stop();

    uint8_t src = WAKE_SRC_NONE;

    if (i2c_take_wake_flag())
    {
        src = WAKE_SRC_I2C;
    }
    else if (s_button_event)
    {
        src = WAKE_SRC_BUTTON;
    }

    s_button_event = false;
    s_wake_src = src;

    if (src != WAKE_SRC_NONE)
    {
        s_wake_count++;
    }

    /* Возвращаем I2C только если хост под питанием (иначе back-power
     * через линии). Если периферия уже обслуживает кадр, который
     * разбудил EC (ADDR/TXIS/TXIE), CR1 не трогаем: сброс TXIE оборвал
     * бы чтение, и slave удержал бы SCL до watchdog-а. */
    if (s_pwr_en)
    {
        if ((I2C1->CR1 & I2C_CR1_PE) == 0U)
        {
            i2c_recover();
        }
        else if (((I2C1->ISR & (I2C_ISR_ADDR | I2C_ISR_TXIS)) == 0U) &&
                 ((I2C1->CR1 & I2C_CR1_TXIE) == 0U))
        {
            i2c_set_enabled(true);
        }
    }
    else
    {
        i2c_set_enabled(false);
    }

    return true;
}

uint8_t power_last_wake_src(void)
{
    return s_wake_src;
}

uint8_t power_wake_count(void)
{
    return s_wake_count;
}

void power_clear_wake_src(void)
{
    s_wake_src = WAKE_SRC_NONE;
}

void EXTI0_1_IRQHandler(void)
{
    if (EXTI->FPR1 & EXTI_FPR1_FPIF0)
    {
        EXTI->FPR1 = EXTI_FPR1_FPIF0;

        if (s_in_stop)
        {
            s_button_event = true;
        }
    }

    if (EXTI->RPR1 & EXTI_RPR1_RPIF0)
    {
        EXTI->RPR1 = EXTI_RPR1_RPIF0;
    }
}