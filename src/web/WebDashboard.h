#pragma once

#include <Arduino.h>
#include <WebServer.h>

#include "core/HealthMonitor.h"
#include "models/WeatherData.h"

namespace web {

class WebDashboard {
public:
    WebDashboard();

    bool begin(uint16_t port = 80);
    void loop();

    void update(const core::HealthSnapshot& health, const WeatherData& weather, int aircraftCount);

private:
    void registerRoutes();
    void handleIndex();
    void handleStatusJson();

    WebServer server_;
    core::HealthSnapshot healthCache_;
    WeatherData weatherCache_;
    int aircraftCountCache_;
    bool started_;
};

}  // namespace web
