/* hal/c0/i2c.c
 *
 * I2C1 slave, адрес 0x42, STM32C031F6P6 (TSSOP20):
 *   PB6 — I2C1_SCL (AF6, open-drain)
 *   PB7 — I2C1_SDA (AF6, open-drain)
 *
 * Такт ядра I2C1 — HSIKER 48 МГц (RCC->CCIPR.I2C1SEL = 10, HSIKERON=1):
 * это требование пробуждения из Stop по совпадению адреса (с PCLK
 * периферия в Stop не тактируется). WUPEN
 * включается только перед WFI: в рабочем режиме на C011 он держит SCL
 * низко и убивает шину.
 * TIMINGR рассчитан на Fast-mode 400 кГц, поэтому slave обслуживает и
 * 400 кГц (базовый pro-max), и 100 кГц (активная плата sc230ai).
 *
 * В отличие от F103 (два вектора EV/ER), у STM32C0 один I2C1_IRQn.
 *
 * Освобождение линии. Slave-transmitter держит SCL, пока TXDR не
 * заполнен. Если мастер исчез посреди чтения (перезагрузка RV1106),
 * STOPF/NACKF уже не придут, поэтому линию отпускает таймер:
 *   - аппаратный tTIMEOUT (TIMEOUTR, ~15 мс) поднимает I2C_ISR_TIMEOUT;
 *   - программный bus-watchdog (app/src/bus_watchdog.c) страхует, если
 *     аппаратный таймер не сработал.
 * Оба пути зовут i2c_recover(): PE=0/1 снимает stretching.
 */

#include <string.h>

#include "board.h"
#include "i2c.h"
#include "i2c_debug.h"
#include "power.h"
#include "system.h"

/* 48 МГц, Fast-mode 400 кГц: PRESC=0, SCLDEL=15, SDADEL=2,
 * SCLH=28 (604 нс), SCLL=62 (1.31 мкс). */
#define I2C_TIMINGR_400K 0x00F21C3EU

/* SCL-low timeout: tTIMEOUT = (TIMEOUTA + 1) * 2048 * tI2CCLK.
 * При I2CCLK = 48 МГц и TIMEOUTA = 350: 351 * 2048 / 48e6 = 14.98 мс.
 * Порог выбран ниже типичного таймаута хоста (rk3x-i2c, ~20-30 мс),
 * чтобы EC отпускал линию раньше, чем мастер объявит ошибку.
 * TIDLE = 0 — детектируется именно низкий уровень SCL. */
#define I2C_TIMEOUTR_SCL_LOW_15MS                                                                  \
    (((uint32_t)350U << I2C_TIMEOUTR_TIMEOUTA_Pos) | I2C_TIMEOUTR_TIMOUTEN)

#define I2C_AF_I2C1 6U

volatile uint8_t rx_buf[I2C_RX_BUF_SIZE];
volatile uint16_t rx_count;
volatile uint8_t i2c_process_request;

static volatile uint8_t rx_frame_done;
static volatile uint8_t i2c_wake_flag;
static uint8_t tx_buf[I2C_TX_BUF_SIZE];
static volatile uint16_t tx_len;
static volatile uint16_t tx_pos;

static bool s_i2c_running;
static bool s_i2c_paused;
static bool s_host_enabled;
static uint32_t s_pause_started_ms;

#ifdef DEBUG_BUILD
volatile i2c_debug_t dbg_i2c;
#endif

static void i2c_gpio_init(void)
{
    RCC->IOPENR |= RCC_IOPENR_GPIOBEN;

    /* PB6/PB7: alternate function, open-drain. */
    GPIOB->OTYPER |= (1U << 6) | (1U << 7);
    GPIOB->PUPDR &= ~((3U << (6U * 2U)) | (3U << (7U * 2U)));
    GPIOB->MODER = (GPIOB->MODER & ~((3U << (6U * 2U)) | (3U << (7U * 2U)))) | (2U << (6U * 2U)) |
                   (2U << (7U * 2U));

    GPIOB->AFR[0] = (GPIOB->AFR[0] & ~(0xFFUL << 24)) | ((uint32_t)I2C_AF_I2C1 << 24) |
                    ((uint32_t)I2C_AF_I2C1 << 28);
}

