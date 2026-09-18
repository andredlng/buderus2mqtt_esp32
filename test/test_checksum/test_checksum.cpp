#include <unity.h>
#include "buderus_protocol.h"
#include <cstring>

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

void test_no_hybrid_record_on_lost_payofs0() {
    // Simulate: 2 chunks of zone 0x81, then boiler 0x88 chunk with payofs=6
    // (as if boiler's payofs=0 frame was lost), then zone 0x82 payofs=0 to trigger emission.
    // Bug: without the fix, boiler chunks get appended to zone data, creating a hybrid record.

    uint8_t stream[] = {
        // Zone 0x81 chunk 0 (payofs=0)
        0x81, 0x00, 0x04, 0x02, 0x28, 0x28, 0x2A, 0x29, 0x59, 0xAF, 0x82,
        // Zone 0x81 chunk 1 (payofs=6)
        0x81, 0x06, 0x00, 0x00, 0x64, 0x02, 0x80, 0x00, 0xD0, 0xAF, 0x82,
        // Boiler 0x88 chunk 1 (payofs=6) - simulates lost payofs=0
        0x88, 0x06, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF, 0x6F, 0xAF, 0x82,
        // Zone 0x82 chunk 0 (payofs=0) - triggers emission
        0x82, 0x00, 0x04, 0x02, 0x19, 0x19, 0x26, 0x6E, 0x9C, 0xAF, 0x82,
    };

    uint8_t recnums[8] = {};
    size_t reclens[8] = {};
    int call_count = 0;

    BuderusProtocol proto;
    proto.begin();
    proto.onRecord([&](uint8_t recnum, const uint8_t* data, size_t len) {
        if (call_count < 8) {
            recnums[call_count] = recnum;
            reclens[call_count] = len;
        }
        call_count++;
    });

    proto.feedBytes(stream, sizeof(stream));

    // Trace with fix:
    // 1. Zone 0x81 chunk 0 (payofs=0): emitRecord no-op, start recbuf_, lastrec_=0x81
    // 2. Zone 0x81 chunk 1 (payofs=6): append, recbuf_len_=12
    // 3. Boiler 0x88 (payofs=6): recnum!=lastrec_ -> emitRecord(zone 0x81, 12 bytes), reset
    // 4. Zone 0x82 (payofs=0): emitRecord no-op (recbuf_len_==0), start new record
    TEST_ASSERT_EQUAL(1, call_count);
    TEST_ASSERT_EQUAL_HEX8(0x81, recnums[0]);
    TEST_ASSERT_EQUAL(12, reclens[0]);

    // Critical: no record with recnum=0x88 was emitted (no hybrid)
    for (int i = 0; i < call_count; i++) {
        TEST_ASSERT_NOT_EQUAL_HEX8(0x88, recnums[i]);
    }
}

static size_t appendFrame(uint8_t* stream, size_t pos, uint8_t recnum, uint8_t payofs, const uint8_t* payload) {
    uint8_t* f = stream + pos;
    f[0] = recnum;
    f[1] = payofs;
    memcpy(f + 2, payload, 6);
    f[8] = BuderusProtocol::checksum(f);
    f[9] = 0xAF;
    f[10] = 0x82;
    return pos + 11;
}

void test_marker_bytes_inside_payload() {
    // Marker-like bytes that do not end a checksum-valid frame must not split it.
    // Unstuffed data can't contain them on the real bus; this guards against line noise.
    uint8_t expected[42];
    for (size_t i = 0; i < sizeof(expected); i++) expected[i] = (uint8_t)(i + 1);
    expected[13] = 0xAF;
    expected[14] = 0x02;
    expected[26] = 0xAF;
    expected[27] = 0x82;

    uint8_t stream[11 * 8];
    size_t pos = 0;
    for (uint8_t ofs = 0; ofs < 42; ofs += 6) {
        pos = appendFrame(stream, pos, 0x88, ofs, expected + ofs);
    }
    const uint8_t trigger[] = {0x04, 0x02, 0x19, 0x19, 0x26, 0x6E};
    pos = appendFrame(stream, pos, 0x82, 0x00, trigger);

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
            memcpy(got_data, data, len < sizeof(got_data) ? len : sizeof(got_data));
        }
        call_count++;
    });

    proto.feedBytes(stream, pos);

    TEST_ASSERT_EQUAL(1, call_count);
    TEST_ASSERT_EQUAL_HEX8(0x88, got_recnum);
    TEST_ASSERT_EQUAL(42, got_len);
    TEST_ASSERT_EQUAL_HEX8_ARRAY(expected, got_data, 42);
}

// The controller inserts 0x00 after every 0xAF inside a frame (byte stuffing).
static size_t appendStuffedFrame(uint8_t* stream, size_t pos, uint8_t recnum, uint8_t payofs, const uint8_t* payload) {
    uint8_t f[11];
    appendFrame(f, 0, recnum, payofs, payload);
    for (size_t i = 0; i < 9; i++) {
        stream[pos++] = f[i];
        if (f[i] == 0xAF) stream[pos++] = 0x00;
    }
    stream[pos++] = 0xAF;
    stream[pos++] = 0x82;
    return pos;
}

