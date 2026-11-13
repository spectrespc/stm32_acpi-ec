/* hal/include/i2c_debug.h
 *
 * Диагностика I2C-slave: счётчики событий, снимки регистров и
 * форензика «линия прижата» для сборок с -DDEBUG_BUILD.
 *
 * Зачем: STM32 держит SCL, пока не заполнен TXDR в slave-transmitter.
 * Если мастер (RV1106) исчезает посреди чтения, штатные события шины
 * (STOPF/NACKF) уже не приходят, и состояние видно только по таймеру.
 * Счётчики позволяют измерить, как часто это происходит и за сколько
 * миллисекунд прошивка сама отпускает линию.
 *
 * Все функции ниже в обычной (не DEBUG_BUILD) сборке — no-op, поэтому
 * вызывающий код не обязан обрастать #ifdef.
 */

#ifndef I2C_DEBUG_H
#define I2C_DEBUG_H

#include <stdint.h>
#include <stdbool.h>

/* Размер ответа CMD_GET_DEBUG (0x28). Layout — docs/protocol.md. */
#define I2C_DEBUG_PAYLOAD_LEN 60U

#ifdef DEBUG_BUILD

/* Зеркало в PWR->BKP2R/BKP3R: backup-домен переживает NRST, поэтому
 * состояние «до нажатия кнопки» можно прочитать после сброса.
 * BKP2R = ISR[15:0] | CR1[15:0] << 16;
 * BKP3R = упакованные счётчики и флаги (см. i2c_debug_mirror_backup). */
#define I2C_DEBUG_BKP_STATE_INDEX 2U
#define I2C_DEBUG_BKP_FLAGS_INDEX 3U

typedef struct
{
    volatile uint32_t addr_events;
    volatile uint32_t read_addr_events;
    volatile uint32_t txis_events;
    volatile uint32_t stop_fires;
    volatile uint32_t nack_fires;
    volatile uint32_t err_fires;
    volatile uint32_t timeout_fires;
    volatile uint32_t bus_hang_events;
    volatile uint32_t bus_hang_ms_max;
    volatile uint32_t irq_masked_us_max;
    volatile uint32_t paused_ms_max;
    volatile uint16_t isr_at_read_addr;
    volatile uint16_t cr1_at_read_addr;
    volatile uint16_t hang_isr;
    volatile uint16_t hang_cr1;
    volatile uint8_t hold_scl;
} i2c_debug_t;

extern volatile i2c_debug_t dbg_i2c;

/* 0x29: следующий ADDR(read) обслужен без заполнения TXDR — эталонное
 * зависание для проверки таймерного освобождения линии. */
void i2c_debug_hold_scl(bool enable);
bool i2c_debug_hold_scl_enabled(void);

/* Отметки из ISR/таймеров. */
void i2c_debug_note_read_addr(uint32_t isr, uint32_t cr1);
void i2c_debug_note_txis(void);
void i2c_debug_note_timeout(void);
void i2c_debug_note_nack(void);
void i2c_debug_note_error(void);
void i2c_debug_note_hang(uint32_t ms);
void i2c_debug_note_irq_masked(uint32_t us);
void i2c_debug_note_pause(uint32_t ms);

/* Собрать ответ CMD_GET_DEBUG; возвращает число байт. */
uint8_t i2c_debug_snapshot(uint8_t *out, uint8_t cap);

#else /* !DEBUG_BUILD */

static inline void i2c_debug_hold_scl(bool enable)
{
    (void)enable;
}

static inline bool i2c_debug_hold_scl_enabled(void)
{
    return false;
}

static inline void i2c_debug_note_read_addr(uint32_t isr, uint32_t cr1)
{
    (void)isr;
    (void)cr1;
}

static inline void i2c_debug_note_txis(void)
{
}

static inline void i2c_debug_note_timeout(void)
{
}

static inline void i2c_debug_note_nack(void)
{
}

static inline void i2c_debug_note_error(void)
{
}

static inline void i2c_debug_note_hang(uint32_t ms)
{
    (void)ms;
}

static inline void i2c_debug_note_irq_masked(uint32_t us)
{
    (void)us;
}

static inline void i2c_debug_note_pause(uint32_t ms)
{
    (void)ms;
}

static inline uint8_t i2c_debug_snapshot(uint8_t *out, uint8_t cap)
{
    (void)out;
    (void)cap;
    return 0;
}

#endif /* DEBUG_BUILD */

#endif /* I2C_DEBUG_H */