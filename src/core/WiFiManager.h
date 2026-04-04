// src/core/WiFiManager.h
#pragma once

#include <Arduino.h>
#include <WiFi.h>

namespace core {

class WiFiManager {
public:
    WiFiManager();

    // Attempt initial connection + NTP sync. Returns true on success.
    bool connect();

    // Call from App::tick(). Handles reconnect every WIFI_RECONNECT_INTERVAL ms.
    void tick(uint32_t nowMs);

    bool isConnected() const;
    String localIP() const;

    // One-shot flag: true for one tick() call after a successful reconnect.
    // Consumers should check this to re-init auth tokens etc.
    bool justReconnected();

private:
    uint32_t lastReconnectAttemptMs_;
    bool reconnectedFlag_;
};

}  // namespace core
