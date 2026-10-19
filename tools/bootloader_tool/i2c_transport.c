/* tools/bootloader_tool/i2c_transport.c */

#define _GNU_SOURCE

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/i2c.h>
#include <linux/i2c-dev.h>

#include "i2c_transport.h"

int i2c_transport_open(const char *device, uint8_t addr)
{
    int fd = open(device, O_RDWR);
    if (fd < 0)
    {
        fprintf(stderr, "Cannot open %s: %s\n", device, strerror(errno));
        return -1;
    }

    if (ioctl(fd, I2C_SLAVE, addr) < 0)
    {
        fprintf(stderr, "Cannot set I2C address 0x%02X: %s\n", addr, strerror(errno));
        close(fd);
        return -1;
    }

    return fd;
}

int i2c_transport_write_read(int fd, const uint8_t *wbuf, size_t wlen, uint8_t *rbuf, size_t rlen)
{
    /* Записываем команду */
    ssize_t written = write(fd, wbuf, wlen);
    if (written != (ssize_t)wlen)
    {
        fprintf(stderr, "I2C write failed (%zd/%zu): %s\n", written, wlen, strerror(errno));
        return -1;
    }

    /* Ждём, пока устройство обработает команду (CP2112 медленный). */
    usleep(I2C_WRITE_READ_DELAY_US);

    /* Читаем ответ с несколькими попытками */
    for (int attempt = 0; attempt < I2C_READ_RETRIES; attempt++)
    {
        ssize_t readed = read(fd, rbuf, rlen);

        if (readed >= 3)
        {
            return 0;
        }

        if (attempt < I2C_READ_RETRIES - 1)
        {
            usleep(I2C_WRITE_READ_DELAY_US);
        }
    }

    fprintf(stderr, "I2C read failed after %d retries\n", I2C_READ_RETRIES);
    return -1;
}

int i2c_transport_write_only(int fd, const uint8_t *buf, size_t len)
{
    ssize_t written = write(fd, buf, len);
    if (written != (ssize_t)len)
    {
        fprintf(stderr, "I2C write failed (%zd/%zu): %s\n", written, len, strerror(errno));
        return -1;
    }

    return 0;
}
