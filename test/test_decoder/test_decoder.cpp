#include <unity.h>
#include <cstring>
#include "buderus_decoder.h"

// Helper: find message by key
static const char* findValue(const MqttMessage* msgs, size_t n, const char* key) {
    for (size_t i = 0; i < n; i++) {
        if (strcmp(msgs[i].key, key) == 0) return msgs[i].value;
    }
    return nullptr;
}

static bool hasKey(const MqttMessage* msgs, size_t n, const char* key) {
    return findValue(msgs, n, key) != nullptr;
}

// --- Zone decoder tests ---

static void makeZoneRecord(uint8_t* rec, uint8_t rb0, uint8_t rb1, uint8_t vs, uint8_t vi,
                           uint8_t rs_raw, uint8_t ri_raw, uint8_t pu, uint8_t sg, uint8_t rb10) {
    memset(rec, 0, 18);
    rec[0] = rb0; rec[1] = rb1; rec[2] = vs; rec[3] = vi;
    rec[4] = rs_raw; rec[5] = ri_raw; rec[8] = pu; rec[9] = sg;
    rec[10] = rb10;
    rec[12] = 43; rec[13] = 38; rec[14] = 44; // k1/k2/k3
}

void test_zone1_basic_publish() {
    // Zone 1 publishes when run % 2 == 1 % 2, i.e. run=1
    uint8_t rec[18];
    makeZoneRecord(rec, 0x04, 0x02, 40, 40, 42, 41, 64, 0, 0);
    MqttMessage msgs[16];

    // run=0: 0 % 2 != 1 % 2 -> no publish
    size_t n = BuderusDecoder::decodeZone(1, rec, 18, msgs, 16, 0);
    TEST_ASSERT_EQUAL(0, n);

    // run=1: 1 % 2 == 1 % 2 -> publish
    n = BuderusDecoder::decodeZone(1, rec, 18, msgs, 16, 1);
    TEST_ASSERT_GREATER_THAN(0, n);

    TEST_ASSERT_EQUAL_STRING("20.5", findValue(msgs, n, "hk1"));      // ri = 41/2.0
    TEST_ASSERT_EQUAL_STRING("21.0", findValue(msgs, n, "hk1_s"));    // rs = 42/2.0
    TEST_ASSERT_EQUAL_STRING("40", findValue(msgs, n, "hk1_v"));
    TEST_ASSERT_EQUAL_STRING("40", findValue(msgs, n, "hk1_vs"));
    TEST_ASSERT_EQUAL_STRING("64", findValue(msgs, n, "hk1_pu"));
}

void test_zone2_publishes_on_even_run() {
    uint8_t rec[18];
    makeZoneRecord(rec, 0x04, 0x02, 40, 40, 42, 41, 64, 0, 0);
    MqttMessage msgs[16];

    // Zone 2, run=0: 0 % 2 == 2 % 2 -> publish
    size_t n = BuderusDecoder::decodeZone(2, rec, 18, msgs, 16, 0);
    TEST_ASSERT_GREATER_THAN(0, n);
}

void test_zone_invalid_sensor_skips_ri_and_pu() {
    // ri_raw=110 -> ri=55.0 -> invalid sensor
    uint8_t rec[18];
    makeZoneRecord(rec, 0x04, 0x02, 40, 40, 42, 110, 64, 0, 0);
    MqttMessage msgs[16];

    size_t n = BuderusDecoder::decodeZone(2, rec, 18, msgs, 16, 0);
    TEST_ASSERT_GREATER_THAN(0, n);

    TEST_ASSERT_FALSE(hasKey(msgs, n, "hk2"));
    TEST_ASSERT_FALSE(hasKey(msgs, n, "hk2_pu"));
    TEST_ASSERT_TRUE(hasKey(msgs, n, "hk2_s"));
    TEST_ASSERT_TRUE(hasKey(msgs, n, "hk2_sg"));
    TEST_ASSERT_TRUE(hasKey(msgs, n, "hk2_v"));
    TEST_ASSERT_TRUE(hasKey(msgs, n, "hk2_vs"));
}