void i2c_set_response(uint8_t status, const uint8_t *payload, uint16_t plen)
{
    if (plen > (I2C_TX_BUF_SIZE - 3U))
    {
        plen = I2C_TX_BUF_SIZE - 3U;
    }

    uint16_t total = (uint16_t)(1U + plen);

    tx_buf[0] = (uint8_t)(total & 0xFFU);
    tx_buf[1] = (uint8_t)(total >> 8);
    tx_buf[2] = status;

    if (plen > 0U)
    {
        memcpy(&tx_buf[3], payload, plen);
    }

    tx_len = (uint16_t)(2U + total);
    tx_pos = 0;
}

void i2c_set_status(uint8_t status)
{
    i2c_set_response(status, NULL, 0);
}

/* Конфигурационные регистры пишутся только при PE=0. */
static void i2c_config_regs(void)
{
    I2C1->CR1 &= ~I2C_CR1_PE;

    I2C1->TIMINGR = I2C_TIMINGR_400K;

    /* TIMEOUTA/TIDLE можно менять только при TIMOUTEN=0. */
    I2C1->TIMEOUTR = 0;
    I2C1->TIMEOUTR = I2C_TIMEOUTR_SCL_LOW_15MS;

    I2C1->OAR1 = ((uint32_t)I2C_ADDR7 << 1) | I2C_OAR1_OA1EN;

    /* Флаги мог оставить незавершённый кадр: после PE=0 они уже не
     * отражают состояние шины, поэтому чистим все известные. */
    I2C1->ICR = I2C_ICR_ADDRCF | I2C_ICR_NACKCF | I2C_ICR_STOPCF | I2C_ICR_BERRCF | I2C_ICR_ARLOCF |
                I2C_ICR_OVRCF | I2C_ICR_TIMOUTCF;
}

/* Переконфигурирует I2C после STOP/NACK/ошибки: PE=0 сбрасывает
 * TXDR/state machine (убирает застрявший 0xFF и clock stretching),
 * конфигурационные регистры записываются заново. */
static void i2c_hw_config(void)
{
    i2c_config_regs();

    I2C1->CR1 = I2C_CR1_PE | I2C_CR1_ADDRIE | I2C_CR1_NACKIE | I2C_CR1_STOPIE | I2C_CR1_RXIE |
                I2C_CR1_ERRIE;
}

void i2c_recover(void)
{
    if (!s_i2c_running)
    {
        return;
    }

    i2c_hw_config();
}

void i2c_get_bus_state(i2c_bus_state_t *state)
{
    if (state == NULL)
    {
        return;
    }

    state->enabled = (I2C1->CR1 & I2C_CR1_PE) != 0U;
    state->busy = (I2C1->ISR & I2C_ISR_BUSY) != 0U;
    state->tx_active = ((I2C1->CR1 & I2C_CR1_TXIE) != 0U) || ((I2C1->ISR & I2C_ISR_TXIS) != 0U);
    state->tx_pos = tx_pos;
    state->rx_count = rx_count;
}

bool i2c_bus_watchdog_supported(void)
{
    return true;
}

void i2c_wakeup_arm(void)
{
    /* WUPEN включает пробуждение из Stop по адресу, но в рабочем
     * режиме C011 он держит SCL низко (проверено на стенде), поэтому
     * выставляется только непосредственно перед WFI. */
    if (I2C1->CR1 & I2C_CR1_PE)
    {
        I2C1->CR1 |= I2C_CR1_WUPEN;
    }
}

void i2c_wakeup_disarm(void)
{
    I2C1->CR1 &= ~I2C_CR1_WUPEN;
}

