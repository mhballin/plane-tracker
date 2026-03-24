#pragma once

#include <Arduino.h>

#include "LVGLDisplayManager.h"
#include "core/HealthMonitor.h"
#include "core/Scheduler.h"
#include "models/Aircraft.h"
#include "models/WeatherData.h"
#include "services/OpenSkyService.h"
#include "services/WeatherService.h"
#include "web/WebDashboard.h"

namespace core {

class App {
public:
    App();
    ~App();

    bool begin();
    void tick();

private:
    bool connectWiFi();
    void setupTasks();

    void updateWeather();
    void updateAircraft();
    void updateDisplay();

    void processSerialCommands();
    void processRawTouchDump();
    void applyNightMode();
    void updateWebSnapshot();

    LVGLDisplayManager* display_;
    OpenSkyService* openSkyService_;
    WeatherService* weatherService_;
    web::WebDashboard* webDashboard_;
    Aircraft* aircraftList_;

    WeatherData currentWeather_;
    Scheduler scheduler_;
    HealthMonitor health_;

    int currentAircraftCount_;
    int currentAircraftIndex_;
    uint32_t lastPlaneSwitchMs_;
    uint32_t lastTickMs_;
    uint32_t lastRedrawnScreenChange_;

    int8_t weatherTaskId_;
    int8_t aircraftTaskId_;
    int8_t displayTaskId_;
    int8_t healthTaskId_;

    String serialBuffer_;
    bool rawTouchMode_;
};

}  // namespace core
