#include "core/App.h"

#include <Wire.h>
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
    , currentAircraftIndex_(0)
    , lastPlaneSwitchMs_(0)
    , lastTickMs_(0)
    , lastRedrawnScreenChange_(0)
    , weatherTaskId_(Scheduler::INVALID_TASK)
    , aircraftTaskId_(Scheduler::INVALID_TASK)
    , displayTaskId_(Scheduler::INVALID_TASK)
    , healthTaskId_(Scheduler::INVALID_TASK)
    , serialBuffer_("")
    , rawTouchMode_(false) {
}

App::~App() {
    delete webDashboard_;
    delete weatherService_;
    delete openSkyService_;
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
    if (!display_->initialize()) {
        Serial.println("[ERROR] Display initialization failed");
        return false;
    }

    display_->setStatusMessage("Connecting WiFi...");
    if (!wifiManager_.connect()) {
        display_->setStatusMessage("WiFi failed - retrying...");
        Serial.println("[WiFi] Initial connection failed; will retry in tick()");
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

    processSerialCommands();
    processRawTouchDump();
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
        uint32_t changedAt = display_->getLastScreenChangeTime();
        if (changedAt != 0 && changedAt != lastRedrawnScreenChange_) {
            updateDisplay();
            lastRedrawnScreenChange_ = changedAt;
        }

        if (display_->shouldReturnToHome()) {
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

    if (display_ && display_->getCurrentScreen() == LVGLDisplayManager::SCREEN_AIRCRAFT_DETAIL) {
        if (currentAircraftCount_ > 1 && (now - lastPlaneSwitchMs_) >= Config::PLANE_DISPLAY_TIME) {
            currentAircraftIndex_ = (currentAircraftIndex_ + 1) % currentAircraftCount_;
            lastPlaneSwitchMs_ = now;
        }
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

    if (currentAircraftIndex_ >= currentAircraftCount_) {
        currentAircraftIndex_ = 0;
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
    if (!display_) {
        return;
    }

    LVGLDisplayManager::ScreenState screen = display_->getCurrentScreen();

    if (screen == LVGLDisplayManager::SCREEN_HOME) {
        display_->update(currentWeather_, nullptr, currentAircraftCount_);
        return;
    }

    if (screen == LVGLDisplayManager::SCREEN_AIRCRAFT_DETAIL) {
        if (currentAircraftCount_ > 0 && currentAircraftIndex_ < currentAircraftCount_ && aircraftList_[currentAircraftIndex_].valid) {
            display_->update(currentWeather_, &aircraftList_[currentAircraftIndex_], currentAircraftCount_);
        } else {
            display_->setScreen(LVGLDisplayManager::SCREEN_NO_AIRCRAFT);
        }
        return;
    }

    display_->update(currentWeather_, nullptr, 0);
}

void App::processSerialCommands() {
    while (Serial.available()) {
        char c = static_cast<char>(Serial.read());
        if (c == '\n' || c == '\r') {
            if (serialBuffer_.isEmpty()) {
                continue;
            }

            serialBuffer_.toUpperCase();
            if (serialBuffer_ == "RAW") {
                rawTouchMode_ = !rawTouchMode_;
                Serial.printf("[CMD] RAW mode %s\n", rawTouchMode_ ? "ON" : "OFF");
            } else if (serialBuffer_ == "I2CSCAN") {
                Serial.println("[CMD] Running I2C scan (use cautiously after display init)");
                Wire.end();
                Wire.begin(19, 20);
                Wire.setClock(400000);
                int found = 0;
                for (uint8_t address = 1; address < 127; ++address) {
                    Wire.beginTransmission(address);
                    uint8_t err = Wire.endTransmission();
                    if (err == 0) {
                        Serial.printf("[I2C] 0x%02X\n", address);
                        found++;
                    }
                }
                Serial.printf("[I2C] Devices: %d\n", found);
            }
            serialBuffer_ = "";
            continue;
        }

        serialBuffer_ += c;
        if (serialBuffer_.length() > 64) {
            serialBuffer_ = serialBuffer_.substring(serialBuffer_.length() - 64);
        }
    }
}

void App::processRawTouchDump() {
    if (!rawTouchMode_ || !display_) {
        return;
    }

    auto* lcd = display_->getLCD();
    if (!lcd) {
        return;
    }

    int tx = 0;
    int ty = 0;
    if (lcd->getTouch(&tx, &ty)) {
        Serial.printf("RAW_TOUCH: %d, %d\n", tx, ty);
    }
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
