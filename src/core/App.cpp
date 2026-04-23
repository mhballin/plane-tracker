#include "core/App.h"

#include <time.h>

#include "config/Config.h"

namespace core {

App::App()
    : display_(nullptr)
    , openSkyService_(nullptr)
    , weatherService_(nullptr)
    , webDashboard_(nullptr)
    , aircraftList_(nullptr)
    , currentAircraftCount_(0)
    , lastTickMs_(0)
    , weatherTaskId_(Scheduler::INVALID_TASK)
    , aircraftTaskId_(Scheduler::INVALID_TASK)
    , displayTaskId_(Scheduler::INVALID_TASK)
    , healthTaskId_(Scheduler::INVALID_TASK)
    , routeCache_(nullptr)
    , aircraftDismissed_(false)
    , serial_(nullptr) {
}

App::~App() {
    delete webDashboard_;
    delete weatherService_;
    delete openSkyService_;
    delete routeCache_;
    delete display_;
    delete[] aircraftList_;
}

bool App::begin() {
    Serial.begin(115200);
    delay(1000);

    Serial.println("\n\n=== ESP32 Plane Tracker v4 Rewrite ===");
    health_.setStatusMessage("Booting");

    aircraftList_ = new Aircraft[Config::MAX_AIRCRAFT];
    if (!aircraftList_) {
        Serial.println("[ERROR] Failed to allocate aircraft array");
        return false;
    }

    display_ = new LVGLDisplayManager();
    if (!display_) {
        Serial.println("[ERROR] Failed to allocate display manager");
        return false;
    }
    // Phase 1: hardware + LVGL task — must happen while internal SRAM is free
    if (!display_->initHardware()) {
        Serial.println("[ERROR] Display hardware init failed");
        return false;
    }

    // WiFi runs after the LVGL task is created but before screens are built.
    // This gives WiFi access to internal SRAM DMA buffers (not yet consumed by
    // the widget tree), while the task stack is already safely allocated.
    if (!wifiManager_.connect()) {
        Serial.println("[WiFi] Initial connection failed; will retry in tick()");
    }

    // Phase 2: build screens — large widget objects go to PSRAM via stdlib malloc
    if (!display_->buildScreens()) {
        Serial.println("[ERROR] Display screen build failed");
        return false;
    }

    serial_ = SerialCommandHandler(display_);

    if (!wifiManager_.isConnected()) {
        display_->setStatusMessage("WiFi failed - retrying...");
    }

    openSkyService_ = new OpenSkyService();
    weatherService_ = new WeatherService(Config::WEATHER_API_KEY, Config::WEATHER_CITY);
    webDashboard_ = new web::WebDashboard();

    if (!openSkyService_ || !weatherService_ || !webDashboard_) {
        Serial.println("[ERROR] Failed to allocate one or more core services");
        return false;
    }

    if (!openSkyService_->initialize()) {
        Serial.println("[WARN] OpenSky auth failed; API may be rate-limited");
    }

    webDashboard_->begin(80);

    routeCache_ = new RouteCache();

    setupTasks();

    // Prime data so first render/web response is useful.
    updateWeather();
    updateAircraft();
    updateDisplay();
    updateWebSnapshot();

    Serial.println("[INIT] v4 rewrite foundation ready");
    return true;
}

void App::tick() {
    uint32_t now = millis();

    uint32_t tickDelta = now - lastTickMs_;
    if (tickDelta > 0) {
        if (display_) {
            display_->tick(tickDelta);
        }
        lastTickMs_ = now;
    }

    serial_.tick();
    applyNightMode();

    wifiManager_.tick(now);
    if (wifiManager_.justReconnected()) {
        if (display_) display_->setStatusMessage("WiFi reconnected");
        if (openSkyService_) openSkyService_->initialize();
    }

    if (display_) {
        display_->processTouch();
    }

    if (display_) {
        if (display_->wasUserDismissed()) {
            aircraftDismissed_ = true;
        }

        // Reset dismissed flag once the sky clears
        if (currentAircraftCount_ == 0) {
            aircraftDismissed_ = false;
        }

        // Auto-switch to radar when aircraft detected; back to home when clear
        if (currentAircraftCount_ > 0 && !aircraftDismissed_
                && display_->getCurrentScreen() != LVGLDisplayManager::SCREEN_RADAR) {
            display_->setScreen(LVGLDisplayManager::SCREEN_RADAR);
        }

        if (currentAircraftCount_ == 0
                && display_->getCurrentScreen() == LVGLDisplayManager::SCREEN_RADAR) {
            display_->setScreen(LVGLDisplayManager::SCREEN_HOME);
        }
    }

    if (scheduler_.due(weatherTaskId_, now)) {
        updateWeather();
        scheduler_.markRun(weatherTaskId_, now);
    }

    if (scheduler_.due(aircraftTaskId_, now)) {
        updateAircraft();
        scheduler_.markRun(aircraftTaskId_, now);
    }

    if (scheduler_.due(displayTaskId_, now)) {
        updateDisplay();
        scheduler_.markRun(displayTaskId_, now);
    }

    if (scheduler_.due(healthTaskId_, now)) {
        health_.tick(now);
        updateWebSnapshot();
        scheduler_.markRun(healthTaskId_, now);
    }

    if (webDashboard_) {
        webDashboard_->loop();
    }

    delay(Config::TICK_DELAY_MS);
}

void App::setupTasks() {
    weatherTaskId_ = scheduler_.addTask(Config::WEATHER_UPDATE_INTERVAL, false);
    aircraftTaskId_ = scheduler_.addTask(Config::PLANE_UPDATE_INTERVAL, false);
    displayTaskId_ = scheduler_.addTask(Config::DISPLAY_UPDATE_INTERVAL, true);
    healthTaskId_ = scheduler_.addTask(Config::HEALTH_UPDATE_INTERVAL, true);
}

void App::updateWeather() {
    if (!weatherService_ || !wifiManager_.isConnected()) {
        return;
    }

    if (weatherService_->getWeather(currentWeather_)) {
        time_t nowTs = time(nullptr);
        if (display_) {
            display_->setLastUpdateTimestamp(nowTs);
            display_->setStatusMessage("Weather updated");
        }
        health_.markWeatherSuccess(nowTs);
        health_.setStatusMessage("Weather OK");
    } else {
        String err = weatherService_->getLastError();
        if (display_) {
            display_->setStatusMessage(err.isEmpty() ? "Weather failed" : err);
        }
        health_.setStatusMessage(err.isEmpty() ? "Weather failed" : err);
    }
}

void App::updateAircraft() {
    if (!openSkyService_ || !aircraftList_ || !wifiManager_.isConnected()) {
        currentAircraftCount_ = 0;
        health_.setAircraftCount(0);
        return;
    }

    currentAircraftCount_ = openSkyService_->fetchAircraft(aircraftList_, Config::MAX_AIRCRAFT);
    if (currentAircraftCount_ < 0) {
        currentAircraftCount_ = 0;
    }

    health_.setAircraftCount(static_cast<uint16_t>(currentAircraftCount_));
    if (currentAircraftCount_ > 0) {
        health_.markAircraftSuccess(time(nullptr));
        health_.setStatusMessage("Aircraft OK");
    } else {
        health_.setStatusMessage("No aircraft");
    }
}

void App::updateDisplay() {
    if (!display_) return;

    LVGLDisplayManager::ScreenState screen = display_->getCurrentScreen();

    if (screen == LVGLDisplayManager::SCREEN_HOME) {
        display_->update(currentWeather_, nullptr, currentAircraftCount_);
        return;
    }

    if (screen == LVGLDisplayManager::SCREEN_RADAR) {
        if (routeCache_ && wifiManager_.isConnected()) {
            for (int i = 0; i < currentAircraftCount_; i++) {
                Aircraft& a = aircraftList_[i];
                if (!a.valid || a.callsign.isEmpty() || a.routeLookupDone) continue;
                String org, dst, orgCity, orgCountry, dstCity, dstCountry;
                if (routeCache_->lookup(a.callsign, org, dst, orgCity, orgCountry,
                                        dstCity, dstCountry)) {
                    a.origin             = org;
                    a.destination        = dst;
                    a.originDisplay      = orgCity.isEmpty() ? org : orgCity;
                    a.destinationDisplay = dstCity.isEmpty() ? dst : dstCity;
                }
                a.routeLookupDone = true;
                break;  // one lookup per update cycle — don't block the main loop
            }
        }
        display_->update(currentWeather_, aircraftList_, currentAircraftCount_);
        return;
    }

    display_->update(currentWeather_, nullptr, 0);
}

void App::applyNightMode() {
    if (!display_) {
        return;
    }

    struct tm timeInfo;
    if (!getLocalTime(&timeInfo)) {
        return;
    }

    bool isNight = (timeInfo.tm_hour >= Config::NIGHT_START_HOUR) || (timeInfo.tm_hour < Config::NIGHT_END_HOUR);
    static bool lastNight = false;

    if (isNight != lastNight) {
        if (isNight) {
            display_->setBrightness(Config::NIGHT_BRIGHTNESS);
        } else {
            display_->setBrightness(Config::BRIGHTNESS_MAX);
        }
        lastNight = isNight;
    }
}

void App::updateWebSnapshot() {
    if (!webDashboard_) {
        return;
    }

    webDashboard_->update(health_.snapshot(), currentWeather_, currentAircraftCount_);
}

}  // namespace core
