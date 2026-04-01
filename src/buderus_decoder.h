#pragma once

#include <cstdint>
#include <cstddef>

struct MqttMessage {
    char key[24];
    char value[256];
};

class BuderusDecoder {
public:
    size_t dispatch(uint8_t recnum, const uint8_t* data, size_t len, MqttMessage* out, size_t max_out);

    // Public for testing
    static size_t decodeZone(uint8_t zone, const uint8_t* data, size_t len, MqttMessage* out, size_t max_out, uint32_t run);
    static size_t decodeWater(const uint8_t* data, size_t len, MqttMessage* out, size_t max_out, uint32_t run);
    static size_t decodeBoiler(const uint8_t* data, size_t len, MqttMessage* out, size_t max_out, uint32_t run);
    static size_t decodeConfig(const uint8_t* data, size_t len, MqttMessage* out, size_t max_out, uint32_t run);
    static size_t decodeEnergy(const uint8_t* data, size_t len, MqttMessage* out, size_t max_out, uint32_t run);
    static size_t decodeSolar(const uint8_t* data, size_t len, MqttMessage* out, size_t max_out, uint32_t run);

private:
    static int8_t signedByte(uint8_t b) { return (int8_t)b; }
    static bool reclenCheck(int expected, size_t actual);

    static size_t addMsg(MqttMessage* out, size_t idx, size_t max, const char* key, const char* value);
    static size_t addMsgInt(MqttMessage* out, size_t idx, size_t max, const char* key, int value);
    static size_t addMsgFloat(MqttMessage* out, size_t idx, size_t max, const char* key, float value);

    uint32_t run_zone_[9] = {};
    uint32_t run_water_ = 0;
    uint32_t run_boiler_ = 0;
    uint32_t run_config_ = 0;
    uint32_t run_energy_ = 0;
    uint32_t run_solar_ = 0;
};