// Boiler record captured from the bus: the 0x0c frame's checksum is 0xAF, sent as "af 00".
static const uint8_t captured_boiler[42] = {
    0x23, 0x44, 0x1c, 0x2a, 0x15, 0x88,
    0x00, 0x00, 0x00, 0xff, 0x00, 0x00,
    0x17, 0x33, 0x05, 0x00, 0x00, 0x05,
    0x04, 0x27, 0x36, 0x00, 0x00, 0x00,
    0x1e, 0x64, 0x9c, 0x00, 0x44, 0x6e,
    0xcd, 0x43, 0x00, 0x00, 0x04, 0x00,
    0x00, 0x00, 0x6e, 0x6e, 0x6e, 0x00,
};

static size_t buildCapturedBoilerStream(uint8_t* stream) {
    size_t pos = 0;
    for (uint8_t ofs = 0; ofs < 42; ofs += 6) {
        pos = appendStuffedFrame(stream, pos, 0x88, ofs, captured_boiler + ofs);
    }
    const uint8_t zeros[6] = {};
    return appendFrame(stream, pos, 0x80, 0x00, zeros);
}

void test_stuffed_checksum_byte() {
    uint8_t stream[11 * 8 + 8];
    size_t len = buildCapturedBoilerStream(stream);
    const uint8_t wire_0c[] = {0x88, 0x0c, 0x17, 0x33, 0x05, 0x00, 0x00, 0x05, 0xaf, 0x00, 0xaf, 0x82};
    TEST_ASSERT_EQUAL_HEX8_ARRAY(wire_0c, stream + 22, sizeof(wire_0c));

    uint8_t got_data[64] = {};
    size_t got_len = 0;
    int calls = 0;
    int discards = 0;
    BuderusProtocol proto;
    proto.begin();
    proto.onRecord([&](uint8_t recnum, const uint8_t* data, size_t n) {
        if (recnum == 0x88) { got_len = n; memcpy(got_data, data, n); }
        calls++;
    });
    proto.onDiscard([&](const uint8_t*, size_t, size_t, uint8_t, uint8_t, size_t) { discards++; });

    proto.feedBytes(stream, len);

    TEST_ASSERT_EQUAL(1, calls);
    TEST_ASSERT_EQUAL(42, got_len);
    TEST_ASSERT_EQUAL_HEX8_ARRAY(captured_boiler, got_data, 42);
    TEST_ASSERT_EQUAL(0, discards);
}

void test_stuffing_split_across_reads() {
    uint8_t stream[11 * 8 + 8];
    size_t len = buildCapturedBoilerStream(stream);

    uint8_t got_data[64] = {};
    size_t got_len = 0;
    BuderusProtocol proto;
    proto.begin();
    proto.onRecord([&](uint8_t recnum, const uint8_t* data, size_t n) {
        if (recnum == 0x88) { got_len = n; memcpy(got_data, data, n); }
    });

    for (size_t i = 0; i < len; i++) proto.feedBytes(stream + i, 1);

    TEST_ASSERT_EQUAL(42, got_len);
    TEST_ASSERT_EQUAL_HEX8_ARRAY(captured_boiler, got_data, 42);
}

void test_stuffed_af_followed_by_data_zero() {
    // Payload "af 00" is sent as "af 00 00": only the first 0x00 is stuffing.
    const uint8_t payload[6] = {0xAF, 0x00, 0x01, 0x02, 0x03, 0x04};
    uint8_t stream[32];
    size_t pos = appendStuffedFrame(stream, 0, 0x84, 0x00, payload);
    const uint8_t zeros[6] = {};
    pos = appendFrame(stream, pos, 0x80, 0x00, zeros);

    uint8_t got_data[16] = {};
    size_t got_len = 0;
    BuderusProtocol proto;
    proto.begin();
    proto.onRecord([&](uint8_t recnum, const uint8_t* data, size_t n) {
        if (recnum == 0x84) { got_len = n; memcpy(got_data, data, n); }
    });
    proto.feedBytes(stream, pos);

    TEST_ASSERT_EQUAL(6, got_len);
    TEST_ASSERT_EQUAL_HEX8_ARRAY(payload, got_data, 6);
}

void test_perl_89_18_exception_is_a_stuffed_frame() {
    // "89 18 01 af 00 de 00 00 00 ed af 82" from l4000-daemon.pl continues the config record.
    const uint8_t zeros[6] = {};
    uint8_t stream[11 * 6 + 1];
    size_t pos = 0;
    for (uint8_t ofs = 0; ofs < 0x18; ofs += 6) pos = appendFrame(stream, pos, 0x89, ofs, zeros);
    const uint8_t wire[] = {0x89, 0x18, 0x01, 0xaf, 0x00, 0xde, 0x00, 0x00, 0x00, 0xed, 0xaf, 0x82};
    memcpy(stream + pos, wire, sizeof(wire));
    pos += sizeof(wire);
    pos = appendFrame(stream, pos, 0x8a, 0x00, zeros);

    uint8_t got_data[64] = {};
    size_t got_len = 0;
    int calls = 0;
    BuderusProtocol proto;
    proto.begin();
    proto.onRecord([&](uint8_t recnum, const uint8_t* data, size_t n) {
        if (recnum == 0x89) { got_len = n; memcpy(got_data, data, n); }
        calls++;
    });
    proto.feedBytes(stream, pos);

    TEST_ASSERT_EQUAL(1, calls);
    TEST_ASSERT_EQUAL(30, got_len);
    const uint8_t tail[] = {0x01, 0xaf, 0xde, 0x00, 0x00, 0x00};
    TEST_ASSERT_EQUAL_HEX8_ARRAY(tail, got_data + 24, 6);
}

