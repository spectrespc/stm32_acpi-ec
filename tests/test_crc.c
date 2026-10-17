/* tests/test_crc.c
 *
 * Проверка CRC32 и CRC16-CCITT на стандартных векторах.
 */

#include "unity.h"
#include "crc.h"

static void test_crc32_known_vector(void)
{
    const uint8_t data[] = "123456789";
    TEST_ASSERT_EQUAL_HEX32(0xCBF43926UL, crc32_sw(data, 9));
}

static void test_crc32_empty(void)
{
    TEST_ASSERT_EQUAL_HEX32(0x00000000UL, crc32_sw((const uint8_t *)"", 0));
}

static void test_crc32_incremental_data(void)
{
    const uint8_t data[] = {0x00, 0xFF, 0x10, 0x20};
    /* Значение получено эталонной реализацией (zlib crc32). */
    TEST_ASSERT_EQUAL_HEX32(0xEE70D968UL, crc32_sw(data, sizeof data));
}

static void test_crc16_ccitt_known_vector(void)
{
    const uint8_t data[] = "123456789";
    /* CRC16-CCITT (poly 0x1021, init 0xFFFF) => 0x29B1 */
    TEST_ASSERT_EQUAL_UINT16(0x29B1U, crc16_ccitt(data, 9));
}

static void test_crc16_ccitt_empty(void)
{
    TEST_ASSERT_EQUAL_UINT16(0xFFFFU, crc16_ccitt((const uint8_t *)"", 0));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_crc32_known_vector);
    RUN_TEST(test_crc32_empty);
    RUN_TEST(test_crc32_incremental_data);
    RUN_TEST(test_crc16_ccitt_known_vector);
    RUN_TEST(test_crc16_ccitt_empty);
    return UNITY_END();
}