void test_zone_invalid_flow_temp_skips_vi() {
    // vi=110 -> no flow temp sensor
    uint8_t rec[18];
    makeZoneRecord(rec, 0x04, 0x02, 40, 110, 42, 41, 64, 0, 0);
    MqttMessage msgs[16];

    size_t n = BuderusDecoder::decodeZone(2, rec, 18, msgs, 16, 0);
    TEST_ASSERT_FALSE(hasKey(msgs, n, "hk2_v"));
    TEST_ASSERT_TRUE(hasKey(msgs, n, "hk2_vs"));
}

void test_zone_both_sensors_invalid() {
    uint8_t rec[18];
    makeZoneRecord(rec, 0x04, 0x02, 40, 110, 42, 110, 64, 0, 0);
    MqttMessage msgs[16];

    size_t n = BuderusDecoder::decodeZone(2, rec, 18, msgs, 16, 0);
    TEST_ASSERT_FALSE(hasKey(msgs, n, "hk2_v"));
    TEST_ASSERT_FALSE(hasKey(msgs, n, "hk2"));
    TEST_ASSERT_FALSE(hasKey(msgs, n, "hk2_pu"));
    TEST_ASSERT_TRUE(hasKey(msgs, n, "hk2_s"));
    TEST_ASSERT_TRUE(hasKey(msgs, n, "hk2_sg"));
    TEST_ASSERT_TRUE(hasKey(msgs, n, "hk2_vs"));
    TEST_ASSERT_TRUE(hasKey(msgs, n, "hk2_err"));
}

void test_zone_signed_stellglied() {
    uint8_t rec[18];
    makeZoneRecord(rec, 0x04, 0x02, 40, 40, 42, 41, 64, 200, 0); // sg=200 -> -56
    MqttMessage msgs[16];

    size_t n = BuderusDecoder::decodeZone(2, rec, 18, msgs, 16, 0);
    TEST_ASSERT_EQUAL_STRING("-56", findValue(msgs, n, "hk2_sg"));
}

void test_zone_wrong_length() {
    uint8_t rec[12] = {};
    MqttMessage msgs[16];
    size_t n = BuderusDecoder::decodeZone(1, rec, 12, msgs, 16, 1);
    TEST_ASSERT_EQUAL(0, n);
}

void test_zone_error_flags() {
    uint8_t rec[18];
    makeZoneRecord(rec, 0x04, 0x04, 40, 40, 42, 41, 64, 0, 0); // rb1=0x04 -> Fernbedienung error
    MqttMessage msgs[16];

    size_t n = BuderusDecoder::decodeZone(2, rec, 18, msgs, 16, 0);
    const char* err = findValue(msgs, n, "hk2_err");
    TEST_ASSERT_NOT_NULL(err);
    TEST_ASSERT_NOT_EQUAL(nullptr, strstr(err, "Fernbedienungs-Kommunikation gestoert"));
}

void test_zone_no_error_empty_string() {
    uint8_t rec[18];
    makeZoneRecord(rec, 0x04, 0x02, 40, 40, 42, 41, 64, 0, 0);
    MqttMessage msgs[16];

    size_t n = BuderusDecoder::decodeZone(2, rec, 18, msgs, 16, 0);
    TEST_ASSERT_EQUAL_STRING("", findValue(msgs, n, "hk2_err"));
}

// --- Water decoder tests ---

