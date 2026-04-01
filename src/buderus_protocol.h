#pragma once

#include <cstdint>
#include <cstddef>
#include <functional>

#ifndef NATIVE_TEST
#include <Arduino.h>
#endif

class BuderusProtocol {
public:
    using RecordCallback = std::function<void(uint8_t recnum, const uint8_t* data, size_t len)>;

    void begin(
#ifndef NATIVE_TEST
        HardwareSerial& serial
#endif
    );
    void loop();
    void onRecord(RecordCallback cb) { callback_ = cb; }

    // Public for testing
    static uint8_t checksum(const uint8_t* block);

    // Feed raw bytes (used by tests and by loop())
    void feedBytes(const uint8_t* data, size_t len);

private:
    void processBuffer();
    void emitRecord();

    static const size_t MAX_BUF = 2048;
    uint8_t buf_[MAX_BUF];
    size_t buf_len_ = 0;

    uint8_t recbuf_[256];
    size_t recbuf_len_ = 0;
    uint8_t lastrec_ = 0;

    RecordCallback callback_;

#ifndef NATIVE_TEST
    HardwareSerial* serial_ = nullptr;
#endif
};
