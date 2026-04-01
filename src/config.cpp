#include "config.h"

#ifndef NATIVE_TEST

#include <Arduino.h>
#include <LittleFS.h>
#include <ArduinoJson.h>

void configDefaults(Config& cfg) {
    strlcpy(cfg.mqtt_host, DEFAULT_MQTT_HOST, sizeof(cfg.mqtt_host));
    cfg.mqtt_port = DEFAULT_MQTT_PORT;
    cfg.mqtt_user[0] = '\0';
    cfg.mqtt_password[0] = '\0';
    strlcpy(cfg.mqtt_topic, DEFAULT_MQTT_TOPIC, sizeof(cfg.mqtt_topic));
    strlcpy(cfg.mqtt_clientid, DEFAULT_MQTT_CLIENTID, sizeof(cfg.mqtt_clientid));
    cfg.mqtt_keepalive = DEFAULT_MQTT_KEEPALIVE;
}

void configLoad(Config& cfg) {
    configDefaults(cfg);

    if (!LittleFS.begin(true)) {
        Serial.println("[config] LittleFS mount failed");
        return;
    }

    File f = LittleFS.open("/config.json", "r");
    if (!f) {
        Serial.println("[config] No config file, using defaults");
        return;
    }

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, f);
    f.close();

    if (err) {
        Serial.printf("[config] JSON parse error: %s\n", err.c_str());
        return;
    }

    if (doc["mqtt_host"].is<const char*>())
        strlcpy(cfg.mqtt_host, doc["mqtt_host"], sizeof(cfg.mqtt_host));
    if (doc["mqtt_port"].is<int>())
        cfg.mqtt_port = doc["mqtt_port"];
    if (doc["mqtt_user"].is<const char*>())
        strlcpy(cfg.mqtt_user, doc["mqtt_user"], sizeof(cfg.mqtt_user));
    if (doc["mqtt_password"].is<const char*>())
        strlcpy(cfg.mqtt_password, doc["mqtt_password"], sizeof(cfg.mqtt_password));
    if (doc["mqtt_topic"].is<const char*>())
        strlcpy(cfg.mqtt_topic, doc["mqtt_topic"], sizeof(cfg.mqtt_topic));
    if (doc["mqtt_clientid"].is<const char*>())
        strlcpy(cfg.mqtt_clientid, doc["mqtt_clientid"], sizeof(cfg.mqtt_clientid));
    if (doc["mqtt_keepalive"].is<int>())
        cfg.mqtt_keepalive = doc["mqtt_keepalive"];

    Serial.printf("[config] Loaded: mqtt=%s:%d topic=%s\n", cfg.mqtt_host, cfg.mqtt_port, cfg.mqtt_topic);
}

void configSave(const Config& cfg) {
    JsonDocument doc;
    doc["mqtt_host"] = cfg.mqtt_host;
    doc["mqtt_port"] = cfg.mqtt_port;
    doc["mqtt_user"] = cfg.mqtt_user;
    doc["mqtt_password"] = cfg.mqtt_password;
    doc["mqtt_topic"] = cfg.mqtt_topic;
    doc["mqtt_clientid"] = cfg.mqtt_clientid;
    doc["mqtt_keepalive"] = cfg.mqtt_keepalive;

    File f = LittleFS.open("/config.json", "w");
    if (!f) {
        Serial.println("[config] Failed to open config file for writing");
        return;
    }

    serializeJson(doc, f);
    f.close();
    Serial.println("[config] Saved");
}

#endif // NATIVE_TEST
