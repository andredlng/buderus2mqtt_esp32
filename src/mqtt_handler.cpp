#include "mqtt_handler.h"

#ifndef NATIVE_TEST

void MqttHandler::begin(const Config& cfg) {
    cfg_ = &cfg;
    client_.setClient(eth_client_);
    client_.setServer(cfg.mqtt_host, cfg.mqtt_port);
    client_.setKeepAlive(cfg.mqtt_keepalive);

    snprintf(status_topic_, sizeof(status_topic_), "%s/status", cfg.mqtt_topic);

    connect();
}

void MqttHandler::connect() {
    if (client_.connected()) return;

    Serial.printf("[mqtt] Connecting to %s:%d...\n", cfg_->mqtt_host, cfg_->mqtt_port);

    bool ok;
    if (cfg_->mqtt_user[0]) {
        ok = client_.connect(cfg_->mqtt_clientid, cfg_->mqtt_user, cfg_->mqtt_password,
                             status_topic_, 0, false, "offline");
    } else {
        ok = client_.connect(cfg_->mqtt_clientid, nullptr, nullptr,
                             status_topic_, 0, false, "offline");
    }

    if (ok) {
        Serial.println("[mqtt] Connected");
        client_.publish(status_topic_, "online");
    } else {
        Serial.printf("[mqtt] Failed, rc=%d\n", client_.state());
    }
}

void MqttHandler::loop() {
    if (client_.connected()) {
        client_.loop();
        return;
    }

    unsigned long now = millis();
    if (now - last_reconnect_ >= 5000) {
        last_reconnect_ = now;
        connect();
    }
}

void MqttHandler::publish(const char* key, const char* value) {
    if (!client_.connected()) return;

    char topic[96];
    snprintf(topic, sizeof(topic), "%s/%s", cfg_->mqtt_topic, key);
    client_.publish(topic, value);
}

#endif // NATIVE_TEST