struct DiscardReport {
    uint8_t data[64];
    size_t len;
    size_t total;
    uint8_t recnum;
    uint8_t payofs;
    size_t expected_ofs;
};

void test_discarded_bytes_are_reported() {
    uint8_t zeros[6] = {};
    uint8_t stream[11 * 4];
    size_t pos = 0;
    pos = appendFrame(stream, pos, 0x88, 0x00, zeros);
    pos = appendFrame(stream, pos, 0x88, 0x06, zeros);
    stream[pos - 11 + 4] ^= 0xFF; // corrupt second frame
    pos = appendFrame(stream, pos, 0x88, 0x0C, zeros);

    DiscardReport reports[4] = {};
    int count = 0;

    BuderusProtocol proto;
    proto.begin();
    proto.onDiscard([&](const uint8_t* data, size_t len, size_t total,
                        uint8_t recnum, uint8_t payofs, size_t expected_ofs) {
        if (count < 4) {
            memcpy(reports[count].data, data, len);
            reports[count].len = len;
            reports[count].total = total;
            reports[count].recnum = recnum;
            reports[count].payofs = payofs;
            reports[count].expected_ofs = expected_ofs;
        }
        count++;
    });

    proto.feedBytes(stream, pos);

    TEST_ASSERT_EQUAL(1, count);
    // Trailing 0x82 of frame 1 plus the whole corrupt frame
    TEST_ASSERT_EQUAL(12, reports[0].total);
    TEST_ASSERT_EQUAL(12, reports[0].len);
    TEST_ASSERT_EQUAL_HEX8_ARRAY(stream + 10, reports[0].data, 12);
    TEST_ASSERT_EQUAL_HEX8(0x88, reports[0].recnum);
    TEST_ASSERT_EQUAL_HEX8(0x0C, reports[0].payofs);
    TEST_ASSERT_EQUAL(6, reports[0].expected_ofs);
}

void test_clean_stream_reports_no_discards() {
    uint8_t zeros[6] = {};
    uint8_t stream[11 * 3];
    size_t pos = 0;
    pos = appendFrame(stream, pos, 0x88, 0x00, zeros);
    pos = appendFrame(stream, pos, 0x88, 0x06, zeros);
    pos = appendFrame(stream, pos, 0x82, 0x00, zeros);

    int count = 0;
    BuderusProtocol proto;
    proto.begin();
    proto.onDiscard([&](const uint8_t*, size_t, size_t, uint8_t, uint8_t, size_t) { count++; });

    // Byte-wise feeding exercises the trim path between frames
    for (size_t i = 0; i < pos; i++) proto.feedBytes(stream + i, 1);

    TEST_ASSERT_EQUAL(0, count);
}

void test_end_of_cycle_block_is_not_reported() {
    // Captured before zone 1 on every cycle: "af 82 a0 a4 af 82" after a frame ending in "af 02".
    const uint8_t zeros[6] = {};
    uint8_t stream[64];
    size_t pos = appendFrame(stream, 0, 0x9e, 0x00, zeros);
    stream[pos - 1] = 0x02;
    const uint8_t block[] = {0xaf, 0x82, 0xa0, 0xa4, 0xaf, 0x82};
    memcpy(stream + pos, block, sizeof(block));
    pos += sizeof(block);
    pos = appendFrame(stream, pos, 0x80, 0x00, zeros);

    int discards = 0;
    int records = 0;
    BuderusProtocol proto;
    proto.begin();
    proto.onRecord([&](uint8_t, const uint8_t*, size_t) { records++; });
    proto.onDiscard([&](const uint8_t*, size_t, size_t, uint8_t, uint8_t, size_t) { discards++; });
    proto.feedBytes(stream, pos);

    TEST_ASSERT_EQUAL(1, records);
    TEST_ASSERT_EQUAL(0, discards);
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
    RUN_TEST(test_no_hybrid_record_on_lost_payofs0);
    RUN_TEST(test_marker_bytes_inside_payload);
    RUN_TEST(test_discarded_bytes_are_reported);
    RUN_TEST(test_clean_stream_reports_no_discards);
    RUN_TEST(test_end_of_cycle_block_is_not_reported);
    RUN_TEST(test_stuffed_checksum_byte);
    RUN_TEST(test_stuffing_split_across_reads);
    RUN_TEST(test_stuffed_af_followed_by_data_zero);
    RUN_TEST(test_perl_89_18_exception_is_a_stuffed_frame);
    return UNITY_END();
}
