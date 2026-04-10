// src/LVGLDisplayManager.h
#pragma once

#include <Arduino.h>
#include <lvgl.h>
#define LGFX_USE_V1
#include <LovyanGFX.hpp>
#include "models/Aircraft.h"
#include "models/WeatherData.h"
#include "config/Config.h"

class LGFX_Panel;

class LVGLDisplayManager {
public:
    LVGLDisplayManager();
    ~LVGLDisplayManager();

    bool initialize();
    void update(const WeatherData& weather, const Aircraft* aircraft, int aircraftCount);
    void tick(uint32_t period_ms = 5);
    void setBrightness(uint8_t brightness);
    void setLastUpdateTimestamp(time_t timestamp);
    void setStatusMessage(const String& msg);

    enum ScreenState {
        SCREEN_HOME,
        SCREEN_AIRCRAFT_DETAIL,
        SCREEN_NO_AIRCRAFT  // treated as SCREEN_HOME internally
    };

    void setScreen(ScreenState screen);
    ScreenState getCurrentScreen() const { return currentScreen; }

    lgfx::LGFX_Device* getDisplay();
    lgfx::LGFX_Device* getLCD();
    unsigned long getLastScreenChangeTime() const { return lastScreenChange; }
    bool shouldReturnToHome();
    void processTouch();

private:
    LGFX_Panel* lcd;
    lv_display_t* lv_display;
    lv_indev_t*   lv_indev;

    // --- Screens ---
    lv_obj_t* screen_home;        // home with radar panel (aircraft present)
    lv_obj_t* screen_home_empty;  // home full-width weather (no aircraft)
    lv_obj_t* screen_aircraft;    // aircraft detail

    // --- Shared weather widget set (one per home screen variant) ---
    struct WeatherWidgets {
        lv_obj_t* label_time         = nullptr;
        lv_obj_t* label_date         = nullptr;
        lv_obj_t* label_temperature  = nullptr;
        lv_obj_t* label_weather_desc = nullptr;
        lv_obj_t* label_feels_like   = nullptr;
        lv_obj_t* label_temp_range   = nullptr;
        lv_obj_t* label_wind         = nullptr;
        lv_obj_t* label_humidity_val = nullptr;
        lv_obj_t* label_sunrise      = nullptr;
        lv_obj_t* label_sunset       = nullptr;
        lv_obj_t* label_status_left  = nullptr;  // "OPENSKY OK · Last: HH:MM"
        lv_obj_t* label_status_live  = nullptr;  // "● LIVE" / "● IDLE"
        struct ForecastRow {
            lv_obj_t* container  = nullptr;
            lv_obj_t* label_day  = nullptr;
            lv_obj_t* label_cond = nullptr;
            lv_obj_t* label_hi   = nullptr;
            lv_obj_t* label_lo   = nullptr;
        } forecast[5];
    };
    WeatherWidgets homeWidgets;   // aircraft-present screen
    WeatherWidgets emptyWidgets;  // no-aircraft screen

    // --- Radar panel widgets (screen_home only) ---
    lv_obj_t* radar_container;
    lv_obj_t* radar_blips[Config::MAX_AIRCRAFT];
    lv_obj_t* label_contact_count;
    lv_obj_t* btn_view_planes;

    // --- Aircraft detail screen widgets ---
    lv_obj_t* label_callsign;
    lv_obj_t* label_distance;       // "42.3 nm · 047° NE"
    lv_obj_t* label_airline;
    lv_obj_t* label_aircraft_type;
    lv_obj_t* label_squawk;
    lv_obj_t* label_route_main;     // "BOS → LAX" large
    lv_obj_t* label_route_sub;      // "Boston · Los Angeles"
    lv_obj_t* label_altitude;
    lv_obj_t* label_velocity;
    lv_obj_t* label_heading;
    lv_obj_t* label_vert_speed;     // color-coded fpm
    lv_obj_t* label_status_aircraft;
    lv_obj_t* btn_back_home;

    // --- State ---
    ScreenState currentScreen;
    bool homeHasAircraft;
    unsigned long lastScreenChange;
    unsigned long lastUserInteraction;
    String statusMessage;
    uint32_t statusClearTime;
    time_t lastUpdateTime;
    uint8_t currentBrightness;

    // --- LVGL callbacks ---
    static void flush_cb(lv_display_t* disp, const lv_area_t* area, uint8_t* px_map);
    static void touchpad_read(lv_indev_t* indev, lv_indev_data_t* data);
    static void event_btn_view_planes(lv_event_t* e);
    static void event_btn_back_home(lv_event_t* e);

    // --- Screen builders ---
    void build_home_screen();
    void build_home_empty_screen();
    void build_aircraft_screen();
    void build_no_aircraft_screen();  // backward-compat stub

    // --- Section builders ---
    void buildTopBar(lv_obj_t* screen, WeatherWidgets& w);
    void buildStatusBar(lv_obj_t* screen, WeatherWidgets& w);
    void buildWeatherPanel(lv_obj_t* parent, WeatherWidgets& w);
    void buildRadarPanel(lv_obj_t* parent);

    // --- Update functions ---
    void updateWeatherWidgets(WeatherWidgets& w, const WeatherData& weather, int aircraftCount);
    void update_home_screen(const WeatherData& weather, const Aircraft* aircraft, int aircraftCount);
    void update_aircraft_screen(const Aircraft& aircraft);
    void update_clock(WeatherWidgets& w);

    // --- Utilities ---
    String formatTime(time_t timestamp);
    String formatDate(time_t timestamp);
    const char* getWeatherIcon(const String& condition);
};
