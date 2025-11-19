// src/main.cpp
// ESP32 Plane Tracker - Main Application Loop

#include <Arduino.h>
#include <Wire.h>
#include <WiFi.h>
#include <time.h>
#include "config/Config.h"
#include "DisplayManager.h"
#include "services/OpenSkyService.h"
#include "services/WeatherService.h"
#include "models/Aircraft.h"
#include "models/WeatherData.h"

// Global instances
static DisplayManager* display = nullptr;
static OpenSkyService* openSkyService = nullptr;
static WeatherService* weatherService = nullptr;
static Aircraft* aircraftList = nullptr;
static WeatherData currentWeather;

// State tracking
static int currentAircraftCount = 0;
static int currentAircraftIndex = 0;
static unsigned long lastPlaneSwitch = 0;

// Function declarations
bool connectWiFi();
void updateWeather();
void updateAircraft();
void updateDisplay();

// Enable this to run an automatic I2C scan once at boot and print results to serial
#define AUTO_I2C_SCAN 1

// When SMOKE_TEST is enabled we compile a separate setup/loop in display_smoke_test.cpp
#ifndef SMOKE_TEST
void setup() {
    Serial.begin(115200);
    delay(1000);
    
    Serial.println("\n\n=== ESP32 Plane Tracker v3.0 ===");
    Serial.println("[BOOT] Initializing system...");

    // Allocate memory
    aircraftList = new Aircraft[Config::MAX_AIRCRAFT];
    if (!aircraftList) {
        Serial.println("[ERROR] Failed to allocate aircraft array");
        while(1) delay(1000);
    }
    
    // Initialize display first (so we can show status)
    display = new DisplayManager();
    if (!display) {
        Serial.println("[ERROR] Display allocation failed");
        while(1) delay(1000);
    }
    
    Serial.println("[INIT] Initializing display...");
    if (!display->initialize()) {
        Serial.println("[ERROR] Display initialization failed");
        while(1) delay(1000);
    }
    Serial.println("[INIT] ✅ Display ready");
    
    // IMPORTANT: Do NOT run I2C scans after display initialization!
    // The touch controller is already initialized and scanning disrupts it.
    // The early I2C scan before display init is sufficient for diagnostics.
    
    // Connect to WiFi with visual feedback
    display->showError("Connecting...", "Waiting for WiFi");
    if (!connectWiFi()) {
        display->showError("WiFi Error", "Failed to connect");
        Serial.println("[ERROR] WiFi connection failed");
        while(1) delay(1000);
    }
    Serial.println("[INIT] ✅ WiFi connected");
    
    // Configure NTP for local time
    Serial.println("[INIT] Syncing time with NTP...");
    configTime(Config::GMT_OFFSET_SEC, Config::DAYLIGHT_OFFSET_SEC, Config::NTP_SERVER);
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo, 10000)) {
        Serial.println("[WARN] Failed to obtain time from NTP");
    } else {
        Serial.printf("[INIT] ✅ Time synced: %02d:%02d:%02d\n", 
                     timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
    }
    
    // Initialize services
    openSkyService = new OpenSkyService();
    if (!openSkyService) {
        display->showError("Error", "OpenSky service failed");
        Serial.println("[ERROR] OpenSky allocation failed");
        while(1) delay(1000);
    }
    
    display->showError("Authenticating...", "OpenSky Network");
    if (!openSkyService->initialize()) {
        Serial.println("[WARN] OpenSky authentication failed - using anonymous access");
        // Continue anyway - anonymous access might work
    } else {
        Serial.println("[INIT] ✅ OpenSky authenticated");
    }
    
    weatherService = new WeatherService(Config::WEATHER_API_KEY, Config::WEATHER_CITY);
    if (!weatherService) {
        display->showError("Error", "Weather service failed");
        Serial.println("[ERROR] Weather service allocation failed");
        while(1) delay(1000);
    }
    Serial.println("[INIT] ✅ Weather service ready");
    
    // Initial data fetch
    display->showError("Loading...", "Fetching initial data");
    updateWeather();
    updateAircraft();
    
    Serial.println("[INIT] === System Ready ===\n");
    delay(1000);
    display->clear();
}