void test_water_basic_publish() {
    uint8_t rec[12] = {};
    rec[0] = 0x01; rec[1] = 0x01; rec[2] = 60; rec[3] = 55; rec[5] = 0x03;
    MqttMessage msgs[16];

    size_t n = BuderusDecoder::decodeWater(rec, 12, msgs, 16, 0);
    TEST_ASSERT_GREATER_THAN(0, n);

    TEST_ASSERT_EQUAL_STRING("55", findValue(msgs, n, "ww"));
    TEST_ASSERT_EQUAL_STRING("60", findValue(msgs, n, "ww_s"));
    TEST_ASSERT_EQUAL_STRING("1", findValue(msgs, n, "ww_l"));
    TEST_ASSERT_EQUAL_STRING("1", findValue(msgs, n, "ww_laden"));
    TEST_ASSERT_EQUAL_STRING("1", findValue(msgs, n, "ww_zirk"));
}

void test_water_odd_run_no_publish() {
    uint8_t rec[12] = {};
    rec[0] = 0x01; rec[1] = 0x01; rec[2] = 60; rec[3] = 55;
    MqttMessage msgs[16];

    size_t n = BuderusDecoder::decodeWater(rec, 12, msgs, 16, 1);
    TEST_ASSERT_EQUAL(0, n);
}

void test_water_error_flags() {
    uint8_t rec[12] = {};
    rec[0] = 0x10; rec[1] = 0x01; rec[2] = 60; rec[3] = 55;
    MqttMessage msgs[16];

    size_t n = BuderusDecoder::decodeWater(rec, 12, msgs, 16, 0);
    const char* err = findValue(msgs, n, "ww_err");
    TEST_ASSERT_NOT_NULL(err);
    TEST_ASSERT_NOT_EQUAL(nullptr, strstr(err, "Fehler bei Desinfektion"));
}

void test_water_external_error() {
    uint8_t rec[12] = {};
    rec[2] = 60; rec[3] = 55; rec[7] = 0x01;
    MqttMessage msgs[16];

    size_t n = BuderusDecoder::decodeWater(rec, 12, msgs, 16, 0);
    TEST_ASSERT_EQUAL_STRING("Externe Fehlermeldung", findValue(msgs, n, "ww_err"));
}

void test_water_wf3_input_is_not_an_error() {
    uint8_t rec[12] = {};
    rec[2] = 60; rec[3] = 55; rec[6] = 0x02; // WF3-EIN
    MqttMessage msgs[16];

    size_t n = BuderusDecoder::decodeWater(rec, 12, msgs, 16, 0);
    TEST_ASSERT_EQUAL_STRING("", findValue(msgs, n, "ww_err"));
}

// --- Boiler decoder tests ---

void test_boiler_basic_publish() {
    uint8_t rec[42] = {};
    rec[0] = 70; rec[1] = 65; rec[2] = 60; rec[3] = 55; rec[8] = 80;
    MqttMessage msgs[16];

    size_t n = BuderusDecoder::decodeBoiler(rec, 42, msgs, 16, 0);
    TEST_ASSERT_GREATER_THAN(0, n);

    TEST_ASSERT_EQUAL_STRING("65", findValue(msgs, n, "kessel"));
    TEST_ASSERT_EQUAL_STRING("70", findValue(msgs, n, "kessel_s"));
    TEST_ASSERT_EQUAL_STRING("80", findValue(msgs, n, "brenner"));
    TEST_ASSERT_EQUAL_STRING("0", findValue(msgs, n, "brenner_betrieb"));
    TEST_ASSERT_EQUAL_STRING("60", findValue(msgs, n, "k_ein"));
    TEST_ASSERT_EQUAL_STRING("55", findValue(msgs, n, "k_aus"));
}

void test_boiler_status_fields() {
    uint8_t rec[42] = {};
    rec[0] = 70; rec[1] = 65; rec[7] = 0x0A; // Stufe 1 (0x02) + Betrieb (0x08)
    rec[34] = 0x04; // Brenner=Auto
    MqttMessage msgs[16];

    size_t n = BuderusDecoder::decodeBoiler(rec, 42, msgs, 16, 0);

    TEST_ASSERT_EQUAL_STRING("1", findValue(msgs, n, "brenner_betrieb"));
    TEST_ASSERT_EQUAL_STRING("1", findValue(msgs, n, "brenner_stufe1"));
    TEST_ASSERT_EQUAL_STRING("0", findValue(msgs, n, "brenner_stufe2"));
    TEST_ASSERT_EQUAL_STRING("Auto", findValue(msgs, n, "brenner_modus"));
}

