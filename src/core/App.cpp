#include "core/App.h"

#include <time.h>
#include <SPI.h>
#include <SD.h>

#include "config/Config.h"
#include "hal/ElecrowDisplayProfile.h"

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
    , debugLastSummaryMs_(0)
    , debugMaxFreezeMs_(0)
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
    uint32_t wifiStartMs = millis();
    if (display_) display_->freezeRendering();
    bool wifiOk = wifiManager_.connect();
    if (display_) {
        uint32_t heldMs = millis() - wifiStartMs;
        if (heldMs > debugMaxFreezeMs_) debugMaxFreezeMs_ = heldMs;
        display_->unfreezeRendering();
    }
    if (Config::DEBUG_TIMING_LOGS) {
        Serial.printf("[TIMING] WiFi connect in begin: %lu ms  ok=%d\n",
                      millis() - wifiStartMs, wifiOk ? 1 : 0);
    }
    if (!wifiOk) {
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

    uint32_t authStartMs = millis();
    if (display_) display_->freezeRendering();
    bool authOk = openSkyService_->initialize();
    if (display_) {
        uint32_t heldMs = millis() - authStartMs;
        if (heldMs > debugMaxFreezeMs_) debugMaxFreezeMs_ = heldMs;
        display_->unfreezeRendering();
    }
    if (Config::DEBUG_TIMING_LOGS) {
        Serial.printf("[TIMING] OpenSky init auth in begin: %lu ms  ok=%d\n",
                      millis() - authStartMs, authOk ? 1 : 0);
    }
    if (!authOk) {
        Serial.println("[WARN] OpenSky auth failed; API may be rate-limited");
    }

    webDashboard_->begin();

    routeCache_ = new RouteCache();

    // Init SD card on the dedicated SPI bus (GPIO 10-13, standard CrowPanel wiring).
    // Runs after WiFi so SPI bus arbitration is settled; SD doesn't need network.
    SPI.begin(hal::Elecrow5Inch::SD_SCK,
              hal::Elecrow5Inch::SD_MISO,
              hal::Elecrow5Inch::SD_MOSI,
              hal::Elecrow5Inch::SD_CS);
    bool sdOk = SD.begin(hal::Elecrow5Inch::SD_CS, SPI);
    if (sdOk) {
        Serial.printf("[SD] Card mounted (%llu MB)\n",
                      SD.cardSize() / (1024ULL * 1024ULL));
    } else {
        Serial.println("[SD] Card not found — routes use session RAM only");
    }
    routeCache_->setSDReady(sdOk);

    setupTasks();

    // Tasks registered with runImmediately=true so the first App::tick() fires
    // all four update functions immediately, without blocking setup().

    // Reset runtime freeze baseline after boot so summaries reflect steady-state behavior.
    debugMaxFreezeMs_ = 0;
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

    if (wifiManager_.isConnected()) {
        wifiManager_.tick(now);
    } else {
        uint32_t reconnectStartMs = millis();
        if (display_) display_->freezeRendering();
        wifiManager_.tick(now);
        if (display_) {
            uint32_t heldMs = millis() - reconnectStartMs;
            if (heldMs > debugMaxFreezeMs_) debugMaxFreezeMs_ = heldMs;
            display_->unfreezeRendering();
        }
        if (Config::DEBUG_TIMING_LOGS) {
            Serial.printf("[TIMING] WiFi reconnect attempt window: %lu ms\n",
                          millis() - reconnectStartMs);
        }
    }
    if (display_) display_->setWifiConnected(wifiManager_.isConnected());
    if (wifiManager_.justReconnected()) {
        if (display_) display_->setStatusMessage("WiFi reconnected");
        if (openSkyService_) {
            uint32_t reauthStartMs = millis();
            if (display_) display_->freezeRendering();
            bool reauthOk = openSkyService_->initialize();
            if (display_) {
                uint32_t heldMs = millis() - reauthStartMs;
                if (heldMs > debugMaxFreezeMs_) debugMaxFreezeMs_ = heldMs;
                display_->unfreezeRendering();
            }
            if (Config::DEBUG_TIMING_LOGS) {
                Serial.printf("[TIMING] OpenSky re-auth on reconnect: %lu ms  ok=%d\n",
                              millis() - reauthStartMs, reauthOk ? 1 : 0);
            }
        }
    }

    if (display_) {
        if (display_->wasUserDismissed()) {
            aircraftDismissed_ = true;
            display_->setScreen(LVGLDisplayManager::SCREEN_HOME);
        }

        if (display_->wasUserRequestedRadar()) {
            display_->setScreen(LVGLDisplayManager::SCREEN_RADAR);
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
        // Immediately refresh display with new aircraft data and trigger route/type lookups
        updateDisplay();
        scheduler_.markRun(displayTaskId_, now);
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

    if (Config::DEBUG_TIMING_LOGS && (now - debugLastSummaryMs_) >= Config::DEBUG_SUMMARY_INTERVAL_MS) {
        debugLastSummaryMs_ = now;
        Serial.printf("[SUMMARY] heap=%lu max_heap=%lu psram_free=%lu max_freeze=%lums aircraft=%d wifi=%d\n",
                      (unsigned long)ESP.getFreeHeap(),
                      (unsigned long)ESP.getMaxAllocHeap(),
                      (unsigned long)ESP.getFreePsram(),
                      (unsigned long)debugMaxFreezeMs_,
                      currentAircraftCount_,
                      wifiManager_.isConnected() ? 1 : 0);
    }

    delay(Config::TICK_DELAY_MS);
}

void App::setupTasks() {
    weatherTaskId_ = scheduler_.addTask(Config::WEATHER_UPDATE_INTERVAL, true);
    // Offset aircraft fetch by 5s from display update cycle (display: 0,10,20,30...; aircraft: 5,35,65...)
    // This makes data arrive *between* display ticks, reducing visible "blip" when freeze ends + render happens.
    aircraftTaskId_ = scheduler_.addTask(Config::PLANE_UPDATE_INTERVAL, true, 5000);
    displayTaskId_ = scheduler_.addTask(Config::DISPLAY_UPDATE_INTERVAL, true);
    healthTaskId_ = scheduler_.addTask(Config::HEALTH_UPDATE_INTERVAL, true);
}

void App::updateWeather() {
    if (!weatherService_ || !wifiManager_.isConnected()) {
        return;
    }

    uint32_t t0 = millis();
    if (display_) display_->freezeRendering();
    bool weatherOk = weatherService_->getWeather(currentWeather_);
    if (display_) {
        uint32_t heldMs = millis() - t0;
        if (heldMs > debugMaxFreezeMs_) debugMaxFreezeMs_ = heldMs;
        display_->unfreezeRendering();
    }
    if (Config::DEBUG_TIMING_LOGS) {
        Serial.printf("[TIMING] Weather fetch: %lu ms  ok=%d\n", millis() - t0, weatherOk ? 1 : 0);
    }

    if (weatherOk) {
        if (display_) {
            display_->setStatusMessage("Weather updated");
        }
        health_.markWeatherSuccess(time(nullptr));
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

    // fetchAircraft() resets routeLookupDone/typeLookupDone and clears all route
    // fields unconditionally. Snapshot resolved data by callsign so we can restore
    // it for aircraft that persist across the refresh instead of re-fetching.
    struct RouteSnapshot {
        String callsign, origin, destination, originDisplay, destinationDisplay, aircraftType;
        bool routeLookupDone, typeLookupDone;
    };
    RouteSnapshot snapshots[Config::MAX_AIRCRAFT];
    int snapCount = 0;
    for (int i = 0; i < currentAircraftCount_ && i < Config::MAX_AIRCRAFT; i++) {
        const Aircraft& a = aircraftList_[i];
        if (a.valid && !a.callsign.isEmpty()) {
            snapshots[snapCount++] = {
                a.callsign, a.origin, a.destination,
                a.originDisplay, a.destinationDisplay, a.aircraftType,
                a.routeLookupDone, a.typeLookupDone
            };
        }
    }

    Serial.printf("[App] aircraft fetch BEGIN t=%lu ms\n", millis());
    uint32_t t0 = millis();
    if (display_) display_->freezeRendering();
    int newCount = openSkyService_->fetchAircraft(aircraftList_, Config::MAX_AIRCRAFT);
    if (display_) {
        uint32_t heldMs = millis() - t0;
        if (heldMs > debugMaxFreezeMs_) debugMaxFreezeMs_ = heldMs;
        display_->unfreezeRendering();
    }
    Serial.printf("[App] aircraft fetch END   t=%lu ms  count=%d\n", millis(), newCount);
    if (Config::DEBUG_TIMING_LOGS) {
        Serial.printf("[TIMING] Aircraft fetch: %lu ms  count=%d\n", millis() - t0, newCount);
    }

    currentAircraftCount_ = newCount < 0 ? 0 : newCount;

    // Restore route/type data for aircraft still in range after the refresh
    for (int i = 0; i < currentAircraftCount_; i++) {
        Aircraft& a = aircraftList_[i];
        if (a.callsign.isEmpty()) continue;
        for (int j = 0; j < snapCount; j++) {
            if (snapshots[j].callsign == a.callsign) {
                a.origin             = snapshots[j].origin;
                a.destination        = snapshots[j].destination;
                a.originDisplay      = snapshots[j].originDisplay;
                a.destinationDisplay = snapshots[j].destinationDisplay;
                a.aircraftType       = snapshots[j].aircraftType;
                a.routeLookupDone    = snapshots[j].routeLookupDone;
                a.typeLookupDone     = snapshots[j].typeLookupDone;
                break;
            }
        }
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
        if (Config::DEBUG_ISOLATE_ROUTE_TYPE_LOOKUPS) {
            static bool loggedIsolation = false;
            if (!loggedIsolation) {
                Serial.println("[ISOLATION] Route/type lookups disabled for root-cause investigation");
                loggedIsolation = true;
            }
            display_->update(currentWeather_, aircraftList_, currentAircraftCount_);
            return;
        }

        if (!Config::ENABLE_ROUTE_TYPE_LOOKUPS) {
            display_->update(currentWeather_, aircraftList_, currentAircraftCount_);
            return;
        }

        if (routeCache_ && wifiManager_.isConnected()) {
            uint32_t budgetStartMs = millis();
            for (int i = 0; i < currentAircraftCount_; i++) {
                if ((millis() - budgetStartMs) >= Config::DEBUG_LOOKUP_BUDGET_MS) {
                    if (Config::DEBUG_TIMING_LOGS) {
                        Serial.printf("[TIMING] Lookup budget hit after %lu ms; deferring remaining entries\n",
                                      millis() - budgetStartMs);
                    }
                    break;
                }

                Aircraft& a = aircraftList_[i];
                if (!a.valid || a.callsign.isEmpty()) continue;

                if (!a.routeLookupDone) {
                    String org, dst, orgCity, orgCountry, dstCity, dstCountry;
                    uint32_t t0 = millis();
                    display_->freezeRendering();
                    bool routeFound = routeCache_->lookup(a.callsign, org, dst, orgCity, orgCountry,
                                                         dstCity, dstCountry);
                    uint32_t lookupMs = millis() - t0;
                    if (lookupMs > debugMaxFreezeMs_) debugMaxFreezeMs_ = lookupMs;
                    display_->unfreezeRendering();
                    if (Config::DEBUG_TIMING_LOGS) {
                        Serial.printf("[TIMING] Route lookup callsign=%s ms=%lu found=%d\n",
                                      a.callsign.c_str(), lookupMs, routeFound ? 1 : 0);
                    }
                    if (routeFound) {
                        a.origin             = org;
                        a.destination        = dst;
                        a.originDisplay      = orgCity.isEmpty() ? org
                                             : (orgCountry.isEmpty() ? orgCity : orgCity + ", " + orgCountry);
                        a.destinationDisplay = dstCity.isEmpty() ? dst
                                             : (dstCountry.isEmpty() ? dstCity : dstCity + ", " + dstCountry);
                    }
                    a.routeLookupDone = true;
                    break;
                }

                if (!a.typeLookupDone && !a.icao24.isEmpty()) {
                    String typeStr;
                    uint32_t t0 = millis();
                    display_->freezeRendering();
                    bool typeFound = routeCache_->lookupType(a.icao24, typeStr);
                    uint32_t lookupMs = millis() - t0;
                    if (lookupMs > debugMaxFreezeMs_) debugMaxFreezeMs_ = lookupMs;
                    display_->unfreezeRendering();
                    if (Config::DEBUG_TIMING_LOGS) {
                        Serial.printf("[TIMING] Type lookup icao24=%s ms=%lu found=%d\n",
                                      a.icao24.c_str(), lookupMs, typeFound ? 1 : 0);
                    }
                    if (typeFound) {
                        a.aircraftType = typeStr;
                    }
                    a.typeLookupDone = true;
                    break;
                }
            }
        }
        display_->update(currentWeather_, aircraftList_, currentAircraftCount_);
        return;
    }

    display_->update(currentWeather_, nullptr, 0);
}

void App::applyNightMode() {
    static unsigned long lastCheck = 0;
    if (millis() - lastCheck < 60000) return;
    lastCheck = millis();

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
