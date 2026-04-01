#pragma once

#include <cstdint>

// Hardware defaults
#define SERIAL_RX_PIN 13
#define SERIAL_BAUD   1200

// MQTT defaults
#define DEFAULT_MQTT_HOST     "localhost"
#define DEFAULT_MQTT_PORT     1883
#define DEFAULT_MQTT_TOPIC    "heating"
#define DEFAULT_MQTT_CLIENTID "buderus2mqtt"
#define DEFAULT_MQTT_KEEPALIVE 30

struct Config {
    char mqtt_host[64];
    uint16_t mqtt_port;
    char mqtt_user[32];
    char mqtt_password[64];
    char mqtt_topic[32];
    char mqtt_clientid[32];
    uint16_t mqtt_keepalive;
};

void configLoad(Config& cfg);
void configSave(const Config& cfg);
void configDefaults(Config& cfg);