void test_boiler_odd_run_no_publish() {
    uint8_t rec[42] = {};
    MqttMessage msgs[16];
    size_t n = BuderusDecoder::decodeBoiler(rec, 42, msgs, 16, 1);
    TEST_ASSERT_EQUAL(0, n);
}

void test_boiler_error_flags() {
    uint8_t rec[42] = {};
    rec[0] = 70; rec[1] = 65; rec[6] = 0x01; // BRENNERSTOERUNG
    MqttMessage msgs[16];

    size_t n = BuderusDecoder::decodeBoiler(rec, 42, msgs, 16, 0);
    const char* err = findValue(msgs, n, "kessel_err");
    TEST_ASSERT_NOT_NULL(err);
    TEST_ASSERT_NOT_EQUAL(nullptr, strstr(err, "BRENNERSTOERUNG"));
}

// --- Config decoder tests ---

void test_config_basic_publish() {
    uint8_t rec[18] = {};
    rec[0] = 10; rec[1] = 8;
    MqttMessage msgs[16];

    size_t n = BuderusDecoder::decodeConfig(rec, 18, msgs, 16, 0);
    TEST_ASSERT_GREATER_THAN(0, n);

    TEST_ASSERT_EQUAL_STRING("10", findValue(msgs, n, "aussen"));
    TEST_ASSERT_EQUAL_STRING("8", findValue(msgs, n, "aussen_d"));
}

void test_config_negative_temperature() {
    uint8_t rec[18] = {};
    rec[0] = 246; rec[1] = 248; // -10, -8 as signed bytes
    MqttMessage msgs[16];

    size_t n = BuderusDecoder::decodeConfig(rec, 18, msgs, 16, 0);
    TEST_ASSERT_GREATER_THAN(0, n);

    TEST_ASSERT_EQUAL_STRING("-10", findValue(msgs, n, "aussen"));
    TEST_ASSERT_EQUAL_STRING("-8", findValue(msgs, n, "aussen_d"));
}

void test_config_sensor_fault_no_publish() {
    uint8_t rec[18] = {};
    rec[0] = 110; rec[1] = 8;
    MqttMessage msgs[16];

    size_t n = BuderusDecoder::decodeConfig(rec, 18, msgs, 16, 0);
    TEST_ASSERT_EQUAL(0, n);
}

void test_config_plausibility_rejection() {
    uint8_t rec[18] = {};
    rec[0] = 60; rec[1] = 8; // 60 > 50
    MqttMessage msgs[16];

    size_t n = BuderusDecoder::decodeConfig(rec, 18, msgs, 16, 0);
    TEST_ASSERT_EQUAL(0, n);
}

void test_config_publish_frequency() {
    uint8_t rec[18] = {};
    rec[0] = 10; rec[1] = 8;
    MqttMessage msgs[16];
    int publish_count = 0;

    for (uint32_t run = 0; run < 10; run++) {
        size_t n = BuderusDecoder::decodeConfig(rec, 18, msgs, 16, run);
        if (n > 0) publish_count++;
    }
    // Runs 0, 5 -> 2 publishes
    TEST_ASSERT_EQUAL(2, publish_count);
}

// --- Energy decoder tests ---

void test_energy_basic_publish() {
    uint8_t rec[36] = {};
    uint32_t wm = 123456;
    rec[30] = (wm >> 24) & 0xFF;
    rec[31] = (wm >> 16) & 0xFF;
    rec[32] = (wm >> 8) & 0xFF;
    rec[33] = wm & 0xFF;
    MqttMessage msgs[16];

    size_t n = BuderusDecoder::decodeEnergy(rec, 36, msgs, 16, 0);
    TEST_ASSERT_GREATER_THAN(0, n);

    TEST_ASSERT_EQUAL_STRING("123456", findValue(msgs, n, "energie"));
}

