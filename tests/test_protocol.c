/* tests/test_protocol.c
 *
 * Проверка фрейминга протокола v2: разбор заголовка кадра и
 * little-endian упаковка/распаковка.
 */

#include "unity.h"
#include "protocol.h"

static void test_le16_roundtrip(void)
{
    uint8_t b[2];
    proto_put_le16(b, 0x1234U);
    TEST_ASSERT_EQUAL_UINT16(0x1234U, proto_get_le16(b));
    TEST_ASSERT_EQUAL_UINT16(0x0034U, (uint16_t)b[0]);
    TEST_ASSERT_EQUAL_UINT16(0x0012U, (uint16_t)b[1]);
}

static void test_le32_roundtrip(void)
{
    uint8_t b[4];
    proto_put_le32(b, 0xDEADBEEFUL);
    TEST_ASSERT_EQUAL_HEX32(0xDEADBEEFUL, proto_get_le32(b));
    TEST_ASSERT_EQUAL_UINT16(0x00EFU, (uint16_t)b[0]);
    TEST_ASSERT_EQUAL_UINT16(0x00DEU, (uint16_t)b[3]);
}

static void test_parse_valid_frame(void)
{
    const uint8_t buf[] = {0x02, 0x00, CMD_GET_INFO, 0xAA};
    uint16_t frame_len = 0;
    uint8_t cmd = 0;
    uint16_t plen = 0;

    TEST_ASSERT_TRUE(protocol_parse_request(buf, sizeof buf, 260, &frame_len, &cmd, &plen));
    TEST_ASSERT_EQUAL_UINT16(2U, frame_len);
    TEST_ASSERT_EQUAL_UINT16(CMD_GET_INFO, cmd);
    TEST_ASSERT_EQUAL_UINT16(1U, plen);
}

static void test_parse_zero_length_rejected(void)
{
    const uint8_t buf[] = {0x00, 0x00, 0x01};
    TEST_ASSERT_FALSE(protocol_parse_request(buf, sizeof buf, 260, NULL, NULL, NULL));
}

static void test_parse_truncated_rejected(void)
{
    const uint8_t buf[] = {0x05, 0x00, 0x01};
    TEST_ASSERT_FALSE(protocol_parse_request(buf, sizeof buf, 260, NULL, NULL, NULL));
}

static void test_parse_max_len_rejected(void)
{
    const uint8_t buf[] = {0x02, 0x00, CMD_GET_INFO, 0xAA};
    TEST_ASSERT_FALSE(protocol_parse_request(buf, sizeof buf, 3, NULL, NULL, NULL));
}

static void test_parse_max_len_boundary(void)
{
    const uint8_t buf[] = {0x02, 0x00, CMD_GET_INFO, 0xAA};
    TEST_ASSERT_TRUE(protocol_parse_request(buf, sizeof buf, 4, NULL, NULL, NULL));
}

static void test_parse_write_data_header(void)
{
    /* WRITE_DATA: LEN = 1(cmd) + 4(offset) + 1(len) = 6 */
    const uint8_t buf[] = {0x06, 0x00, CMD_WRITE_DATA, 0x00, 0x00, 0x00, 0x00, 0x20};
    uint8_t cmd = 0;
    uint16_t plen = 0;

    TEST_ASSERT_TRUE(protocol_parse_request(buf, sizeof buf, 260, NULL, &cmd, &plen));
    TEST_ASSERT_EQUAL_UINT16(CMD_WRITE_DATA, cmd);
    TEST_ASSERT_EQUAL_UINT16(5U, plen);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_le16_roundtrip);
    RUN_TEST(test_le32_roundtrip);
    RUN_TEST(test_parse_valid_frame);
    RUN_TEST(test_parse_zero_length_rejected);
    RUN_TEST(test_parse_truncated_rejected);
    RUN_TEST(test_parse_max_len_rejected);
    RUN_TEST(test_parse_max_len_boundary);
    RUN_TEST(test_parse_write_data_header);
    return UNITY_END();
}
