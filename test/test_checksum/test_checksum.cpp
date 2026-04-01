#include <unity.h>
#include "buderus_protocol.h"

void test_known_block_zone2() {
    // Sample from Perl source: 81 00 04 02 28 28 2a 29 59
    uint8_t block[] = {0x81, 0x00, 0x04, 0x02, 0x28, 0x28, 0x2A, 0x29};
    TEST_ASSERT_EQUAL_HEX8(0x59, BuderusProtocol::checksum(block));
}

void test_known_block_zone2_cont() {
    // 81 06 00 00 64 02 80 00 d0
    uint8_t block[] = {0x81, 0x06, 0x00, 0x00, 0x64, 0x02, 0x80, 0x00};
    TEST_ASSERT_EQUAL_HEX8(0xD0, BuderusProtocol::checksum(block));
}

void test_known_block_zone2_cont2() {
    // 81 0c 2c 38 43 00 00 00 a4
    uint8_t block[] = {0x81, 0x0C, 0x2C, 0x38, 0x43, 0x00, 0x00, 0x00};
    TEST_ASSERT_EQUAL_HEX8(0xA4, BuderusProtocol::checksum(block));
}

void test_known_block_zone3() {
    // 82 00 04 02 19 19 26 6e 9c
    uint8_t block[] = {0x82, 0x00, 0x04, 0x02, 0x19, 0x19, 0x26, 0x6E};
    TEST_ASSERT_EQUAL_HEX8(0x9C, BuderusProtocol::checksum(block));
}

void test_known_block_zone3_cont() {
    // 82 06 00 00 64 00 80 00 93
    uint8_t block[] = {0x82, 0x06, 0x00, 0x00, 0x64, 0x00, 0x80, 0x00};
    TEST_ASSERT_EQUAL_HEX8(0x93, BuderusProtocol::checksum(block));
}

void test_all_zeros() {
    uint8_t block[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    TEST_ASSERT_EQUAL_HEX8(0x00, BuderusProtocol::checksum(block));
}

void test_protocol_record_assembly() {
    // Feed 3 blocks that form a zone 2 record (18 bytes = 3 blocks of 6 payload bytes)
    // Block 1: recnum=0x81, payofs=0x00, payload=04 02 28 28 2a 29, cs=0x59
    // Block 2: recnum=0x81, payofs=0x06, payload=00 00 64 02 80 00, cs=0xD0
    // Block 3: recnum=0x81, payofs=0x0C, payload=2c 38 43 00 00 00, cs=0xA4
    // Each followed by marker 0xAF 0x82

    uint8_t stream[] = {
        0x81, 0x00, 0x04, 0x02, 0x28, 0x28, 0x2A, 0x29, 0x59, 0xAF, 0x82,
        0x81, 0x06, 0x00, 0x00, 0x64, 0x02, 0x80, 0x00, 0xD0, 0xAF, 0x82,
        0x81, 0x0C, 0x2C, 0x38, 0x43, 0x00, 0x00, 0x00, 0xA4, 0xAF, 0x82,
    };

    // Feed a 4th block (new record) to trigger emission of the assembled record
    // Use zone 3 block: 82 00 04 02 19 19 26 6e 9c
    uint8_t trigger[] = {
        0x82, 0x00, 0x04, 0x02, 0x19, 0x19, 0x26, 0x6E, 0x9C, 0xAF, 0x82,
    };

    uint8_t got_recnum = 0;
    uint8_t got_data[64] = {};
    size_t got_len = 0;
    int call_count = 0;

    BuderusProtocol proto;
    proto.begin();
    proto.onRecord([&](uint8_t recnum, const uint8_t* data, size_t len) {
        if (call_count == 0) {
            got_recnum = recnum;
            got_len = len;
            memcpy(got_data, data, len);
        }
        call_count++;
    });

    proto.feedBytes(stream, sizeof(stream));
    // No emission yet (record still being assembled)
    TEST_ASSERT_EQUAL(0, call_count);

    // Trigger emission with new record
    proto.feedBytes(trigger, sizeof(trigger));
    TEST_ASSERT_EQUAL(1, call_count);
    TEST_ASSERT_EQUAL_HEX8(0x81, got_recnum);
    TEST_ASSERT_EQUAL(18, got_len);

    // Verify assembled payload: block1(04 02 28 28 2a 29) + block2(00 00 64 02 80 00) + block3(2c 38 43 00 00 00)
    uint8_t expected[] = {0x04, 0x02, 0x28, 0x28, 0x2A, 0x29,
                          0x00, 0x00, 0x64, 0x02, 0x80, 0x00,
                          0x2C, 0x38, 0x43, 0x00, 0x00, 0x00};
    TEST_ASSERT_EQUAL_HEX8_ARRAY(expected, got_data, 18);
}

int main(int argc, char** argv) {
    UNITY_BEGIN();
    RUN_TEST(test_known_block_zone2);
    RUN_TEST(test_known_block_zone2_cont);
    RUN_TEST(test_known_block_zone2_cont2);
    RUN_TEST(test_known_block_zone3);
    RUN_TEST(test_known_block_zone3_cont);
    RUN_TEST(test_all_zeros);
    RUN_TEST(test_protocol_record_assembly);
    return UNITY_END();
}
