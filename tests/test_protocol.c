/* tests/test_protocol.c
 *
 * Проверка фрейминга протокола v3: разбор заголовка кадра,
 * little-endian упаковка/распаковка и кадры power-команд.
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

static void test_proto_version_v3(void)
{
    TEST_ASSERT_EQUAL_UINT16(3U, PROTO_VER);
}

static void test_parse_get_power_frame(void)
{
    const uint8_t buf[] = {0x01, 0x00, CMD_GET_POWER};
    uint8_t cmd = 0;
    uint16_t plen = 0;

    TEST_ASSERT_TRUE(protocol_parse_request(buf, sizeof buf, 260, NULL, &cmd, &plen));
    TEST_ASSERT_EQUAL_UINT16(CMD_GET_POWER, cmd);
    TEST_ASSERT_EQUAL_UINT16(0U, plen);
}

static void test_parse_set_power_frame(void)
{
    const uint8_t buf[] = {0x03, 0x00, CMD_SET_POWER, HOST_STATE_AWAKE, POWER_ACK_REQUEST};
    uint8_t cmd = 0;
    uint16_t plen = 0;

    TEST_ASSERT_TRUE(protocol_parse_request(buf, sizeof buf, 260, NULL, &cmd, &plen));
    TEST_ASSERT_EQUAL_UINT16(CMD_SET_POWER, cmd);
    TEST_ASSERT_EQUAL_UINT16(POWER_SET_PAYLOAD_LEN, plen);
}

static void test_power_payload_layout(void)
{
    TEST_ASSERT_EQUAL_UINT16(8U, POWER_PAYLOAD_LEN);
    TEST_ASSERT_EQUAL_UINT32(0U, POWER_WAKE_NONE);
    TEST_ASSERT_EQUAL_UINT32(4U, POWER_WAKE_RESET);
    TEST_ASSERT_EQUAL_UINT32(4U, POWER_EVENT_DOUBLE);
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
    RUN_TEST(test_proto_version_v3);
    RUN_TEST(test_parse_get_power_frame);
    RUN_TEST(test_parse_set_power_frame);
    RUN_TEST(test_power_payload_layout);
    RUN_TEST(test_parse_write_data_header);
    return UNITY_END();
}