void i2c_init(void)
{
    i2c_gpio_init();

    RCC->APBENR1 |= RCC_APBENR1_I2C1EN;

    i2c_hw_config();

    NVIC_SetPriority(I2C1_IRQn, 0);
    NVIC_EnableIRQ(I2C1_IRQn);

    s_i2c_running = true;
    s_i2c_paused = false;
    s_host_enabled = true;
}

void i2c_set_enabled(bool enabled)
{
    s_host_enabled = enabled;

    if (s_i2c_paused)
    {
        return;
    }

    if (enabled)
    {
        I2C1->CR1 |= I2C_CR1_PE;
        I2C1->CR1 &= ~I2C_CR1_TXIE;
        NVIC_EnableIRQ(I2C1_IRQn);
    }
    else
    {
        NVIC_DisableIRQ(I2C1_IRQn);
        I2C1->CR1 &= ~(I2C_CR1_PE | I2C_CR1_WUPEN);
        I2C1->ICR = I2C_ICR_ADDRCF | I2C_ICR_NACKCF | I2C_ICR_STOPCF | I2C_ICR_BERRCF |
                    I2C_ICR_ARLOCF | I2C_ICR_OVRCF | I2C_ICR_TIMOUTCF;
    }
}

void i2c_deinit(void)
{
    NVIC_DisableIRQ(I2C1_IRQn);
    I2C1->CR1 &= ~I2C_CR1_PE;
    RCC->APBENR1 &= ~RCC_APBENR1_I2C1EN;

    s_i2c_running = false;
    s_i2c_paused = false;
    s_host_enabled = false;
}

void i2c_pause(void)
{
    if (!s_i2c_running || s_i2c_paused)
    {
        return;
    }

    s_pause_started_ms = millis();

    /* PE=0 немедленно отпускает SCL/SDA, поэтому запись Flash не
     * удерживает линию даже посреди кадра. */
    NVIC_DisableIRQ(I2C1_IRQn);
    I2C1->CR1 &= ~(I2C_CR1_PE | I2C_CR1_WUPEN);
    s_i2c_paused = true;
}

void i2c_resume(void)
{
    if (!s_i2c_paused)
    {
        return;
    }

    i2c_debug_note_pause((uint32_t)(millis() - s_pause_started_ms));

    i2c_config_regs();

    if (s_host_enabled)
    {
        i2c_hw_config();
        NVIC_EnableIRQ(I2C1_IRQn);
    }

    s_i2c_paused = false;
}

bool i2c_take_wake_flag(void)
{
    bool seen = i2c_wake_flag != 0U;

    i2c_wake_flag = 0;
    return seen;
}

