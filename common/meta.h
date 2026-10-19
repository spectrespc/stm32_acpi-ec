/* common/meta.h
 *
 * Метаданные приложения (протокол v2, один слот) и ping-pong
 * с двумя страницами в конце Flash.
 *
 * ВАЖНО: структура MetaConfig является бинарным контрактом между
 * загрузчиком и приложением. Раньше она дублировалась в двух
 * заголовках и совпадала лишь «по договорённости»; теперь это
 * единственное определение, а размер и смещения проверяются
 * _Static_assert ниже.
 */

#ifndef META_H
#define META_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "board.h"

#define META_MAGIC 0x424F4F54UL /* 'BOOT' */

/* Флаги состояния (поле flags) */
#define META_FLAG_VALID   (1U << 0) /* образ прошёл проверку */
#define META_FLAG_PENDING (1U << 1) /* образ ожидает подтверждения запуска */

typedef struct
{
    uint32_t magic;
    uint32_t seq; /* счётчик записи для ping-pong страниц */

    uint32_t img_size; /* размер образа, байт */
    uint32_t img_crc;  /* CRC32 образа */
    uint32_t version;  /* версия прошивки */

    uint16_t boot_attempts;
    uint16_t flags; /* META_FLAG_* */

    uint16_t crc16; /* CRC16-CCITT всего выше, до этого поля */
    uint16_t reserved;
} MetaConfig;

_Static_assert(sizeof(MetaConfig) == 28, "MetaConfig must be 28 bytes");
_Static_assert(offsetof(MetaConfig, img_size) == 8, "MetaConfig.img_size offset");
_Static_assert(offsetof(MetaConfig, flags) == 22, "MetaConfig.flags offset");
_Static_assert(offsetof(MetaConfig, crc16) == 24, "MetaConfig.crc16 offset");

/* Актуальные метаданные загрузчика (загружаются meta_load()). */
extern MetaConfig meta;

/* Дописать magic и CRC16 (после заполнения полей). */
void meta_finalize(MetaConfig *cfg);

/* Прочитать валидную страницу с большим seq.
 * page_out — индекс страницы (0/1) или -1, если метаданных нет. */
bool meta_read_current(MetaConfig *cfg, int *page_out);

/* Записать cfg в страницу, противоположную current_page (ping-pong). */
bool meta_write_other(const MetaConfig *cfg, int current_page);

/* --- API загрузчика --- */
bool meta_load(void);
bool meta_save(MetaConfig *cfg);

/* --- API приложения --- */
/* Подтверждение загрузки: снимает флаг PENDING. */
bool boot_confirm(void);

#endif /* META_H */