void loop() {
    unsigned long now = millis();
    // Track last screen change we have already redrawn for immediate screen transitions
    static unsigned long lastRedrawnScreenChange = 0;
    // Serial command parser: type 'CAL' + Enter in Serial Monitor to run touch calibration on demand
    static String _serialBuf = "";
    static bool _rawTouchMode = false;
    while (Serial.available()) {
        char c = (char)Serial.read();
        if (c == '\r' || c == '\n') {
            if (_serialBuf.length() > 0) {
                String cmd = _serialBuf;
                cmd.toUpperCase();
                if (cmd == "I2CSCAN") {
                    Serial.println("[CMD] Running I2C scanner...");
                    Wire.end();
                    Wire.begin(19, 20);
                    Wire.setClock(400000);
                    int nDevices = 0;
                    for (uint8_t address = 1; address < 127; ++address) {
                        Wire.beginTransmission(address);
                        uint8_t error = Wire.endTransmission();
                        if (error == 0) {
                            Serial.printf("I2C device found at 0x%02X\n", address);
                            nDevices++;
                        }
                    }
                    if (nDevices == 0) Serial.println("No I2C devices found");
                    else Serial.printf("I2C scan complete (%d devices)\n", nDevices);
                } else if (cmd == "RAW") {
                    _rawTouchMode = !_rawTouchMode;
                    Serial.printf("[CMD] Raw touch mode %s\n", _rawTouchMode ? "ON" : "OFF");
                }
            }
            _serialBuf = "";
        } else {
            _serialBuf += c;
            if (_serialBuf.length() > 64) _serialBuf = _serialBuf.substring(_serialBuf.length() - 64);
        }
    }

    // If raw touch mode is enabled, dump getTouch() samples to serial for debugging
    if (_rawTouchMode && display) {
        LGFX* lg = display->getDisplay();
        if (lg) {
            int tx=0, ty=0;
            if (lg->getTouch(&tx, &ty)) {
                Serial.printf("RAW_TOUCH: %d, %d\n", tx, ty);
            }
        }
        delay(50); // slow down prints
    }
    
    // Process touch gestures continuously for responsive UI
    if (display) {
        display->processTouch();
    }

    // If a screen change occurred since last redraw, force an immediate display update
    if (display) {
        unsigned long sc = display->getLastScreenChangeTime();
        if (sc != 0 && sc != lastRedrawnScreenChange) {
            updateDisplay();
            lastRedrawnScreenChange = sc;
            Serial.println("[DISPLAY] Immediate redraw after screen change");
        }
    }
    
    // Check if we should auto-return to home screen
    if (display && display->shouldReturnToHome()) {
        display->setScreen(DisplayManager::SCREEN_HOME);
        Serial.println("[UI] Auto-returning to home screen");
    }
    
    // Update weather periodically (every 30 minutes)
    static unsigned long lastWeatherUpdate = 0;
    if (now - lastWeatherUpdate >= Config::WEATHER_UPDATE_INTERVAL) {
        updateWeather();
        lastWeatherUpdate = now;
    }
    
    // Update aircraft data periodically (every 1 minute)
    static unsigned long lastAircraftUpdate = 0;
    if (now - lastAircraftUpdate >= Config::PLANE_UPDATE_INTERVAL) {
        updateAircraft();
        lastAircraftUpdate = now;
        if (display) display->setLastUpdateTimestamp(time(nullptr));
    }
    
    // Auto-cycle through aircraft when in aircraft view
    if (display && display->getCurrentScreen() == DisplayManager::SCREEN_AIRCRAFT_DETAIL) {
        if (currentAircraftCount > 1) {
            if (now - lastPlaneSwitch >= Config::PLANE_DISPLAY_TIME) {
                currentAircraftIndex = (currentAircraftIndex + 1) % currentAircraftCount;
                lastPlaneSwitch = now;
                Serial.printf("[DISPLAY] Auto-cycling to aircraft %d/%d\n", 
                             currentAircraftIndex + 1, currentAircraftCount);
            }
        }
    }
    
    // Update display (every second for smooth clock updates)
    static unsigned long lastDisplayUpdate = 0;
    if (now - lastDisplayUpdate >= Config::DISPLAY_UPDATE_INTERVAL) {
        updateDisplay();
        lastDisplayUpdate = now;
    }
    
    delay(50);  // Small delay to prevent watchdog issues
}
#endif // SMOKE_TEST

