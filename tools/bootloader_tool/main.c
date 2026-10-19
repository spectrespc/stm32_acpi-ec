/* tools/bootloader_tool/main.c
 *
 * Унифицированная утилита прошивки устройств проекта.
 *
 * Цели:
 *   ec    — STM32F103, контроллер батареи (ACPI EC), адрес I2C 0x42.
 *           Прошивка через I2C-протокол загрузчика (протокол v2, один слот).
 *   motor — STM8S103F3P6, эмулятор шаговых двигателей.
 *           Прошивка через внешнюю утилиту stm8flash (ST-Link v2).
 *
 * Использование:
 *   bootloader_tool ec <i2c-dev> info
 *   bootloader_tool ec <i2c-dev> status
 *   bootloader_tool ec <i2c-dev> flash <file.bin> <version>
 *   bootloader_tool ec <i2c-dev> enter-update
 *   bootloader_tool ec <i2c-dev> reset
 *   bootloader_tool motor <i2c-dev> flash <file.ihx> <ver>     (OTA по I2C)
 *   bootloader_tool motor stlink <file.ihx> [--verify]        (ST-Link)
 *
 * Протокол (цель ec):
 *   Кадр запроса:  [LEN_LO][LEN_HI][CMD][PAYLOAD...]
 *   Кадр ответа:   [LEN_LO][LEN_HI][STATUS][PAYLOAD...]
 *   LEN = количество байт после него (STATUS + PAYLOAD).
 */

#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "crc_host.h"
#include "i2c_transport.h"

/* ========================================================================
 * Константы протокола (протокол v2, один слот приложения)
 * ======================================================================== */

#define I2C_ADDR7      0x42
#define I2C_ADDR_MOTOR 0x39 /* STM8 эмулятор моторов                   */

#define CMD_GET_INFO      0x01
#define CMD_BEGIN_UPDATE  0x02
#define CMD_WRITE_DATA    0x03
#define CMD_VERIFY_UPDATE 0x04
#define CMD_COMMIT        0x05
#define CMD_GET_STATUS    0x07
#define CMD_RESET         0x08
#define CMD_ENTER_UPDATE  0x0E

#define ST_OK        0x00
#define ST_ERR_CMD   0x01
#define ST_ERR_PARAM 0x02
#define ST_ERR_STATE 0x03
#define ST_ERR_FLASH 0x05
#define ST_ERR_CRC   0x06
#define ST_ERR_SEQ   0x07
#define ST_ERR_LEN   0x08

/* GET_INFO: payload 12 байт, одинаков для загрузчика и прошивки:
 *   [0]     device_type: 1 = загрузчик, 2 = приложение
 *   [1]     версия протокола (2)
 *   [2..5]  версия прошивки, LE32
 *   [6..9]  размер образа, LE32
 *   [10]    флаги: бит 0 = pending (ожидает подтверждения запуска)
 *   [11]    дополнительно (для приложения: число ячеек батареи)
 */
#define DEVICE_TYPE_BOOT       1 /* STM32 загрузчик  */
#define DEVICE_TYPE_APP        2 /* STM32 прошивка   */
#define DEVICE_TYPE_MOTOR_BOOT 3 /* STM8 загрузчик   */
#define DEVICE_TYPE_MOTOR_APP  4 /* STM8 прошивка    */
#define PROTO_VER              2
#define INFO_PAYLOAD_LEN       12
#define INFO_FLAG_PENDING      (1U << 0)

/*
 * Размер чанка данных при записи прошивки.
 * Кадр WRITE_DATA: 2(LEN) + 1(CMD) + 4(offset) + 1(len) + N(data) = 8+N.
 * CP2112 ограничивает запись ~61 байтом: 8 + 32 = 40 байт — безопасно.
 */
#define CHUNK_SIZE 32

/* Размер буфера чтения (максимальный ответ: 2 + 1 + 12 = 15 байт). */
#define I2C_READ_BUF_SIZE 64

