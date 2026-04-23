// src/LVGLDisplayManager.h
#pragma once

#include <Arduino.h>
#include <lvgl.h>
#define LGFX_USE_V1
#include <LovyanGFX.hpp>
#include <esp_timer.h>
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
        SCREEN_HOME,   // weather + quiet airspace panel
        SCREEN_RADAR,  // full radar + aircraft list
    };

    void setScreen(ScreenState screen);
    ScreenState getCurrentScreen() const { return currentScreen; }

    lgfx::LGFX_Device* getLCD();
    unsigned long getLastScreenChangeTime() const { return lastScreenChange; }
    bool shouldReturnToHome();
    void processTouch();

    // Returns true (and clears the flag) if the user tapped BACK since last call
    bool wasUserDismissed();

private:
    LGFX_Panel* lcd;
    lv_display_t* lv_display;
    lv_indev_t*   lv_indev;

    // --- Shared weather widget set ---
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
    // Radar blip on the radar screen (one per MAX_AIRCRAFT slot)
    struct RadarBlip {
        lv_obj_t*    dot    = nullptr;
        lv_obj_t*    vector = nullptr;  // lv_line, heading direction
        lv_obj_t*    label  = nullptr;  // callsign text
        lv_point_precise_t   vec_pts[2] = {};   // kept alive for lv_line
    };

    // One row in the aircraft list panel
    struct AircraftListRow {
        lv_obj_t* container      = nullptr;  // tappable row
        lv_obj_t* accent_bar     = nullptr;
        lv_obj_t* label_callsign = nullptr;
        lv_obj_t* label_type_route = nullptr;
        lv_obj_t* label_summary  = nullptr;  // visible when collapsed
        lv_obj_t* expanded_panel = nullptr;  // hidden when collapsed
        lv_obj_t* label_alt      = nullptr;
        lv_obj_t* label_speed    = nullptr;
        lv_obj_t* label_hdg      = nullptr;
        lv_obj_t* label_dist     = nullptr;
        lv_obj_t* label_status   = nullptr;
    };

    // --- Screens ---
    lv_obj_t* screen_home  = nullptr;
    lv_obj_t* screen_radar = nullptr;

    // --- Home screen widgets ---
    WeatherWidgets homeWidgets;

    // Airspace status panel (on home screen)
    lv_obj_t* airspace_circle_    = nullptr;
    lv_obj_t* airspace_coastline_ = nullptr;
    lv_point_precise_t airspace_pts_[256] = {};      // projected pts for dim coastline
    lv_obj_t* label_airspace_status_ = nullptr;
    lv_obj_t* label_airspace_sub_    = nullptr;
    lv_obj_t* label_airspace_range_  = nullptr;

    // --- Radar screen widgets ---
    lv_obj_t* radar_circle_     = nullptr;
    lv_obj_t* radar_coastline_  = nullptr;   // lv_line, full coastline
    lv_point_precise_t radar_pts_[256]  = {};        // projected pts for coastline
    lv_obj_t* label_radar_count_ = nullptr;  // "3 AIRCRAFT NEARBY" badge
    lv_obj_t* label_radar_time_  = nullptr;
    lv_obj_t* label_radar_date_  = nullptr;

    RadarBlip   radar_blips_[Config::MAX_AIRCRAFT];
    lv_obj_t*   list_container_    = nullptr;
    lv_obj_t*   label_list_header_ = nullptr;
    AircraftListRow list_rows_[Config::MAX_AIRCRAFT];
    int         list_selected_idx_  = -1;

    // --- State ---
    ScreenState currentScreen;
    unsigned long lastScreenChange;
    unsigned long lastUserInteraction;
    String statusMessage;
    uint32_t statusClearTime;
    time_t lastUpdateTime;
    uint8_t currentBrightness;
    bool userDismissed_;

    // --- LVGL task / tick timer ---
    esp_timer_handle_t lvgl_tick_timer_ = nullptr;
    TaskHandle_t       lvgl_task_handle_ = nullptr;
    static void        lvgl_task(void* arg);

    // --- LVGL callbacks ---
    static void flush_cb(lv_display_t* disp, const lv_area_t* area, uint8_t* px_map);
    static void touchpad_read(lv_indev_t* indev, lv_indev_data_t* data);

    // --- Screen builders ---
    void build_home_screen();
    void buildAirspacePanel(lv_obj_t* parent);
    void build_radar_screen();

    // --- Section builders ---
    void buildTopBar(lv_obj_t* screen, WeatherWidgets& w);
    void buildStatusBar(lv_obj_t* screen, WeatherWidgets& w);
    void buildWeatherPanel(lv_obj_t* parent, WeatherWidgets& w);

    // --- Update functions (keep existing updateWeatherWidgets and update_clock) ---
    void updateWeatherWidgets(WeatherWidgets& w, const WeatherData& weather, int aircraftCount);
    void update_home_screen(const WeatherData& weather, int aircraftCount);
    void update_radar_screen(const Aircraft* aircraft, int aircraftCount);
    void update_clock(WeatherWidgets& w);

    // --- Event handlers ---
    void onListRowClicked(int idx);
    static void event_list_row_clicked(lv_event_t* e);
    static void event_topbar_back(lv_event_t* e);

    // --- Utilities ---
    String formatTime(time_t timestamp);
    String formatDate(time_t timestamp);
};
