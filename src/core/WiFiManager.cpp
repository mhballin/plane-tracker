// src/core/WiFiManager.cpp
#include "core/WiFiManager.h"
#include "config/Config.h"
#include <time.h>

namespace core {

WiFiManager::WiFiManager()
    : lastReconnectAttemptMs_(0)
    , reconnectedFlag_(false) {}

bool WiFiManager::connect() {
    Serial.printf("[WiFi] Connecting to SSID: \"%s\"\n", Config::WIFI_SSID);
    WiFi.persistent(false);   // don't write credentials to NVS — avoids stale state across reflashes
    WiFi.disconnect(true);    // clear any lingering NVS/driver state from previous firmware
    delay(100);
    WiFi.mode(WIFI_STA);
    WiFi.begin(Config::WIFI_SSID, Config::WIFI_PASSWORD);

    uint32_t started = millis();
    while (WiFi.status() != WL_CONNECTED) {
        if ((millis() - started) > Config::WIFI_TIMEOUT_MS) {
            Serial.println("[WiFi] Connection timeout");
            return false;
        }
        delay(300);
    }

    Serial.printf("[WiFi] Connected: %s\n", WiFi.localIP().toString().c_str());
    configTime(Config::GMT_OFFSET_SEC, Config::DAYLIGHT_OFFSET_SEC, Config::NTP_SERVER);
    return true;
}

void WiFiManager::tick(uint32_t nowMs) {
    if (WiFi.status() == WL_CONNECTED) {
        return;
    }

    if ((nowMs - lastReconnectAttemptMs_) < Config::WIFI_RECONNECT_INTERVAL) {
        return;
    }

    lastReconnectAttemptMs_ = nowMs;
    Serial.println("[WiFi] Disconnected — attempting reconnect");

    if (connect()) {
        reconnectedFlag_ = true;
    }
}

bool WiFiManager::isConnected() const {
    return WiFi.status() == WL_CONNECTED;
}

String WiFiManager::localIP() const {
    return WiFi.localIP().toString();
}

bool WiFiManager::justReconnected() {
    if (reconnectedFlag_) {
        reconnectedFlag_ = false;
        return true;
    }
    return false;
}

}  // namespace core
