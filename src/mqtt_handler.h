#pragma once

#include "config.h"

#ifndef NATIVE_TEST

#include <ETH.h>
#include <PubSubClient.h>

class MqttHandler {
public:
    void begin(const Config& cfg);
    void loop();
    void publish(const char* key, const char* value);

private:
    void connect();

    WiFiClient eth_client_;
    PubSubClient client_;
    const Config* cfg_ = nullptr;
    unsigned long last_reconnect_ = 0;
    char status_topic_[64];
};

#else

// Stub for native tests
class MqttHandler {
public:
    void begin(const Config&) {}
    void loop() {}
    void publish(const char*, const char*) {}
};

#endif