void I2C1_IRQHandler(void)
{
    uint32_t isr = I2C1->ISR;

    if (isr & I2C_ISR_ADDR)
    {
        i2c_wake_flag = 1;

        if (I2C1->ISR & I2C_ISR_DIR)
        {
            /* Master reads from us. TXIE включаем только здесь, чтобы
             * TXDR не был предзагружен 0xFF до адреса. */
            tx_pos = 0;

            if (i2c_debug_hold_scl_enabled())
            {
                /* 0x29: не заполняем TXDR и не включаем TXIE — шина
                 * остаётся в растяжке до таймерного освобождения. */
            }
            else
            {
                I2C1->CR1 |= I2C_CR1_TXIE;
            }

            i2c_debug_note_read_addr(isr, I2C1->CR1);
        }
        else
        {
            /* Master writes to us */
            rx_count = 0;
            rx_frame_done = 0;
            I2C1->CR1 &= ~I2C_CR1_TXIE;
        }

        I2C1->ICR = I2C_ICR_ADDRCF;
    }

    if (isr & I2C_ISR_RXNE)
    {
        uint8_t b = (uint8_t)I2C1->RXDR;

        if (rx_count < I2C_RX_BUF_SIZE)
        {
            rx_buf[rx_count++] = b;
        }

        if (rx_count >= 2U)
        {
            uint16_t expected = (uint16_t)rx_buf[0] | ((uint16_t)rx_buf[1] << 8);

            if (rx_count >= (uint16_t)(expected + 2U))
            {
                rx_frame_done = 1;
            }
        }
    }

    if (isr & I2C_ISR_TXIS)
    {
        i2c_debug_note_txis();

        if (tx_pos < tx_len)
        {
            I2C1->TXDR = tx_buf[tx_pos++];
        }
        else
        {
            I2C1->TXDR = 0xFF;
        }
    }

    if (isr & I2C_ISR_NACKF)
    {
        i2c_debug_note_nack();
        I2C1->CR1 &= ~I2C_CR1_TXIE;
        I2C1->ICR = I2C_ICR_NACKCF;
        i2c_hw_config();
    }

    if (isr & I2C_ISR_STOPF)
    {
        I2C1->CR1 &= ~I2C_CR1_TXIE;
        I2C1->ICR = I2C_ICR_STOPCF;

        if (rx_frame_done)
        {
            i2c_process_request = 1;
            rx_frame_done = 0;
        }

        /* Сбросить TXDR/state: иначе 0xFF-filler останется и станет
         * первым байтом следующего ответа. */
        i2c_hw_config();
    }

    if (isr & I2C_ISR_TIMEOUT)
    {
        /* Сработал tTIMEOUT (SCL прижат дольше 15 мс) или tLOW:EXT.
         * Мастер уже не доведёт кадр, поэтому обрываем приём и
         * отпускаем линию через PE=0/1. */
        i2c_debug_note_timeout();
        rx_count = 0;
        rx_frame_done = 0;
    }

    uint32_t errors = isr & (I2C_ISR_BERR | I2C_ISR_ARLO | I2C_ISR_OVR | I2C_ISR_TIMEOUT);
    uint32_t icr = 0;

    if (errors & I2C_ISR_BERR)
    {
        icr |= I2C_ICR_BERRCF;
        i2c_debug_note_error();
    }

    if (errors & I2C_ISR_ARLO)
    {
        icr |= I2C_ICR_ARLOCF;
        i2c_debug_note_error();
    }

    if (errors & I2C_ISR_OVR)
    {
        icr |= I2C_ICR_OVRCF;
        i2c_debug_note_error();
    }

    if (errors & I2C_ISR_TIMEOUT)
    {
        icr |= I2C_ICR_TIMOUTCF;
    }

    if (icr != 0U)
    {
        I2C1->ICR = icr;
        i2c_hw_config();
    }
}

/* ========================================================================
 * Отладочный слой (DEBUG_BUILD)
 * ======================================================================== */

#ifdef DEBUG_BUILD

static uint32_t debug_saturate(uint32_t value, uint32_t limit)
{
    return (value > limit) ? limit : value;
}

static void debug_mirror_backup(uint32_t isr, uint32_t cr1)
{
    power_backup_write32(I2C_DEBUG_BKP_STATE_INDEX, (isr & 0xFFFFU) | ((cr1 & 0xFFFFU) << 16U));

    uint32_t packed = debug_saturate(dbg_i2c.addr_events, 0xFU) |
                      (debug_saturate(dbg_i2c.read_addr_events, 0xFU) << 4U) |
                      (debug_saturate(dbg_i2c.txis_events, 0xFU) << 8U) |
                      (debug_saturate(dbg_i2c.bus_hang_events, 0xFU) << 12U) |
                      (debug_saturate(dbg_i2c.timeout_fires, 0xFU) << 16U) |
                      (debug_saturate(dbg_i2c.nack_fires, 0xFU) << 20U);

    if (dbg_i2c.hold_scl != 0U)
    {
        packed |= 1U << 24U;
    }

    if ((cr1 & I2C_CR1_PE) != 0U)
    {
        packed |= 1U << 25U;
    }

    if (s_i2c_paused)
    {
        packed |= 1U << 26U;
    }

    packed |= 1U << 27U; /* флаг «зеркало валидно» */
    packed |= 0xAU << 28U;

    power_backup_write32(I2C_DEBUG_BKP_FLAGS_INDEX, packed);
}

void i2c_debug_hold_scl(bool enable)
{
    dbg_i2c.hold_scl = enable ? 1U : 0U;
    debug_mirror_backup(I2C1->ISR, I2C1->CR1);
}

