/* tests/mocks/i2c.h
 *
 * Хостовая подмена hal/i2c.h: пауза/возобновление — no-op.
 */

#ifndef I2C_H
#define I2C_H

void i2c_pause(void);
void i2c_resume(void);

#endif /* I2C_H */
