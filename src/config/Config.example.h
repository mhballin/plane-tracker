// src/config/Config.example.h
// Example configuration for ESP32 Plane Tracker
// 1) Copy this file to src/config/Config.h
// 2) Fill in your real credentials/keys
// 3) Do NOT commit src/config/Config.h to version control

#pragma once
#include <Arduino.h>

namespace Config {
    // ========================================
    // WiFi Credentials
    // ========================================
    constexpr char WIFI_SSID[] = "YOUR_WIFI_SSID";
    constexpr char WIFI_PASSWORD[] = "YOUR_WIFI_PASSWORD";
    constexpr int WIFI_TIMEOUT_MS = 20000; // 20 seconds to connect

    // ========================================
    // OpenWeather API Configuration
    // ========================================
    constexpr char WEATHER_API_KEY[] = "YOUR_OPENWEATHER_API_KEY";
    constexpr char WEATHER_CITY[] = "Portland,US"; // City,CountryCode

    // ========================================
    // OpenSky Network OAuth2 Configuration
    // ========================================
    constexpr char OPENSKY_CLIENT_ID[] = "your-opensky-client-id";
    constexpr char OPENSKY_CLIENT_SECRET[] = "your-opensky-client-secret";
    constexpr char OPENSKY_TOKEN_ENDPOINT[] = "https://auth.opensky-network.org/auth/realms/opensky-network/protocol/openid-connect/token";

    // ========================================
    // Home Location (defaults to Portland, Maine)
    // ========================================
    constexpr float HOME_LAT = 43.6591f;
    constexpr float HOME_LON = -70.2568f;

    // ========================================
    // Tracking Settings
    // ========================================
    constexpr float VISIBILITY_RANGE = 0.12f;  // ~13km radius (in degrees)
    constexpr int MAX_AIRCRAFT = 15;           // Maximum planes to track

    // ========================================
    // Update Intervals (milliseconds)
    // ========================================
    constexpr unsigned long WEATHER_UPDATE_INTERVAL = 1800000;  // 30 minutes
    constexpr unsigned long PLANE_UPDATE_INTERVAL = 60000;      // 1 minute
    constexpr unsigned long DISPLAY_UPDATE_INTERVAL = 1000;     // 1 second
    constexpr unsigned long TOKEN_LIFETIME = 1800000;           // 30 minutes
    constexpr unsigned long PLANE_DISPLAY_TIME = 5000;          // 5 seconds per plane

    // UI behavior
    constexpr unsigned long UI_AUTO_HOME_MS = 60000; // 60 seconds idle to return home
    constexpr unsigned long UI_STATUS_MS = 5000;     // 5 seconds status message
    constexpr int GESTURE_SWIPE_THRESHOLD = 80;
    constexpr int GESTURE_SWIPE_VELOCITY_MIN = 100;
    constexpr unsigned long GESTURE_MAX_TIME_MS = 500;
    constexpr unsigned long GESTURE_DEBOUNCE_MS = 300;

    // Brightness
    constexpr int BRIGHTNESS_STEP = 25;
    constexpr uint8_t BRIGHTNESS_MIN = 30;
    constexpr uint8_t BRIGHTNESS_MAX = 255;

    // Display presets
    constexpr uint8_t BRIGHTNESS_HIGH = 150;
    constexpr uint8_t BRIGHTNESS_LOW = 50;

    // ========================================
    // Time Configuration (NTP)
    // Tip: Prefer setting a proper TZ string at runtime for DST handling.
    // ========================================
    constexpr char NTP_SERVER[] = "pool.ntp.org";
    constexpr long GMT_OFFSET_SEC = -18000; // Default to Eastern Standard Time (UTC-5)
    constexpr int DAYLIGHT_OFFSET_SEC = 3600; // +1h for DST

    // ========================================
    // Debug Settings
    // ========================================
    constexpr bool DEBUG_SERIAL = true;
    constexpr bool DEBUG_API_RESPONSES = false;
}