void test_energy_32bit_counter() {
    uint8_t rec[36] = {};
    uint32_t wm = 0x01020304;
    rec[30] = 0x01; rec[31] = 0x02; rec[32] = 0x03; rec[33] = 0x04;
    MqttMessage msgs[16];

    size_t n = BuderusDecoder::decodeEnergy(rec, 36, msgs, 16, 0);
    TEST_ASSERT_EQUAL_STRING("16909060", findValue(msgs, n, "energie")); // 0x01020304 = 16909060
}

void test_energy_publish_frequency() {
    uint8_t rec[36] = {};
    rec[33] = 1; // wm=1
    MqttMessage msgs[16];
    int publish_count = 0;

    for (uint32_t run = 0; run < 26; run++) {
        size_t n = BuderusDecoder::decodeEnergy(rec, 36, msgs, 16, run);
        if (n > 0) publish_count++;
    }
    // Runs 0, 13 -> 2 publishes
    TEST_ASSERT_EQUAL(2, publish_count);
}

// --- Solar decoder tests ---

void test_solar_basic_publish() {
    uint8_t rec[10] = {};
    uint16_t ct_raw = 350;
    rec[3] = (ct_raw >> 8) & 0xFF;
    rec[4] = ct_raw & 0xFF;
    rec[5] = 80;  // pump
    rec[6] = 45;  // t1
    rec[8] = 40;  // t2
    MqttMessage msgs[16];

    size_t n = BuderusDecoder::decodeSolar(rec, 10, msgs, 16, 0);
    TEST_ASSERT_GREATER_THAN(0, n);

    TEST_ASSERT_EQUAL_STRING("35.0", findValue(msgs, n, "sol_coll"));
    TEST_ASSERT_EQUAL_STRING("80", findValue(msgs, n, "sol_pump"));
    TEST_ASSERT_EQUAL_STRING("45", findValue(msgs, n, "sol_t1"));
    TEST_ASSERT_EQUAL_STRING("40", findValue(msgs, n, "sol_t2"));
}

void test_solar_always_publishes() {
    uint8_t rec[10] = {};
    rec[3] = 0; rec[4] = 100; rec[5] = 50; rec[6] = 30; rec[8] = 25;
    MqttMessage msgs[16];

    for (uint32_t run = 0; run < 5; run++) {
        size_t n = BuderusDecoder::decodeSolar(rec, 10, msgs, 16, run);
        TEST_ASSERT_GREATER_THAN(0, n);
    }
}

void test_solar_short_record() {
    uint8_t rec[5] = {};
    MqttMessage msgs[16];
    size_t n = BuderusDecoder::decodeSolar(rec, 5, msgs, 16, 0);
    TEST_ASSERT_EQUAL(0, n);
}

void test_solar_error_flags() {
    uint8_t rec[10] = {};
    rec[0] = 0x08; // Collector Temp Limit
    rec[4] = 100;
    MqttMessage msgs[16];

    size_t n = BuderusDecoder::decodeSolar(rec, 10, msgs, 16, 0);
    const char* err = findValue(msgs, n, "sol_err");
    TEST_ASSERT_NOT_NULL(err);
    TEST_ASSERT_NOT_EQUAL(nullptr, strstr(err, "Collector Temp Limit"));
}

// --- Dispatch diagnostics ---