bool i2c_debug_hold_scl_enabled(void)
{
    return dbg_i2c.hold_scl != 0U;
}

void i2c_debug_note_read_addr(uint32_t isr, uint32_t cr1)
{
    dbg_i2c.addr_events++;
    dbg_i2c.read_addr_events++;
    dbg_i2c.isr_at_read_addr = (uint16_t)isr;
    dbg_i2c.cr1_at_read_addr = (uint16_t)cr1;
    debug_mirror_backup(isr, cr1);
}

void i2c_debug_note_txis(void)
{
    dbg_i2c.txis_events++;
}

void i2c_debug_note_timeout(void)
{
    dbg_i2c.timeout_fires++;
    debug_mirror_backup(I2C1->ISR, I2C1->CR1);
}

void i2c_debug_note_nack(void)
{
    dbg_i2c.nack_fires++;
}

void i2c_debug_note_error(void)
{
    dbg_i2c.err_fires++;
}

void i2c_debug_note_hang(uint32_t ms)
{
    uint32_t isr = I2C1->ISR;
    uint32_t cr1 = I2C1->CR1;

    dbg_i2c.bus_hang_events++;

    if (ms > dbg_i2c.bus_hang_ms_max)
    {
        dbg_i2c.bus_hang_ms_max = ms;
    }

    dbg_i2c.hang_isr = (uint16_t)isr;
    dbg_i2c.hang_cr1 = (uint16_t)cr1;
    debug_mirror_backup(isr, cr1);
}

void i2c_debug_note_irq_masked(uint32_t us)
{
    if (us > dbg_i2c.irq_masked_us_max)
    {
        dbg_i2c.irq_masked_us_max = us;
    }
}

void i2c_debug_note_pause(uint32_t ms)
{
    if (ms > dbg_i2c.paused_ms_max)
    {
        dbg_i2c.paused_ms_max = ms;
    }
}

static void put_le16(uint8_t *p, uint16_t v)
{
    p[0] = (uint8_t)(v & 0xFFU);
    p[1] = (uint8_t)(v >> 8);
}

static void put_le32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)(v & 0xFFU);
    p[1] = (uint8_t)((v >> 8) & 0xFFU);
    p[2] = (uint8_t)((v >> 16) & 0xFFU);
    p[3] = (uint8_t)((v >> 24) & 0xFFU);
}

uint8_t i2c_debug_snapshot(uint8_t *out, uint8_t cap)
{
    if ((out == NULL) || (cap < I2C_DEBUG_PAYLOAD_LEN))
    {
        return 0;
    }

    put_le32(&out[0], dbg_i2c.addr_events);
    put_le32(&out[4], dbg_i2c.read_addr_events);
    put_le32(&out[8], dbg_i2c.txis_events);
    put_le32(&out[12], dbg_i2c.stop_fires);
    put_le32(&out[16], dbg_i2c.nack_fires);
    put_le32(&out[20], dbg_i2c.err_fires);
    put_le32(&out[24], dbg_i2c.timeout_fires);
    put_le32(&out[28], dbg_i2c.bus_hang_events);
    put_le32(&out[32], dbg_i2c.bus_hang_ms_max);
    put_le32(&out[36], dbg_i2c.irq_masked_us_max);
    put_le32(&out[40], dbg_i2c.paused_ms_max);
    put_le16(&out[44], dbg_i2c.isr_at_read_addr);
    put_le16(&out[46], dbg_i2c.cr1_at_read_addr);
    put_le16(&out[48], dbg_i2c.hang_isr);
    put_le16(&out[50], dbg_i2c.hang_cr1);
    put_le32(&out[52], power_backup_read32(I2C_DEBUG_BKP_STATE_INDEX));
    put_le32(&out[56], power_backup_read32(I2C_DEBUG_BKP_FLAGS_INDEX));

    return I2C_DEBUG_PAYLOAD_LEN;
}

#endif /* DEBUG_BUILD */