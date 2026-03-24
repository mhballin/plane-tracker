#include "core/HealthMonitor.h"

#include <WiFi.h>

namespace core {

HealthMonitor::HealthMonitor()
    : bootMs_(millis())
    , minFreeHeap_(ESP.getFreeHeap())
    , aircraftCount_(0)
    , lastWeatherSuccess_(0)
    , lastAircraftSuccess_(0)
    , statusMessage_("Booting") {
}

void HealthMonitor::tick(uint32_t nowMs) {
    (void)nowMs;
    uint32_t freeHeap = ESP.getFreeHeap();
    if (freeHeap < minFreeHeap_) {
        minFreeHeap_ = freeHeap;
    }
}

void HealthMonitor::setAircraftCount(uint16_t aircraftCount) {
    aircraftCount_ = aircraftCount;
}

void HealthMonitor::setStatusMessage(const String& message) {
    statusMessage_ = message;
}

void HealthMonitor::markWeatherSuccess(time_t timestamp) {
    lastWeatherSuccess_ = timestamp;
}

void HealthMonitor::markAircraftSuccess(time_t timestamp) {
    lastAircraftSuccess_ = timestamp;
}

HealthSnapshot HealthMonitor::snapshot() const {
    HealthSnapshot snap{};
    snap.uptimeSec = (millis() - bootMs_) / 1000;
    snap.freeHeap = ESP.getFreeHeap();
    snap.minFreeHeap = minFreeHeap_;
    snap.wifiRssi = (WiFi.status() == WL_CONNECTED) ? WiFi.RSSI() : 0;
    snap.aircraftCount = aircraftCount_;
    snap.lastWeatherSuccess = lastWeatherSuccess_;
    snap.lastAircraftSuccess = lastAircraftSuccess_;
    snap.statusMessage = statusMessage_;
    return snap;
}

}  // namespace core