// ========================================
// WiFi Connection
// ========================================
bool connectWiFi() {
    Serial.printf("[WiFi] Connecting to '%s'...\n", Config::WIFI_SSID);
    
    WiFi.mode(WIFI_STA);
    WiFi.begin(Config::WIFI_SSID, Config::WIFI_PASSWORD);
    
    unsigned long startAttempt = millis();
    while (WiFi.status() != WL_CONNECTED) {
        if (millis() - startAttempt >= Config::WIFI_TIMEOUT_MS) {
            Serial.println("[WiFi] Connection timeout");
            return false;
        }
        delay(500);
        Serial.print(".");
    }
    
    Serial.println();
    Serial.printf("[WiFi] ✅ Connected! IP: %s\n", WiFi.localIP().toString().c_str());
    Serial.printf("[WiFi] Signal: %d dBm\n", WiFi.RSSI());
    
    return true;
}

// ========================================
// Weather Update
// ========================================
void updateWeather() {
    Serial.println("[Weather] Fetching weather data...");
    
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("[Weather] ⚠️ No WiFi connection");
        return;
    }
    
    if (weatherService && weatherService->getWeather(currentWeather)) {
        Serial.printf("[Weather] ✅ %.1f°F, %.0f%% RH, %s\n",
                     currentWeather.temperature,
                     currentWeather.humidity,
                     currentWeather.condition.c_str());
        if (display) {
            display->setLastUpdateTimestamp(time(nullptr));
            display->setStatusMessage("Weather updated");
        }
    } else {
        Serial.println("[Weather] ❌ Failed to fetch weather");
        if (display) {
            String err = weatherService ? weatherService->getLastError() : String("Weather service offline");
            if (err.isEmpty()) err = "Weather fetch failed";
            display->setStatusMessage(err);
        }
    }
}

// ========================================
// Aircraft Update
// ========================================
void updateAircraft() {
    Serial.println("[Aircraft] Scanning for planes...");
    
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("[Aircraft] ⚠️ No WiFi connection");
        currentAircraftCount = 0;
        return;
    }
    
    if (!openSkyService) {
        Serial.println("[Aircraft] ❌ Service not initialized");
        currentAircraftCount = 0;
        return;
    }
    
    // Fetch aircraft data
    currentAircraftCount = openSkyService->fetchAircraft(aircraftList, Config::MAX_AIRCRAFT);
    
    if (currentAircraftCount > 0) {
        Serial.printf("[Aircraft] ✅ Found %d aircraft in range\n", currentAircraftCount);
        
        // Reset display index if current index is now invalid
        if (currentAircraftIndex >= currentAircraftCount) {
            currentAircraftIndex = 0;
            lastPlaneSwitch = millis();
        }
        
        // Log details of first aircraft
        Aircraft& plane = aircraftList[0];
        Serial.printf("  └─ %s: %s at %.0f ft, %.0f kts\n",
                     plane.callsign.c_str(),
                     plane.aircraftType.c_str(),
                     plane.altitude * 3.28084f,  // m to ft
                     plane.velocity * 1.94384f); // m/s to kts
    } else {
        Serial.println("[Aircraft] No aircraft detected");
        currentAircraftIndex = 0;
        if (display) {
            String err = openSkyService->getLastError();
            if (err.isEmpty()) err = "No aircraft";
            display->setStatusMessage(err);
        }
    }
}

// ========================================
// Display Update
// ========================================
void updateDisplay() {
    if (!display) return;
    
    // Determine what screen to show based on current state
    DisplayManager::ScreenState currentScreen = display->getCurrentScreen();
    
    if (currentScreen == DisplayManager::SCREEN_HOME) {
        // Home screen - weather with plane counter
        display->update(currentWeather, nullptr, currentAircraftCount);
    } 
    else if (currentScreen == DisplayManager::SCREEN_AIRCRAFT_DETAIL) {
        // Aircraft detail screen
        if (currentAircraftCount > 0 && 
            currentAircraftIndex < currentAircraftCount &&
            aircraftList[currentAircraftIndex].valid) {
            display->update(currentWeather, &aircraftList[currentAircraftIndex], currentAircraftCount);
        } else {
            // No valid aircraft - switch to no aircraft screen
            display->setScreen(DisplayManager::SCREEN_NO_AIRCRAFT);
        }
    }
    else if (currentScreen == DisplayManager::SCREEN_NO_AIRCRAFT) {
        // No aircraft screen
        display->update(currentWeather, nullptr, 0);
    }
}