/* hal/i2c.c
 *
 * I2C1 slave, адрес I2C_ADDR7 (0x42), PCLK1 = 8 МГц, 100 кГц.
 *
 * ВАЖНО: обработчики прерываний читают/пишут регистры SR1/SR2/DR
 * напрямую (опрос флагов внутри IRQ), как в исходных Makefile-версиях.
 * Такое поведение сохранено намеренно — оно влияет на тайминги OTA.
 */

#include <string.h>

#include "board.h"
#include "i2c.h"

volatile uint8_t rx_buf[I2C_RX_BUF_SIZE];
volatile uint16_t rx_count;
volatile uint8_t i2c_process_request;

static volatile uint8_t rx_frame_done;
static uint8_t tx_buf[I2C_TX_BUF_SIZE];
static volatile uint16_t tx_len;
static volatile uint16_t tx_pos;

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

void i2c_init(void)
{
    RCC->APB2ENR |= RCC_APB2ENR_IOPBEN | RCC_APB2ENR_AFIOEN;
    RCC->APB1ENR |= RCC_APB1ENR_I2C1EN;

    /* PB6 SCL, PB7 SDA: alternate function open-drain, 2 MHz */
    GPIOB->CRL &= ~(0xFFU << 24);
    GPIOB->CRL |= (0xEEU << 24);

    I2C1->CR1 |= I2C_CR1_SWRST;
    I2C1->CR1 &= ~I2C_CR1_SWRST;

    /* PCLK1 = 8 MHz, I2C standard mode 100 kHz */
    I2C1->CR2 = 8;
    I2C1->CCR = 40;
    I2C1->TRISE = 9;

    /* Бит 14 OAR1 по RM0008 должен всегда удерживаться в 1 */
    I2C1->OAR1 = (uint16_t)(0x4000U | (I2C_ADDR7 << 1) | 0x01U);
    I2C1->OAR2 = 0;

    /* PE необходимо установить до ACK: при PE=0 запись бита ACK
     * аппаратно игнорируется (подтверждено на STM32F103), из-за чего
     * slave не подтверждал собственный адрес. */
    I2C1->CR1 |= I2C_CR1_PE;
    I2C1->CR1 |= I2C_CR1_ACK;

    I2C1->CR2 |= I2C_CR2_ITEVTEN | I2C_CR2_ITBUFEN | I2C_CR2_ITERREN;

    NVIC_SetPriority(I2C1_EV_IRQn, 0);
    NVIC_SetPriority(I2C1_ER_IRQn, 0);

    NVIC_EnableIRQ(I2C1_EV_IRQn);
    NVIC_EnableIRQ(I2C1_ER_IRQn);
}

void i2c_pause(void)
{
#ifdef BOOT_BUILD
    NVIC_DisableIRQ(I2C1_EV_IRQn);
    NVIC_DisableIRQ(I2C1_ER_IRQn);

    I2C1->CR1 &= ~I2C_CR1_PE;
#else
    /* В приложении I2C во время записи Flash ещё не инициализирован. */
#endif
}

void i2c_resume(void)
{
#ifdef BOOT_BUILD
    i2c_init();
#endif
}

void I2C1_EV_IRQHandler(void)
{
    uint32_t sr1 = I2C1->SR1;

    if (sr1 & I2C_SR1_ADDR)
    {
        uint32_t sr2 = I2C1->SR2;

        if (sr2 & I2C_SR2_TRA)
        {
            /* Master reads from us */
            tx_pos = 0;
        }
        else
        {
            /* Master writes to us */
            rx_count = 0;
            rx_frame_done = 0;
        }
    }

    if (sr1 & I2C_SR1_RXNE)
    {
        uint8_t b = (uint8_t)I2C1->DR;

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

    if ((sr1 & (I2C_SR1_TXE | I2C_SR1_BTF)) && (I2C1->SR2 & I2C_SR2_TRA))
    {
        if (tx_pos < tx_len)
        {
            I2C1->DR = tx_buf[tx_pos++];
        }
        else
        {
            I2C1->DR = 0xFF;
        }
    }

    if (sr1 & I2C_SR1_STOPF)
    {
        /* Clear STOPF: read SR1 then write CR1 */
        (void)I2C1->SR1;
        I2C1->CR1 |= I2C_CR1_PE;

        if (rx_frame_done)
        {
            i2c_process_request = 1;
            rx_frame_done = 0;
        }
    }
}

void I2C1_ER_IRQHandler(void)
{
    I2C1->SR1 &= ~(I2C_SR1_BERR | I2C_SR1_ARLO | I2C_SR1_OVR | I2C_SR1_AF);

    I2C1->CR1 |= I2C_CR1_ACK;
}
