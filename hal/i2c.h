/* hal/i2c.h
 *
 * I2C-slave загрузчика и приложения. Пауза/возобновление нужны
 * загрузчику: во время стирания/записи Flash обслуживание I2C
 * невозможно, поэтому его приостанавливают на время операции.
 */

#ifndef I2C_H
#define I2C_H

#include <stdint.h>

#define I2C_RX_BUF_SIZE 260U
#define I2C_TX_BUF_SIZE 260U

extern volatile uint8_t  rx_buf[I2C_RX_BUF_SIZE];
extern volatile uint16_t rx_count;
extern volatile uint8_t  i2c_process_request;

void i2c_init(void);

/* Пауза/возобновление I2C. В приложении это no-op. */
void i2c_pause(void);
void i2c_resume(void);

void i2c_set_response(uint8_t status, const uint8_t *payload, uint16_t plen);
void i2c_set_status(uint8_t status);

#endif /* I2C_H */