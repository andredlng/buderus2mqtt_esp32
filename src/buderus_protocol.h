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
    // Bytes skipped before a valid frame (total may exceed len if truncated),
    // plus that frame's recnum/payofs and the payload offset the record expected.
    using DiscardCallback = std::function<void(const uint8_t* data, size_t len, size_t total,
                                               uint8_t recnum, uint8_t payofs, size_t expected_ofs)>;

    void begin(
#ifndef NATIVE_TEST
        HardwareSerial& serial
#endif
    );
    void loop();
    void onRecord(RecordCallback cb) { callback_ = cb; }
    void onDiscard(DiscardCallback cb) { discard_cb_ = cb; }

    // Public for testing
    static uint8_t checksum(const uint8_t* block);

    // Feed raw bytes (used by tests and by loop())
    void feedBytes(const uint8_t* data, size_t len);

private:
    void processBuffer();
    void emitRecord();
    void addJunk(const uint8_t* data, size_t len);

    static const size_t MAX_BUF = 2048;
    uint8_t buf_[MAX_BUF];
    size_t buf_len_ = 0;

    uint8_t recbuf_[256];
    size_t recbuf_len_ = 0;
    uint8_t lastrec_ = 0;

    uint8_t junk_[64];
    size_t junk_len_ = 0;
    size_t junk_total_ = 0;

    RecordCallback callback_;
    DiscardCallback discard_cb_;

#ifndef NATIVE_TEST
    HardwareSerial* serial_ = nullptr;
#endif
};
