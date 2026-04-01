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
    size_t space = MAX_BUF - buf_len_;
    if (len > space) {
        // Shift buffer to make room, keeping most recent data
        size_t keep = MAX_BUF - len;
        if (keep < MAX_BUF && keep > 0) {
            memmove(buf_, buf_ + (buf_len_ - keep), keep);
            buf_len_ = keep;
        } else {
            buf_len_ = 0;
        }
        space = MAX_BUF - buf_len_;
        if (len > space) len = space;
    }
    memcpy(buf_ + buf_len_, data, len);
    buf_len_ += len;

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

void BuderusProtocol::emitRecord() {
    if (lastrec_ && recbuf_len_ > 0 && callback_) {
        callback_(lastrec_, recbuf_, recbuf_len_);
    }
}

// Find 0xAF 0x82 or 0xAF 0x02 in buffer. Returns index of 0xAF or -1.
static int findMarker(const uint8_t* buf, size_t len, bool* alt_marker) {
    int be = -1;
    int be2 = -1;

    for (size_t i = 0; i + 1 < len; i++) {
        if (buf[i] == 0xAF && buf[i + 1] == 0x82) {
            if (be < 0) be = (int)i;
            break;
        }
    }
    for (size_t i = 0; i + 1 < len; i++) {
        if (buf[i] == 0xAF && buf[i + 1] == 0x02) {
            if (be2 < 0) be2 = (int)i;
            break;
        }
    }

    if (be < 0 && be2 >= 0) {
        be = be2;
    } else if (be >= 0 && be2 >= 0 && be2 < be) {
        be = be2;
    }

    *alt_marker = (be >= 0 && be == be2);
    return be;
}

void BuderusProtocol::processBuffer() {
    while (true) {
        bool alt_marker = false;
        int be = findMarker(buf_, buf_len_, &alt_marker);

        // Handle protocol exception: 0x89 0x18 with extra bytes
        if (be >= 0 && (be == 9 || be == 10) && buf_len_ >= 2 && buf_[0] == 0x89 && buf_[1] == 0x18) {
            // Skip the 0x89 0x18 prefix
            size_t skip = 2;
            memmove(buf_, buf_ + skip, buf_len_ - skip);
            buf_len_ -= skip;
            continue;
        }

        if (be < 0) break;

        if (be >= 9) {
            const uint8_t* subblock = buf_ + be - 9;

            // Verify checksum
            uint8_t cs = checksum(subblock);
            if (cs != subblock[8]) {
                // Checksum error, skip past this marker
                size_t skip = be + 1;
                memmove(buf_, buf_ + skip, buf_len_ - skip);
                buf_len_ -= skip;
                continue;
            }

            uint8_t recnum = subblock[0];
            uint8_t payofs = subblock[1];
            const uint8_t* payload = subblock + 2; // 6 bytes

            // New record or alt marker or 0x89/0x18 exception
            if (payofs == 0 || alt_marker || (recnum == 0x89 && payofs == 0x18)) {
                emitRecord();
                memcpy(recbuf_, payload, 6);
                recbuf_len_ = 6;
            } else {
                if (recbuf_len_ > 0 && recbuf_len_ + 6 <= sizeof(recbuf_)) {
                    memcpy(recbuf_ + recbuf_len_, payload, 6);
                    recbuf_len_ += 6;
                }
            }

            lastrec_ = recnum;

            // Consume up to marker position + 1 (keep marker's second byte for next scan)
            size_t skip = be + 1;
            memmove(buf_, buf_ + skip, buf_len_ - skip);
            buf_len_ -= skip;
        } else {
            // Not enough data before marker, discard
            size_t skip = be + 2;
            if (skip > buf_len_) skip = buf_len_;
            memmove(buf_, buf_ + skip, buf_len_ - skip);
            buf_len_ -= skip;
        }
    }
}
