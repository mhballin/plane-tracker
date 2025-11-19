// src/DisplayManager.h
#pragma once

#include <Arduino.h>
#include <LovyanGFX.hpp>
#include <time.h>
#include "config/Config.h"
#include "models/Aircraft.h"
#include "models/WeatherData.h"

// Color definitions - Refined dark theme for better readability
#define TFT_BLACK   0x0000
#define TFT_WHITE   0xFFFF
#define TFT_RED     0xF800
#define TFT_GREEN   0x07E0
#define TFT_BLUE    0x001F
#define TFT_YELLOW  0xFFE0
#define TFT_CYAN    0x07FF
#define TFT_MAGENTA 0xF81F

// Enhanced dark theme
#define COLOR_BACKGROUND 0x0821  // Deeper charcoal (less blue tint)
#define COLOR_PANEL 0x2124       // Elevated surface (subtle contrast)
#define COLOR_TEXT 0xFFFF        // Pure white for primary text (max contrast)
#define COLOR_TEXT_DIM 0xBDF7    // Dimmed white for secondary text  
#define COLOR_SUBTEXT 0x8410     // Muted gray for labels
#define COLOR_ACCENT 0x04FF      // Vibrant cyan for interactive elements
#define COLOR_WARNING 0xFD20     // Warm orange for warnings
#define COLOR_SUCCESS 0x07E0     // Green for positive states
#define COLOR_DIVIDER 0x4208     // Subtle divider lines

// Layout constants for consistency
#define MARGIN_SMALL 8
#define MARGIN_MEDIUM 16
#define MARGIN_LARGE 24
#define STATUS_BAR_HEIGHT 36

// Official LovyanGFX config for Elecrow ESP32 5" HMI Display (WZ8048C050)
// Based on: https://github.com/lovyan03/LovyanGFX/blob/master/src/lgfx_user/LGFX_Elecrow_ESP32_Display_WZ8048C050.h
#define LGFX_USE_V1
#include <lgfx/v1/platforms/esp32s3/Panel_RGB.hpp>
#include <lgfx/v1/platforms/esp32s3/Bus_RGB.hpp>
#include <driver/i2c.h>

class LGFX : public lgfx::LGFX_Device {
public:
    lgfx::Bus_RGB _bus_instance;
    lgfx::Panel_RGB _panel_instance;
    lgfx::Light_PWM _light_instance;
    lgfx::Touch_GT911 _touch_instance;  // GT911 I2C capacitive touch (confirmed from examples)

    LGFX(void) {
        // Configure panel first (before bus)
        {
            auto cfg = _panel_instance.config();
            cfg.memory_width  = 800;
            cfg.memory_height = 480;
            cfg.panel_width   = 800;
            cfg.panel_height  = 480;
            cfg.offset_x      = 0;
            cfg.offset_y      = 0;
            _panel_instance.config(cfg);
        }
        
        // Enable PSRAM for framebuffer (needed for full 480x800x2 bytes)
        {
            auto cfg = _panel_instance.config_detail();
            cfg.use_psram = 2; // Use PSRAM for framebuffer
            _panel_instance.config_detail(cfg);
        }

        {
            auto cfg = _bus_instance.config();
            cfg.panel = &_panel_instance;

            cfg.pin_d0  = GPIO_NUM_8;  // B0
            cfg.pin_d1  = GPIO_NUM_3;  // B1
            cfg.pin_d2  = GPIO_NUM_46; // B2
            cfg.pin_d3  = GPIO_NUM_9;  // B3
            cfg.pin_d4  = GPIO_NUM_1;  // B4

            cfg.pin_d5  = GPIO_NUM_5;  // G0
            cfg.pin_d6  = GPIO_NUM_6;  // G1
            cfg.pin_d7  = GPIO_NUM_7;  // G2
            cfg.pin_d8  = GPIO_NUM_15; // G3
            cfg.pin_d9  = GPIO_NUM_16; // G4
            cfg.pin_d10 = GPIO_NUM_4;  // G5

            cfg.pin_d11 = GPIO_NUM_45; // R0
            cfg.pin_d12 = GPIO_NUM_48; // R1
            cfg.pin_d13 = GPIO_NUM_47; // R2
            cfg.pin_d14 = GPIO_NUM_21; // R3
            cfg.pin_d15 = GPIO_NUM_14; // R4

            cfg.pin_henable = GPIO_NUM_40;
            cfg.pin_vsync   = GPIO_NUM_41;
            cfg.pin_hsync   = GPIO_NUM_39;
            cfg.pin_pclk    = GPIO_NUM_0;
            cfg.freq_write  = 15000000; // Elecrow official example uses 15 MHz

            cfg.hsync_polarity    = 0;
            cfg.hsync_front_porch = 8;
            cfg.hsync_pulse_width = 4;
            cfg.hsync_back_porch  = 43;

            cfg.vsync_polarity    = 0;
            cfg.vsync_front_porch = 8;
            cfg.vsync_pulse_width = 4;
            cfg.vsync_back_porch  = 12;

            cfg.pclk_active_neg = 1; // Matches Elecrow / LovyanGFX reference configuration
            cfg.de_idle_high    = 0;
            cfg.pclk_idle_high  = 0;

            _bus_instance.config(cfg);
        }
        _panel_instance.setBus(&_bus_instance);

        {
            auto cfg = _light_instance.config();
            cfg.pin_bl = GPIO_NUM_2;
            _light_instance.config(cfg);
        }
        _panel_instance.light(&_light_instance);

        // Configure touch controller (GT911 I2C capacitive - as used in official Elecrow examples)
        {
            auto cfg = _touch_instance.config();
            cfg.x_min = 0;
            cfg.x_max = 800;
            cfg.y_min = 0;
            cfg.y_max = 480;
            
            // I2C pins for GT911 (from official Elecrow LovyanGFX example)
            cfg.i2c_addr = 0x14;         // Official Elecrow example uses 0x14 (not 0x5D)
            cfg.pin_sda = GPIO_NUM_19;   // I2C SDA
            cfg.pin_scl = GPIO_NUM_20;   // I2C SCL  
            cfg.pin_int = GPIO_NUM_NC;   // No interrupt pin (per official example)
            cfg.pin_rst = GPIO_NUM_NC;   // No reset pin (per official example)
            cfg.i2c_port = I2C_NUM_1;
            cfg.freq = 400000;
            cfg.bus_shared = false;
            
            _touch_instance.config(cfg);
            _panel_instance.setTouch(&_touch_instance);
        }
        setPanel(&_panel_instance);
    }
};