void test_dispatch_reports_wrong_record_length() {
    BuderusDecoder decoder;
    uint8_t rec[30] = {};
    rec[0] = 0xAB; rec[29] = 0x01;
    MqttMessage msgs[16];

    size_t n = decoder.dispatch(0x88, rec, 30, msgs, 16);
    TEST_ASSERT_EQUAL(2, n);
    TEST_ASSERT_EQUAL_STRING("rec=88 len=30 expected=42 data=ab"
                             "0000000000000000000000000000000000000000000000000000000001",
                             findValue(msgs, n, "diag_reclen_err"));
    TEST_ASSERT_EQUAL_STRING("1", findValue(msgs, n, "diag_reclen_count"));
    TEST_ASSERT_FALSE(hasKey(msgs, n, "kessel"));

    n = decoder.dispatch(0x89, rec, 6, msgs, 16);
    TEST_ASSERT_EQUAL_STRING("rec=89 len=6 min=18 data=ab0000000000", findValue(msgs, n, "diag_reclen_err"));
    TEST_ASSERT_EQUAL_STRING("2", findValue(msgs, n, "diag_reclen_count"));
}

void test_dispatch_valid_record_has_no_diagnostics() {
    BuderusDecoder decoder;
    uint8_t rec[42] = {};
    MqttMessage msgs[16];

    size_t n = decoder.dispatch(0x88, rec, 42, msgs, 16);
    TEST_ASSERT_TRUE(hasKey(msgs, n, "kessel"));
    TEST_ASSERT_FALSE(hasKey(msgs, n, "diag_reclen_err"));
}

void test_dispatch_ignores_unknown_record() {
    BuderusDecoder decoder;
    uint8_t rec[3] = {};
    MqttMessage msgs[16];

    TEST_ASSERT_EQUAL(0, decoder.dispatch(0x87, rec, 3, msgs, 16));
    TEST_ASSERT_EQUAL(0, decoder.dispatch(0x99, rec, 3, msgs, 16));
}

int main(int argc, char** argv) {
    UNITY_BEGIN();

    // Zone tests
    RUN_TEST(test_zone1_basic_publish);
    RUN_TEST(test_zone2_publishes_on_even_run);
    RUN_TEST(test_zone_invalid_sensor_skips_ri_and_pu);
    RUN_TEST(test_zone_invalid_flow_temp_skips_vi);
    RUN_TEST(test_zone_both_sensors_invalid);
    RUN_TEST(test_zone_signed_stellglied);
    RUN_TEST(test_zone_wrong_length);
    RUN_TEST(test_zone_error_flags);
    RUN_TEST(test_zone_no_error_empty_string);

    // Water tests
    RUN_TEST(test_water_basic_publish);
    RUN_TEST(test_water_odd_run_no_publish);
    RUN_TEST(test_water_error_flags);
    RUN_TEST(test_water_external_error);
    RUN_TEST(test_water_wf3_input_is_not_an_error);

    // Boiler tests
    RUN_TEST(test_boiler_basic_publish);
    RUN_TEST(test_boiler_status_fields);
    RUN_TEST(test_boiler_odd_run_no_publish);
    RUN_TEST(test_boiler_error_flags);

    // Config tests
    RUN_TEST(test_config_basic_publish);
    RUN_TEST(test_config_negative_temperature);
    RUN_TEST(test_config_sensor_fault_no_publish);
    RUN_TEST(test_config_plausibility_rejection);
    RUN_TEST(test_config_publish_frequency);

    // Energy tests
    RUN_TEST(test_energy_basic_publish);
    RUN_TEST(test_energy_32bit_counter);
    RUN_TEST(test_energy_publish_frequency);

    // Solar tests
    RUN_TEST(test_solar_basic_publish);
    RUN_TEST(test_solar_always_publishes);
    RUN_TEST(test_solar_short_record);
    RUN_TEST(test_solar_error_flags);

    // Dispatch diagnostics
    RUN_TEST(test_dispatch_reports_wrong_record_length);
    RUN_TEST(test_dispatch_valid_record_has_no_diagnostics);
    RUN_TEST(test_dispatch_ignores_unknown_record);

    return UNITY_END();
}
