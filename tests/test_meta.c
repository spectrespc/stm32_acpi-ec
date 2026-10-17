/* tests/test_meta.c
 *
 * Проверка ping-pong метаданных на RAM-модели Flash.
 */

#include "unity.h"
#include "meta.h"
#include "flash.h"

static void setup(void)
{
    mock_flash_reset();
}

static void test_no_meta_present(void)
{
    setup();

    MetaConfig m;
    int page = 42;

    TEST_ASSERT_FALSE(meta_read_current(&m, &page));
    TEST_ASSERT_EQUAL_INT(-1, page);
}

static void test_save_and_read_roundtrip(void)
{
    setup();

    MetaConfig w = {0};
    w.img_size = 1234U;
    w.img_crc  = 0xDEADBEEFUL;
    w.version  = 0x00010000UL;
    w.flags    = META_FLAG_VALID;

    TEST_ASSERT_TRUE(meta_save(&w));

    MetaConfig r;
    int page = -1;
    TEST_ASSERT_TRUE(meta_read_current(&r, &page));
    TEST_ASSERT_EQUAL_INT(0, page);
    TEST_ASSERT_EQUAL_UINT32(1234U, r.img_size);
    TEST_ASSERT_EQUAL_HEX32(0xDEADBEEFUL, r.img_crc);
    TEST_ASSERT_EQUAL_UINT32(0x00010000UL, r.version);
    TEST_ASSERT_EQUAL_UINT32(1U, r.seq);
}

static void test_pingpong_increments_sequence(void)
{
    setup();

    MetaConfig w = {0};
    w.flags = META_FLAG_VALID;

    TEST_ASSERT_TRUE(meta_save(&w));   /* page 0, seq 1 */

    w.img_size = 2048U;
    TEST_ASSERT_TRUE(meta_save(&w));   /* page 1, seq 2 */

    MetaConfig r;
    int page = -1;
    TEST_ASSERT_TRUE(meta_read_current(&r, &page));
    TEST_ASSERT_EQUAL_INT(1, page);
    TEST_ASSERT_EQUAL_UINT32(2U, r.seq);
    TEST_ASSERT_EQUAL_UINT32(2048U, r.img_size);
}

static void test_higher_sequence_wins(void)
{
    setup();

    MetaConfig w = {0};
    w.flags = META_FLAG_VALID;
    meta_save(&w);              /* page 0, seq 1 */

    w.version = 0x00020000UL;
    meta_save(&w);              /* page 1, seq 2 */

    MetaConfig r;
    int page = -1;
    TEST_ASSERT_TRUE(meta_read_current(&r, &page));
    TEST_ASSERT_EQUAL_INT(1, page);
    TEST_ASSERT_EQUAL_UINT32(0x00020000UL, r.version);
}

static void test_corrupt_page_falls_back(void)
{
    setup();

    MetaConfig w = {0};
    w.flags = META_FLAG_VALID;
    w.version = 0x00010000UL;
    meta_save(&w);              /* page 0, seq 1 */

    w.version = 0x00020000UL;
    meta_save(&w);              /* page 1, seq 2 */

    /* Портим magic в странице 1 — должна остаться валидной страница 0. */
    mock_flash_set(META_PAGE1_ADDR, 0x00);

    MetaConfig r;
    int page = -1;
    TEST_ASSERT_TRUE(meta_read_current(&r, &page));
    TEST_ASSERT_EQUAL_INT(0, page);
    TEST_ASSERT_EQUAL_UINT32(0x00010000UL, r.version);
}

static void test_boot_confirm_clears_pending(void)
{
    setup();

    MetaConfig w = {0};
    w.flags = META_FLAG_VALID | META_FLAG_PENDING;
    w.img_size = 4096U;
    meta_save(&w);

    TEST_ASSERT_TRUE(boot_confirm());

    MetaConfig r;
    int page = -1;
    TEST_ASSERT_TRUE(meta_read_current(&r, &page));
    TEST_ASSERT_EQUAL_INT(1, page);
    TEST_ASSERT_TRUE((r.flags & META_FLAG_PENDING) == 0U);
    TEST_ASSERT_TRUE((r.flags & META_FLAG_VALID) != 0U);
    TEST_ASSERT_EQUAL_UINT32(2U, r.seq);
}

static void test_boot_confirm_noop_without_pending(void)
{
    setup();

    MetaConfig w = {0};
    w.flags = META_FLAG_VALID;
    meta_save(&w);              /* seq 1, page 0 */

    TEST_ASSERT_TRUE(boot_confirm());  /* подтверждать нечего */

    MetaConfig r;
    int page = -1;
    TEST_ASSERT_TRUE(meta_read_current(&r, &page));
    TEST_ASSERT_EQUAL_UINT32(1U, r.seq);   /* страница не переписана */
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_no_meta_present);
    RUN_TEST(test_save_and_read_roundtrip);
    RUN_TEST(test_pingpong_increments_sequence);
    RUN_TEST(test_higher_sequence_wins);
    RUN_TEST(test_corrupt_page_falls_back);
    RUN_TEST(test_boot_confirm_clears_pending);
    RUN_TEST(test_boot_confirm_noop_without_pending);
    return UNITY_END();
}