// -----------------------------
// Display Manager
// -----------------------------
class DisplayManager {
public:
    // Screen state enum - public so main.cpp can query it
    enum ScreenState {
        SCREEN_HOME,           // Weather station with forecast, clock, plane counter
        SCREEN_AIRCRAFT_DETAIL,// Individual aircraft details
        SCREEN_NO_AIRCRAFT     // No planes detected message
    };

private:
    LGFX* lcd;
    unsigned long lastDisplayUpdate;
    uint8_t currentBrightness;
    String lastCallsign;  // Track last displayed aircraft to prevent flicker
    float lastWeatherTemp;  // Track last weather to prevent flicker
    String lastWeatherCondition;
    float lastWeatherHumidity;
    float lastWeatherPressure;
    String lastWeatherDescription;
    
    // Screen state management
    ScreenState currentScreen;
    unsigned long lastScreenChange;
    unsigned long lastUserInteraction;
    int currentAircraftCount;  // Track how many aircraft we have
    
    // Touch & gesture detection
    int touchStartX;
    int touchStartY;
    int touchLastX;
    int touchLastY;
    bool touchActive;
    unsigned long touchStartTime;
    bool gestureInProgress;

    // UI helpers
    void drawHeader(const char* title);
    void drawCard(int x, int y, int w, int h, const char* title);
    void drawDivider(int y);
    void drawStatusBar();
    void drawMetricCard(int x, int y, int w, int h, const char* label, const String& primaryValue, const String& secondaryValue = "");
    void drawForecastDay(int x, int y, const char* day, const char* icon, const char* low, const char* high);
    void drawPlaneCounter(int x, int y, int count);
    
    // Weather icons (vector-based)
    void drawSunIcon(int x, int y, int size);
    void drawCloudIcon(int x, int y, int size);
    void drawRainIcon(int x, int y, int size);
    void drawPartlyCloudyIcon(int x, int y, int size);
    void drawPlaneIcon(int x, int y, int size);
    
    // Weather icon selector
    void drawWeatherIcon(int x, int y, int size, const String& condition);

    void resetCachedData();

    // Screens
    void showClock();
    void showWeather(const WeatherData& weather);
    void showPlane(const Aircraft& aircraft);
    void showNoPlanes();
    void showRadar(const Aircraft& aircraft);
    
    // Gesture handling
    void checkForGestures();
    void handleSwipeLeft();
    void handleSwipeRight();
    void handleSwipeUp();
    void handleSwipeDown();
    bool detectSwipe(int& deltaX, int& deltaY);

    // Utilities
    void setBacklight(uint8_t brightness);
    void fadeTransition(bool fadeIn);
    String formatTime(const struct tm& timeinfo);
    String formatDate(const struct tm& timeinfo);

    // New UI internal state
    String m_statusMsg;
    unsigned long m_statusSetAt;
    time_t m_lastUpdateTime;
    unsigned long m_planesViewUntil;
    unsigned long m_lastAcceptedTap;


public:
    DisplayManager();
    ~DisplayManager();

    bool initialize();
    void update(const WeatherData& weather, const Aircraft* aircraft = nullptr, int aircraftCount = 0);
    void setBrightness(uint8_t brightness);
    void adjustBrightness(int delta);  // Adjust by delta amount
    void clear();
    void showError(const char* title, const char* message);
    
    // Screen state management
    ScreenState getCurrentScreen() const { return currentScreen; }
    void setScreen(ScreenState screen);
    bool shouldReturnToHome() const;  // Check if idle timeout reached
    void resetIdleTimer();  // Reset when user interacts
    
    // Gesture processing - call this frequently from main loop
    void processTouch();
    
    // Status bar / last update time
    void setStatusMessage(const String& msg);
    void setLastUpdateTimestamp(time_t epochSecs);

    LGFX* getDisplay() { return lcd; }
};