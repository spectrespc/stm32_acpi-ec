/* tools/bootloader_tool/i2c_transport.h
 *
 * Абстракция I2C-транспорта хоста: Linux i2c-dev (RV1106/RPi) и
 * USB-I2C мост CP2112 (он же i2c-dev, но с задержками и повторами).
 */

#ifndef I2C_TRANSPORT_H
#define I2C_TRANSPORT_H

#include <stddef.h>
#include <stdint.h>

/* Задержка между записью и чтением (CP2112 — медленный USB-I2C мост). */
#define I2C_WRITE_READ_DELAY_US 100000  /* 100 мс */

/* Количество попыток чтения ответа. */
#define I2C_READ_RETRIES        5

/* Открыть I2C-устройство и выставить 7-битный адрес slave.
 * Возврат: fd >= 0 при успехе, -1 при ошибке. */
int i2c_transport_open(const char *device, uint8_t addr);

/* Записать wbuf, выдержать паузу и прочитать ответ в rbuf.
 * Возврат: 0 при успехе, -1 при ошибке. */
int i2c_transport_write_read(int fd,
                             const uint8_t *wbuf, size_t wlen,
                             uint8_t *rbuf, size_t rlen);

/* Записать buf без чтения ответа (например, CMD_RESET). */
int i2c_transport_write_only(int fd, const uint8_t *buf, size_t len);

#endif /* I2C_TRANSPORT_H */