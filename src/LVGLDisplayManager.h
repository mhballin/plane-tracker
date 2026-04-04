// src/LVGLDisplayManager.h
// LVGL-based Display Manager for ESP32 Plane Tracker
#pragma once

#include <Arduino.h>
#include <lvgl.h>
#define LGFX_USE_V1
#include <LovyanGFX.hpp>
#include "models/Aircraft.h"
#include "models/WeatherData.h"

// Use base LovyanGFX type (actual configured panel is in .cpp)
class LGFX_Panel;

class LVGLDisplayManager {
public:
    LVGLDisplayManager();
    ~LVGLDisplayManager();
    
    bool initialize();
    void update(const WeatherData& weather, const Aircraft* aircraft, int aircraftCount);
    void tick(uint32_t period_ms = 5);  // Call this frequently in loop()
    void setBrightness(uint8_t brightness);
    void setLastUpdateTimestamp(time_t timestamp);
    void setStatusMessage(const String& msg);
    
    // Screen management
    enum ScreenState {
        SCREEN_HOME,
        SCREEN_AIRCRAFT_DETAIL,
        SCREEN_NO_AIRCRAFT
    };
    
    void setScreen(ScreenState screen);
    ScreenState getCurrentScreen() const { return currentScreen; }
    
    // Accessors for compatibility
    lgfx::LGFX_Device* getDisplay();
    lgfx::LGFX_Device* getLCD();  // Alias for getDisplay()
    unsigned long getLastScreenChangeTime() const { return lastScreenChange; }
    bool shouldReturnToHome();
    void processTouch();  // For gesture processing if needed

private:
    LGFX_Panel* lcd;
    lv_display_t* lv_display;
    lv_indev_t* lv_indev;
    
    // Screens
    lv_obj_t* screen_home;
    lv_obj_t* screen_aircraft;
    lv_obj_t* screen_no_aircraft;
    
    // Home screen widgets
    lv_obj_t* label_time;
    lv_obj_t* label_date;
    lv_obj_t* label_temperature;
    lv_obj_t* label_weather_desc;
    lv_obj_t* label_feels_like;   // New
    lv_obj_t* label_temp_range;   // New
    lv_obj_t* label_humidity;
    lv_obj_t* label_wind;
    lv_obj_t* arc_humidity;
    lv_obj_t* label_sunrise;
    lv_obj_t* label_sunset;
    lv_obj_t* label_plane_count;
    lv_obj_t* btn_view_planes;
    
    // Forecast widgets
    struct ForecastRow {
        lv_obj_t* container;
        lv_obj_t* label_day;
        lv_obj_t* label_condition;
        lv_obj_t* label_temp;
    };
    ForecastRow forecast_rows[5];
    
    // Aircraft detail screen widgets
    lv_obj_t* label_callsign;
    lv_obj_t* label_aircraft_type;
    lv_obj_t* label_airline;      // New
    lv_obj_t* label_route;        // New
    lv_obj_t* label_altitude;
    lv_obj_t* label_velocity;
    lv_obj_t* label_heading;
    lv_obj_t* label_latitude;
    lv_obj_t* label_longitude;
    lv_obj_t* btn_back_home;
    
    // State
    ScreenState currentScreen;
    unsigned long lastScreenChange;
    unsigned long lastUserInteraction;
    String statusMessage;
    time_t lastUpdateTime;
    uint8_t currentBrightness;
    
    // LVGL callbacks (static)
    static void flush_cb(lv_display_t* disp, const lv_area_t* area, uint8_t* px_map);
    static void touchpad_read(lv_indev_t* indev, lv_indev_data_t* data);
    static void event_btn_view_planes(lv_event_t* e);
    static void event_btn_back_home(lv_event_t* e);
    
    // Screen builders
    void build_home_screen();
    void build_aircraft_screen();
    void build_no_aircraft_screen();

    // Home screen section builders (called by build_home_screen)
    void buildTopBar(lv_obj_t* screen);
    void buildWeatherCard(lv_obj_t* screen);
    void buildAircraftCard(lv_obj_t* screen);
    
    // Update functions
    void update_home_screen(const WeatherData& weather, int aircraftCount);
    void update_aircraft_screen(const Aircraft& aircraft);
    void update_clock();
    
    // Utilities
    String formatTime(time_t timestamp);
    String formatDate(time_t timestamp);
    const char* getWeatherIcon(const String& condition);
};
