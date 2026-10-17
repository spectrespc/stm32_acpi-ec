/* common/meta.c
 *
 * Ping-pong чтение/запись метаданных. Единый код для загрузчика и
 * приложения; различается только семантика обёрток верхнего уровня
 * (meta_save / boot_confirm). Пауза I2C в приложении — no-op, поэтому
 * вызовы i2c_pause()/i2c_resume() безопасны для обоих образов.
 */

#include <string.h>

#include "meta.h"
#include "crc.h"
#include "flash.h"
#include "i2c.h"

MetaConfig meta;

static bool meta_read_page(uint32_t addr, MetaConfig *cfg)
{
    flash_read(addr, cfg, sizeof(*cfg));

    if (cfg->magic != META_MAGIC) {
        return false;
    }

    uint16_t crc = crc16_ccitt(
        (const uint8_t *)cfg,
        offsetof(MetaConfig, crc16)
    );

    return crc == cfg->crc16;
}

bool meta_read_current(MetaConfig *cfg, int *page_out)
{
    MetaConfig m0;
    MetaConfig m1;

    bool v0 = meta_read_page(META_PAGE0_ADDR, &m0);
    bool v1 = meta_read_page(META_PAGE1_ADDR, &m1);

    if (v0 && (!v1 || m0.seq >= m1.seq)) {
        *cfg = m0;
        if (page_out) {
            *page_out = 0;
        }
        return true;
    }

    if (v1) {
        *cfg = m1;
        if (page_out) {
            *page_out = 1;
        }
        return true;
    }

    if (page_out) {
        *page_out = -1;
    }
    return false;
}

void meta_finalize(MetaConfig *cfg)
{
    cfg->magic = META_MAGIC;
    cfg->crc16 = crc16_ccitt(
        (const uint8_t *)cfg,
        offsetof(MetaConfig, crc16)
    );
}

static bool meta_write_page(uint32_t addr, const MetaConfig *cfg)
{
    flash_unlock();
    flash_erase_page_raw(addr);

    const uint16_t *p = (const uint16_t *)cfg;
    bool ok = true;

    for (uint32_t i = 0; i < sizeof(MetaConfig) / 2U; i++) {
        if (flash_program_halfword_raw(addr + i * 2U, p[i]) != 0) {
            ok = false;
            break;
        }
    }

    flash_lock();

    return ok;
}

bool meta_write_other(const MetaConfig *cfg, int current_page)
{
    int target = (current_page == 0) ? 1 : 0;

    uint32_t target_addr =
        (target == 0) ? META_PAGE0_ADDR : META_PAGE1_ADDR;

    i2c_pause();

    bool ok = meta_write_page(target_addr, cfg);

    i2c_resume();

    return ok;
}

bool meta_load(void)
{
    int page;

    if (meta_read_current(&meta, &page)) {
        return true;
    }

    memset(&meta, 0, sizeof(meta));
    meta.magic = META_MAGIC;

    return false;
}

bool meta_save(MetaConfig *cfg)
{
    MetaConfig cur;
    int page;

    if (meta_read_current(&cur, &page)) {
        cfg->seq = cur.seq + 1U;
    } else {
        page = -1;
        cfg->seq = 1U;
    }

    meta_finalize(cfg);

    bool ok = meta_write_other(cfg, page);

    if (ok) {
        meta = *cfg;
    }

    return ok;
}

bool boot_confirm(void)
{
    MetaConfig cfg;
    int page;

    if (!meta_read_current(&cfg, &page)) {
        return false;
    }

    /* Если флаг PENDING не установлен — подтверждать нечего */
    if ((cfg.flags & META_FLAG_PENDING) == 0U) {
        return true;
    }

    cfg.flags &= (uint16_t)~META_FLAG_PENDING;
    cfg.boot_attempts = 0;
    cfg.seq++;
    meta_finalize(&cfg);

    return meta_write_other(&cfg, page);
}