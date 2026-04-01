#ifndef NATIVE_TEST

#include <Arduino.h>
#include <ETH.h>
#include <ArduinoOTA.h>
#include <esp_task_wdt.h>
#include <esp_idf_version.h>

#include "config.h"
#include "mqtt_handler.h"
#include "buderus_protocol.h"
#include "buderus_decoder.h"

static Config config;
static MqttHandler mqtt;
static BuderusProtocol protocol;
static BuderusDecoder decoder;

static volatile bool eth_connected = false;

static void onEthEvent(WiFiEvent_t event) {
    switch (event) {
        case ARDUINO_EVENT_ETH_GOT_IP:
            Serial.printf("[eth] IP: %s\n", ETH.localIP().toString().c_str());
            eth_connected = true;
            break;
        case ARDUINO_EVENT_ETH_DISCONNECTED:
            Serial.println("[eth] Disconnected");
            eth_connected = false;
            break;
        case ARDUINO_EVENT_ETH_START:
            Serial.println("[eth] Started");
            ETH.setHostname("buderus2mqtt");
            break;
        default:
            break;
    }
}

static void setupEth() {
    WiFi.onEvent(onEthEvent);
    ETH.begin();

    Serial.print("[eth] Waiting for link");
    unsigned long start = millis();
    while (!eth_connected && millis() - start < 30000) {
        Serial.print(".");
        delay(500);
    }
    Serial.println();

    if (!eth_connected) {
        Serial.println("[eth] No link after 30s, restarting...");
        ESP.restart();
    }
}

static void setupOTA() {
    ArduinoOTA.setHostname("buderus2mqtt");
    ArduinoOTA.onStart([]() { Serial.println("[ota] Start"); });
    ArduinoOTA.onEnd([]() { Serial.println("[ota] Done"); });
    ArduinoOTA.onError([](ota_error_t err) { Serial.printf("[ota] Error %u\n", err); });
    ArduinoOTA.begin();
}

void setup() {
    Serial.begin(115200);
    Serial.println("\n[buderus2mqtt] Starting...");

    configLoad(config);
    setupEth();
    setupOTA();

    mqtt.begin(config);

    Serial2.begin(SERIAL_BAUD, SERIAL_8N1, SERIAL_RX_PIN, -1);
    protocol.begin(Serial2);
    protocol.onRecord([](uint8_t recnum, const uint8_t* data, size_t len) {
        MqttMessage msgs[16];
        size_t n = decoder.dispatch(recnum, data, len, msgs, 16);
        for (size_t i = 0; i < n; i++) {
            mqtt.publish(msgs[i].key, msgs[i].value);
        }
    });

#if ESP_IDF_VERSION_MAJOR >= 5
    esp_task_wdt_config_t wdt_cfg = { .timeout_ms = 30000, .idle_core_mask = 0, .trigger_panic = true };
    esp_task_wdt_init(&wdt_cfg);
#else
    esp_task_wdt_init(30, true);
#endif
    esp_task_wdt_add(nullptr);

    Serial.println("[buderus2mqtt] Ready");
}

void loop() {
    esp_task_wdt_reset();
    ArduinoOTA.handle();
    mqtt.loop();
    protocol.loop();
}

#endif // NATIVE_TEST