/* ========================================================================
 * Вспомогательные функции
 * ======================================================================== */

static const char *status_str(uint8_t status)
{
    switch (status)
    {
    case ST_OK:
        return "OK";
    case ST_ERR_CMD:
        return "ERR_CMD";
    case ST_ERR_PARAM:
        return "ERR_PARAM";
    case ST_ERR_STATE:
        return "ERR_STATE";
    case ST_ERR_FLASH:
        return "ERR_FLASH";
    case ST_ERR_CRC:
        return "ERR_CRC";
    case ST_ERR_SEQ:
        return "ERR_SEQ";
    case ST_ERR_LEN:
        return "ERR_LEN";
    default:
        return "UNKNOWN";
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
    p[1] = (uint8_t)(v >> 8);
    p[2] = (uint8_t)(v >> 16);
    p[3] = (uint8_t)(v >> 24);
}

static uint16_t get_le16(const uint8_t *p)
{
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

static uint32_t get_le32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

/* ========================================================================
 * Команды протокола (цель ec)
 * ======================================================================== */

/*
 * GET_INFO — единый формат для загрузчика и прошивки.
 * По строке "device_type: N" Makefile-скрипты определяют,
 * кто сейчас отвечает на шине.
 */
static int cmd_info(int fd)
{
    uint8_t wbuf[3];
    uint8_t rbuf[I2C_READ_BUF_SIZE];

    put_le16(wbuf, 1);
    wbuf[2] = CMD_GET_INFO;

    if (i2c_transport_write_read(fd, wbuf, 3, rbuf, I2C_READ_BUF_SIZE) < 0)
    {
        return -1;
    }

    uint16_t length = get_le16(rbuf);
    uint8_t status = rbuf[2];

    if (status != ST_OK)
    {
        fprintf(stderr, "GET_INFO status: 0x%02X (%s)\n", status, status_str(status));
        return -1;
    }

    if (length != (uint16_t)(1 + INFO_PAYLOAD_LEN))
    {
        fprintf(stderr,
                "Unexpected GET_INFO payload length %u (expected %d).\n"
                "Устройство отвечает по старому протоколу.\n",
                length, INFO_PAYLOAD_LEN);
        return -1;
    }

    const uint8_t *p = &rbuf[3];

    uint8_t dev_type = p[0];
    uint8_t proto = p[1];
    uint32_t version = get_le32(&p[2]);
    uint32_t img_size = get_le32(&p[6]);
    uint8_t flags = p[10];
    uint8_t extra = p[11];

    printf("device_type: %u (%s)\n", dev_type,
           dev_type == DEVICE_TYPE_BOOT         ? "bootloader (STM32)"
           : dev_type == DEVICE_TYPE_APP        ? "application (STM32)"
           : dev_type == DEVICE_TYPE_MOTOR_BOOT ? "bootloader (STM8)"
           : dev_type == DEVICE_TYPE_MOTOR_APP  ? "application (STM8)"
                                                : "unknown");
    printf("proto      : %u\n", proto);
    printf("version    : 0x%08X\n", version);
    printf("img_size   : %u bytes (%.2f KB)\n", img_size, (float)img_size / 1024.0f);
    printf("flags      : 0x%02X (pending: %s)\n", flags,
           (flags & INFO_FLAG_PENDING) ? "yes" : "no");

    if (dev_type == DEVICE_TYPE_APP)
    {
        printf("cells      : %u\n", extra);
    }
    else if (dev_type == DEVICE_TYPE_MOTOR_APP)
    {
        printf("motor_flags: 0x%02X\n", extra);
    }

    return 0;
}

static int cmd_status(int fd)
{
    uint8_t wbuf[3];
    uint8_t rbuf[I2C_READ_BUF_SIZE];

    put_le16(wbuf, 1);
    wbuf[2] = CMD_GET_STATUS;

    if (i2c_transport_write_read(fd, wbuf, 3, rbuf, I2C_READ_BUF_SIZE) < 0)
    {
        return -1;
    }

    uint16_t length = get_le16(rbuf);
    uint8_t status = rbuf[2];

    printf("Response length: %u, status: 0x%02X (%s)\n", length, status, status_str(status));

    if (status != ST_OK || length < 7)
    {
        return -1;
    }

    const uint8_t *p = &rbuf[3];

    uint8_t last_status = p[0];
    uint8_t upd_state = p[1];
    uint32_t upd_offset = get_le32(&p[3]);

    printf("\n=== Update Status ===\n");
    printf("Last status   : 0x%02X (%s)\n", last_status, status_str(last_status));
    printf("Update state  : %u (%s)\n", upd_state,
           upd_state == 0   ? "IDLE"
           : upd_state == 1 ? "ACTIVE"
           : upd_state == 2 ? "VERIFIED"
                            : "UNKNOWN");
    printf("Update offset : %u bytes\n", upd_offset);

    return 0;
}

static uint8_t *read_file(const char *path, uint32_t *size)
{
    FILE *f = fopen(path, "rb");
    if (!f)
    {
        fprintf(stderr, "Cannot open file: %s\n", path);
        return NULL;
    }

    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (len <= 0)
    {
        fprintf(stderr, "File is empty\n");
        fclose(f);
        return NULL;
    }

    uint8_t *buf = malloc((size_t)len);
    if (!buf)
    {
        fprintf(stderr, "Out of memory\n");
        fclose(f);
        return NULL;
    }

    if (fread(buf, 1, (size_t)len, f) != (size_t)len)
    {
        fprintf(stderr, "Read error\n");
        free(buf);
        fclose(f);
        return NULL;
    }

    fclose(f);
    *size = (uint32_t)len;
    return buf;
}

static int cmd_flash_blob(int fd, uint8_t *data, uint32_t file_size, uint32_t version,
                          const char *file_path)
{
    uint32_t crc = crc32_sw(data, file_size);

    printf("File    : %s\n", file_path);
    printf("Size    : %u bytes (%.2f KB)\n", file_size, (float)file_size / 1024.0f);
    printf("CRC32   : 0x%08X\n", crc);
    printf("Version : 0x%08X\n\n", version);

    uint8_t wbuf[260];
    uint8_t rbuf[I2C_READ_BUF_SIZE];
    uint8_t status;

    /* ---- BEGIN_UPDATE: len(4) + crc(4) + version(4) ---- */
    put_le16(wbuf, 13);
    wbuf[2] = CMD_BEGIN_UPDATE;
    put_le32(&wbuf[3], file_size);
    put_le32(&wbuf[7], crc);
    put_le32(&wbuf[11], version);

    printf("Sending BEGIN_UPDATE...\n");
    if (i2c_transport_write_read(fd, wbuf, 15, rbuf, I2C_READ_BUF_SIZE) < 0)
    {
        return -1;
    }

    status = rbuf[2];
    printf("Status: 0x%02X (%s)\n", status, status_str(status));

    if (status != ST_OK)
    {
        return -1;
    }

    /* ---- WRITE_DATA ---- */
    uint32_t offset = 0;
    uint32_t total_chunks = (file_size + CHUNK_SIZE - 1) / CHUNK_SIZE;

    printf("Writing %u chunks (chunk size: %u bytes)...\n", total_chunks, CHUNK_SIZE);

    for (uint32_t chunk = 0; chunk < total_chunks; chunk++)
    {
        uint32_t chunk_len = file_size - offset;
        if (chunk_len > CHUNK_SIZE)
        {
            chunk_len = CHUNK_SIZE;
        }

        put_le16(wbuf, (uint16_t)(6 + chunk_len));
        wbuf[2] = CMD_WRITE_DATA;
        put_le32(&wbuf[3], offset);
        wbuf[7] = (uint8_t)chunk_len;
        memcpy(&wbuf[8], &data[offset], chunk_len);

        if (i2c_transport_write_read(fd, wbuf, 8 + chunk_len, rbuf, I2C_READ_BUF_SIZE) < 0)
        {
            return -1;
        }

        status = rbuf[2];
        if (status != ST_OK)
        {
            fprintf(stderr, "\nWrite failed at offset %u: 0x%02X (%s)\n", offset, status,
                    status_str(status));
            return -1;
        }

        offset += chunk_len;

        printf("\rProgress: %u / %u bytes (%.1f%%)", offset, file_size,
               (float)offset / (float)file_size * 100.0f);
        fflush(stdout);
    }

    printf("\n");

    /* ---- VERIFY_UPDATE ---- */
    put_le16(wbuf, 1);
    wbuf[2] = CMD_VERIFY_UPDATE;

    printf("Verifying...\n");
    if (i2c_transport_write_read(fd, wbuf, 3, rbuf, I2C_READ_BUF_SIZE) < 0)
    {
        return -1;
    }

    status = rbuf[2];
    printf("Verify status: 0x%02X (%s)\n", status, status_str(status));

    if (status != ST_OK)
    {
        return -1;
    }

    /* ---- COMMIT: единственный слот всегда активируется ---- */
    put_le16(wbuf, 1);
    wbuf[2] = CMD_COMMIT;

    printf("Committing...\n");
    if (i2c_transport_write_read(fd, wbuf, 3, rbuf, I2C_READ_BUF_SIZE) < 0)
    {
        return -1;
    }

    status = rbuf[2];
    printf("Commit status: 0x%02X (%s)\n", status, status_str(status));

    if (status != ST_OK)
    {
        return -1;
    }

    printf("\nFirmware flashed successfully!\n");
    return 0;
}

/* EC-путь: линейный .bin, дальше общий код */
static int cmd_flash(int fd, const char *file_path, uint32_t version)
{
    uint32_t size;
    uint8_t *data = read_file(file_path, &size);
    if (!data)
    {
        return -1;
    }
    int rc = cmd_flash_blob(fd, data, size, version, file_path);
    free(data);
    return rc;
}

static int cmd_enter_update(int fd)
{
    uint8_t wbuf[3];
    uint8_t rbuf[I2C_READ_BUF_SIZE];

    put_le16(wbuf, 1);
    wbuf[2] = CMD_ENTER_UPDATE;

    printf("Отправка команды входа в режим обновления...\n");

    if (i2c_transport_write_read(fd, wbuf, 3, rbuf, I2C_READ_BUF_SIZE) < 0)
    {
        /* Прошивка могла перезагрузиться до отправки ответа */
        printf("Ответ не получен (возможно, устройство уже перезагружается)\n");
    }
    else
    {
        uint8_t status = rbuf[2];
        printf("Статус: 0x%02X (%s)\n", status, status_str(status));
        if (status != ST_OK)
        {
            return -1;
        }
    }

    /* Ждём, пока устройство перезагрузится в режим обновления */
    printf("Ожидание перезагрузки в режим обновления (3 сек)...\n");
    usleep(3000000);

    return 0;
}

static int cmd_reset(int fd)
{
    uint8_t wbuf[3];

    put_le16(wbuf, 1);
    wbuf[2] = CMD_RESET;

    printf("Sending RESET command...\n");

    if (i2c_transport_write_only(fd, wbuf, 3) < 0)
    {
        return -1;
    }

    printf("Reset sent.\n");
    return 0;
}

/* ========================================================================
 * Цель motor: STM8S103F3P6
 *   — через I2C (тот же протокол v2, адрес 0x39): info/status/flash/
 *     enter-update/reset — flash принимает .ihx (SDCC) или .bin;
 *   — через ST-Link (stm8flash) для первичной прошивки: motor stlink.
 * ======================================================================== */

/* Образ приложения STM8 всегда начинается с базового адреса слота */
#define MOTOR_APP_BASE 0x9200u

/*
 * Разбор Intel HEX (.ihx, вывод SDCC/ASIX) в линейный образ.
 * Пробелы заполняются 0x00 — значением СТЁРТОЙ Flash у STM8S!
 * Возвращает данные (malloc) или NULL; base/size — границы образа.
 */
static uint8_t *read_ihx(const char *path, uint32_t *base, uint32_t *size)
{
    FILE *f = fopen(path, "r");
    if (!f)
    {
        fprintf(stderr, "Cannot open file: %s\n", path);
        return NULL;
    }

    uint32_t lo = 0xFFFFFFFFu, hi = 0;
    uint32_t seg_base = 0; /* для расширенных записей 04 */
    char line[1024];
    uint8_t *tmp = NULL; /* второй проход упрощён: два прохода */

    for (int pass = 0; pass < 2; pass++)
    {
        rewind(f);
        if (pass == 1)
        {
            if (lo == 0xFFFFFFFFu || hi == 0 || hi < lo)
            {
                fprintf(stderr, "Пустой или битый .ihx: %s\n", path);
                return NULL;
            }
            uint32_t n = hi - lo + 1;
            tmp = malloc(n);
            if (!tmp)
            {
                fclose(f);
                return NULL;
            }
            memset(tmp, 0x00, n); /* стёртое состояние STM8 = 0x00 */
            seg_base = 0;
        }

        while (fgets(line, sizeof(line), f))
        {
            if (line[0] != ':')
                continue;
            int ln = (int)strtol(line + 1, NULL, 16);
            int addr = (int)strtol(line + 3, NULL, 16);
            int typ = (int)strtol(line + 7, NULL, 16);
            if (typ == 5)
                break;
            if (typ != 0 && typ != 1 && typ != 2 && typ != 4)
                continue;
            uint32_t a;
            if (typ == 0)
            {
                a = seg_base + (uint32_t)addr;
                for (int i = 0; i < ln; i++)
                {
                    uint32_t v = (uint32_t)strtol(line + 9 + 2 * i, NULL, 16);
                    if (pass == 0)
                    {
                        if (a + i < lo)
                            lo = a + i;
                        if (a + i > hi)
                            hi = a + i;
                    }
                    else
                    {
                        tmp[a + i - lo] = (uint8_t)v;
                    }
                }
            }
            else if (typ == 2)
            { /* extended segment address */
                seg_base = ((uint32_t)strtol(line + 9, NULL, 16)) << 4;
            }
            else if (typ == 4)
            { /* extended linear address */
                seg_base = ((uint32_t)strtol(line + 9, NULL, 16)) << 16;
            }
        }
    }
    fclose(f);
    *base = lo;
    *size = hi - lo + 1;
    return tmp;
}

/*
 * I2C-прошивка STM8: принимает .ihx (преобразует в линейный образ слота)
 * или .bin (уже линейный образ, начинающийся с APP_BASE).
 */
static int cmd_flash_motor(int fd, const char *file_path, uint32_t version)
{
    uint8_t *blob;
    uint32_t size;

    if (strstr(file_path, ".ihx"))
    {
        uint32_t base;
        blob = read_ihx(file_path, &base, &size);
        if (!blob)
            return -1;
        if (base != MOTOR_APP_BASE)
        {
            fprintf(stderr,
                    "Образ .ihx начинается с 0x%04X, а слот приложения — 0x%04X.\n"
                    "Пересоберите приложение: make (ld: HOME=0x9200).\n",
                    base, MOTOR_APP_BASE);
            free(blob);
            return -1;
        }
        printf("Образ .ihx: база 0x%04X, %u байт\n", base, size);
    }
    else
    {
        blob = read_file(file_path, &size);
        if (!blob)
            return -1;
    }

    int rc = cmd_flash_blob(fd, blob, size, version, file_path);
    free(blob);
    return rc;
}

/* ========================================================================
 * Цель motor (ST-Link), сохранена как 'motor stlink'
 * ======================================================================== */

static int motor_flash_stlink(const char *file_path, bool verify)
{
    const char *stm8flash_path = getenv("STM8FLASH");
    if (!stm8flash_path || !stm8flash_path[0])
    {
        stm8flash_path = "stm8flash";
    }

    /* -w записывает образ; при необходимости отдельно проверяем через -v. */
    pid_t pid = fork();
    if (pid < 0)
    {
        fprintf(stderr, "fork: %s\n", strerror(errno));
        return -1;
    }

    if (pid == 0)
    {
        execlp(stm8flash_path, stm8flash_path, "-c", "stlinkv2", "-p", "stm8s103f3", "-w",
               file_path, (char *)NULL);
        fprintf(stderr,
                "Не удалось запустить %s: %s\n"
                "Установите stm8flash или задайте путь в переменной "
                "окружения STM8FLASH.\n",
                stm8flash_path, strerror(errno));
        _exit(127);
    }

    int wstatus = 0;
    if (waitpid(pid, &wstatus, 0) < 0)
    {
        fprintf(stderr, "waitpid: %s\n", strerror(errno));
        return -1;
    }

    if (!WIFEXITED(wstatus) || WEXITSTATUS(wstatus) != 0)
    {
        fprintf(stderr, "stm8flash завершился с ошибкой\n");
        return -1;
    }

    printf("STM8: образ %s записан\n", file_path);

    if (!verify)
    {
        return 0;
    }

    pid = fork();
    if (pid < 0)
    {
        fprintf(stderr, "fork: %s\n", strerror(errno));
        return -1;
    }

    if (pid == 0)
    {
        execlp(stm8flash_path, stm8flash_path, "-c", "stlinkv2", "-p", "stm8s103f3", "-v",
               file_path, (char *)NULL);
        fprintf(stderr, "Не удалось запустить %s: %s\n", stm8flash_path, strerror(errno));
        _exit(127);
    }

    wstatus = 0;
    if (waitpid(pid, &wstatus, 0) < 0)
    {
        fprintf(stderr, "waitpid: %s\n", strerror(errno));
        return -1;
    }

    if (!WIFEXITED(wstatus) || WEXITSTATUS(wstatus) != 0)
    {
        fprintf(stderr, "STM8: верификация завершилась с ошибкой\n");
        return -1;
    }

    printf("STM8: верификация пройдена\n");
    return 0;
}

/* ========================================================================
 * Использование
 * ======================================================================== */

static void print_usage(const char *prog)
{
    printf("Унифицированная утилита прошивки\n\n"
           "Использование:\n"
           "  %s ec <i2c-dev> info                       - информация (загрузчик или прошивка)\n"
           "  %s ec <i2c-dev> status                     - статус обновления (загрузчик)\n"
           "  %s ec <i2c-dev> flash <file.bin> <version> - прошивка STM32 через загрузчик\n"
           "  %s ec <i2c-dev> enter-update               - перевод прошивки в режим обновления\n"
           "  %s ec <i2c-dev> reset                      - перезагрузка устройства\n"
           "  %s motor <i2c-dev> info|status                - состояние STM8 по I2C (addr 0x39)\n"
           "  %s motor <i2c-dev> flash <file.ihx> <ver>    - OTA-прошивка STM8 по I2C\n"
           "  %s motor <i2c-dev> enter-update|reset        - вход в загрузчик / сброс\n"
           "  %s motor stlink <file.ihx> [--verify]        - первичная прошивка (ST-Link)\n"
           "\nПримеры:\n"
           "  %s ec /dev/i2c-7 info\n"
           "  %s ec /dev/i2c-7 flash build/app.bin 0x00010000\n"
           "  %s motor /dev/i2c-7 flash build/motor_i2c_slave.ihx 0x00010000\n"
           "  %s motor stlink build/motor_i2c_slave.ihx --verify\n",
           prog, prog, prog, prog, prog, prog, prog, prog, prog, prog, prog, prog, prog);
}

/* ========================================================================
 * main
 * ======================================================================== */

int main(int argc, char **argv)
{
    if (argc < 2)
    {
        print_usage(argv[0]);
        return 1;
    }

    const char *target = argv[1];

    /* ---------------- Цель: motor (STM8) ---------------- */
    if (strcmp(target, "motor") == 0)
    {
        /* Первичная прошивка по ST-Link (SWIM) */
        if (argc >= 4 && strcmp(argv[2], "stlink") == 0)
        {
            bool verify = (argc >= 5 && strcmp(argv[4], "--verify") == 0);
            return motor_flash_stlink(argv[3], verify) == 0 ? 0 : 1;
        }

        /* Обмен по I2C: тот же протокол v2, адрес 0x39 */
        if (argc >= 4)
        {
            const char *device = argv[2];
            const char *command = argv[3];

            int fd = i2c_transport_open(device, I2C_ADDR_MOTOR);
            if (fd < 0)
            {
                return 1;
            }

            int ret = -1;

            if (strcmp(command, "info") == 0)
            {
                ret = cmd_info(fd);
            }
            else if (strcmp(command, "status") == 0)
            {
                ret = cmd_status(fd);
            }
            else if (strcmp(command, "flash") == 0)
            {
                if (argc < 6)
                {
                    fprintf(stderr,
                            "Usage: %s motor <device> flash <file.ihx|file.bin> <version>\n",
                            argv[0]);
                }
                else
                {
                    uint32_t version = (uint32_t)strtoul(argv[5], NULL, 0);
                    ret = cmd_flash_motor(fd, argv[4], version);
                }
            }
            else if (strcmp(command, "enter-update") == 0)
            {
                ret = cmd_enter_update(fd);
            }
            else if (strcmp(command, "reset") == 0)
            {
                ret = cmd_reset(fd);
            }
            else
            {
                fprintf(stderr, "Unknown motor command: %s\n\n", command);
                print_usage(argv[0]);
            }

            close(fd);
            return (ret == 0) ? 0 : 1;
        }

        print_usage(argv[0]);
        return 1;
    }

    /* ---------------- Цель: ec (STM32, I2C) ---------------- */
    if (strcmp(target, "ec") == 0)
    {
        if (argc < 4)
        {
            print_usage(argv[0]);
            return 1;
        }

        const char *device = argv[2];
        const char *command = argv[3];

        int fd = i2c_transport_open(device, I2C_ADDR7);
        if (fd < 0)
        {
            return 1;
        }

        int ret = -1;

        if (strcmp(command, "info") == 0)
        {
            ret = cmd_info(fd);
        }
        else if (strcmp(command, "status") == 0)
        {
            ret = cmd_status(fd);
        }
        else if (strcmp(command, "flash") == 0)
        {
            if (argc < 6)
            {
                fprintf(stderr, "Usage: %s ec <device> flash <file> <version>\n", argv[0]);
            }
            else
            {
                uint32_t version = (uint32_t)strtoul(argv[5], NULL, 0);
                ret = cmd_flash(fd, argv[4], version);
            }
        }
        else if (strcmp(command, "enter-update") == 0)
        {
            ret = cmd_enter_update(fd);
        }
        else if (strcmp(command, "reset") == 0)
        {
            ret = cmd_reset(fd);
        }
        else
        {
            fprintf(stderr, "Unknown ec command: %s\n\n", command);
            print_usage(argv[0]);
        }

        close(fd);
        return (ret == 0) ? 0 : 1;
    }

    fprintf(stderr, "Unknown target: %s\n\n", target);
    print_usage(argv[0]);
    return 1;
}
