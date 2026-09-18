#include "buderus_protocol.h"
#include <cstring>

void BuderusProtocol::begin(
#ifndef NATIVE_TEST
    HardwareSerial& serial
#endif
) {
#ifndef NATIVE_TEST
    serial_ = &serial;
#endif
    buf_len_ = 0;
    recbuf_len_ = 0;
    lastrec_ = 0;
    prev_af_ = false;
    junk_len_ = 0;
    junk_total_ = 0;
}

uint8_t BuderusProtocol::checksum(const uint8_t* block) {
    uint8_t cs = 0;
    for (int n = 0; n < 8; n++) {
        uint8_t b = block[n];
        for (int i = 0; i < n; i++) {
            if (b < 0x80) {
                b = (b << 1) & 0xFF;
            } else {
                b = (b & 0x7F) << 1;
                b &= 0xFF;
                b ^= 0x19;
            }
        }
        cs ^= b;
    }
    return cs;
}

void BuderusProtocol::feedBytes(const uint8_t* data, size_t len) {
    for (size_t i = 0; i < len; i++) {
        uint8_t b = data[i];
        // The controller stuffs 0x00 after every 0xAF inside a frame, so data
        // never looks like an 0xAF 0x82 / 0xAF 0x02 end marker.
        if (prev_af_ && b == 0x00) {
            prev_af_ = false;
            continue;
        }
        prev_af_ = (b == 0xAF);

        // processBuffer() leaves at most 10 bytes, so this always makes room.
        if (buf_len_ == MAX_BUF) processBuffer();
        buf_[buf_len_++] = b;
    }

    processBuffer();
}

void BuderusProtocol::loop() {
#ifndef NATIVE_TEST
    if (!serial_) return;

    int avail = serial_->available();
    if (avail <= 0) return;

    uint8_t chunk[132];
    int toRead = avail < (int)sizeof(chunk) ? avail : (int)sizeof(chunk);
    int n = serial_->readBytes(chunk, toRead);
    if (n > 0) {
        feedBytes(chunk, n);
    }
#endif
}

void BuderusProtocol::addJunk(const uint8_t* data, size_t len) {
    size_t n = len < sizeof(junk_) - junk_len_ ? len : sizeof(junk_) - junk_len_;
    memcpy(junk_ + junk_len_, data, n);
    junk_len_ += n;
    junk_total_ += len;
}

void BuderusProtocol::emitRecord() {
    if (lastrec_ && recbuf_len_ > 0 && callback_) {
        callback_(lastrec_, recbuf_, recbuf_len_);
    }
}

// Find the first 0xAF 0x82 / 0xAF 0x02 marker preceded by a checksum-valid frame,
// so corrupted bytes before a marker can't shift frame boundaries.
// Returns index of 0xAF or -1.
static int findFrameMarker(const uint8_t* buf, size_t len, bool* alt_marker) {
    for (size_t i = 9; i + 1 < len; i++) {
        if (buf[i] != 0xAF || (buf[i + 1] != 0x82 && buf[i + 1] != 0x02)) continue;
        const uint8_t* subblock = buf + i - 9;
        if (BuderusProtocol::checksum(subblock) != subblock[8]) continue;
        *alt_marker = (buf[i + 1] == 0x02);
        return (int)i;
    }
    return -1;
}

void BuderusProtocol::processBuffer() {
    while (true) {
        bool alt_marker = false;
        int be = findFrameMarker(buf_, buf_len_, &alt_marker);

        if (be < 0) {
            // A frame completed by future bytes needs at most the last 10 bytes here.
            if (buf_len_ > 10) {
                addJunk(buf_, buf_len_ - 10);
                memmove(buf_, buf_ + buf_len_ - 10, 10);
                buf_len_ = 10;
            }
            break;
        }

        const uint8_t* subblock = buf_ + be - 9;
        uint8_t recnum = subblock[0];
        uint8_t payofs = subblock[1];
        const uint8_t* payload = subblock + 2; // 6 bytes

        if (be > 9) addJunk(buf_, be - 9);
        // A lone trailing marker byte of the previous frame is expected, not junk.
        bool only_marker_tail = junk_total_ == 1 && (junk_[0] == 0x82 || junk_[0] == 0x02);
        if (junk_total_ > 0 && !only_marker_tail && discard_cb_) {
            size_t expected_ofs = (recnum == lastrec_) ? recbuf_len_ : 0;
            discard_cb_(junk_, junk_len_, junk_total_, recnum, payofs, expected_ofs);
        }
        junk_len_ = 0;
        junk_total_ = 0;

        if (payofs == 0 || alt_marker) {
            emitRecord();
            memcpy(recbuf_, payload, 6);
            recbuf_len_ = 6;
        } else {
            if (recnum != lastrec_) {
                // Record type changed without payofs=0 (lost frame).
                // Emit accumulated record (likely complete) and reset
                // to prevent hybrid records with mixed data.
                emitRecord();
                recbuf_len_ = 0;
            } else if (recbuf_len_ > 0 && recbuf_len_ + 6 <= sizeof(recbuf_)) {
                memcpy(recbuf_ + recbuf_len_, payload, 6);
                recbuf_len_ += 6;
            }
        }

        lastrec_ = recnum;

        // Consume up to marker position + 1 (keep marker's second byte for next scan)
        size_t skip = be + 1;
        memmove(buf_, buf_ + skip, buf_len_ - skip);
        buf_len_ -= skip;
    }
}
