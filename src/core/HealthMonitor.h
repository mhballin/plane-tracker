#pragma once

#include <Arduino.h>
#include <time.h>

namespace core {

struct HealthSnapshot {
    uint32_t uptimeSec;
    uint32_t freeHeap;
    uint32_t minFreeHeap;
    int32_t wifiRssi;
    uint16_t aircraftCount;
    time_t lastWeatherSuccess;
    time_t lastAircraftSuccess;
    String statusMessage;
};

class HealthMonitor {
public:
    HealthMonitor();

    void tick(uint32_t nowMs);
    void setAircraftCount(uint16_t aircraftCount);
    void setStatusMessage(const String& message);
    void markWeatherSuccess(time_t timestamp);
    void markAircraftSuccess(time_t timestamp);
    HealthSnapshot snapshot() const;

private:
    uint32_t bootMs_;
    uint32_t minFreeHeap_;
    uint16_t aircraftCount_;
    time_t lastWeatherSuccess_;
    time_t lastAircraftSuccess_;
    String statusMessage_;
};

}  // namespace core
