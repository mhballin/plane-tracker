// src/LVGLDisplayManager.h
#pragma once

#include <Arduino.h>
#include <lvgl.h>
#define LGFX_USE_V1
#include <LovyanGFX.hpp>
#include <esp_timer.h>
#include <esp_lcd_panel_rgb.h>
#include <esp_lcd_panel_ops.h>
#include "models/Aircraft.h"
#include "models/WeatherData.h"
#include "config/Config.h"

class LGFX_Panel;

class LVGLDisplayManager {
public:
    LVGLDisplayManager();
    ~LVGLDisplayManager();

    bool initialize();    // combined: initHardware() + buildScreens()
    bool initHardware();  // phase 1: LCD, LVGL, task (call BEFORE WiFi)
    bool buildScreens();  // phase 2: home + radar screens (call AFTER WiFi)
    void update(const WeatherData& weather, const Aircraft* aircraft, int aircraftCount);
    void tick(uint32_t period_ms = 5);
    void setBrightness(uint8_t brightness);
    void setStatusMessage(const String& msg);
    void setWifiConnected(bool connected);
    // Hold the LVGL mutex to freeze rendering during network calls.
    // The display stays on its last frame; dirty marks are preserved.
    void freezeRendering();
    void unfreezeRendering();

    enum ScreenState {
        SCREEN_HOME,   // weather + quiet airspace panel
        SCREEN_RADAR,  // full radar + aircraft list
    };

    void setScreen(ScreenState screen);
    ScreenState getCurrentScreen() const { return currentScreen; }

    lgfx::LGFX_Device* getLCD();

    // Returns true (and clears the flag) if the user tapped BACK since last call
    bool wasUserDismissed();
    // Returns true (and clears the flag) if the user tapped the airspace panel (→ radar)
    bool wasUserRequestedRadar();

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
        lv_obj_t*          dot        = nullptr;
        lv_obj_t*          vector     = nullptr;  // lv_line, heading direction
        lv_obj_t*          label      = nullptr;  // callsign text
        lv_point_precise_t vec_pts[2] = {};        // kept alive for lv_line
        lv_color_t         lastColor  = {};        // sentinel: {.full=0} won't match cyan or amber
        int32_t            targetDotX = 0;         // last animation target (dot top-left corner)
        int32_t            targetDotY = 0;
        bool               placed     = false;     // false until first position set; resets on hide
    };

    // One row in the aircraft list panel
    struct AircraftListRow {
        lv_obj_t* container        = nullptr;
        lv_obj_t* accent_bar       = nullptr;
        lv_obj_t* label_callsign   = nullptr;  // airline name (primary, largest)
        lv_obj_t* label_type_route = nullptr;  // "CITY → CITY · CALLSIGN"
        lv_obj_t* label_type       = nullptr;  // aircraft type
        lv_obj_t* label_summary    = nullptr;  // alt / speed / bearing / distance
        lv_color_t lastColor       = {};       // sentinel: {.full=0} won't match cyan or amber
    };

    // --- Screens ---
    lv_obj_t* screen_home  = nullptr;
    lv_obj_t* screen_radar = nullptr;

    // --- Home screen widgets ---
    WeatherWidgets homeWidgets;

    // Airspace status panel (on home screen)
    lv_obj_t* airspace_circle_    = nullptr;
    lv_obj_t* airspace_coastline_ = nullptr;
    lv_point_precise_t airspace_pts_[33] = {};
    lv_obj_t* label_airspace_status_ = nullptr;
    lv_obj_t* label_airspace_sub_    = nullptr;
    lv_obj_t* label_airspace_range_  = nullptr;

    // --- Radar screen widgets ---
    lv_obj_t* radar_circle_          = nullptr;
    lv_obj_t* radar_coastline_       = nullptr;
    lv_point_precise_t radar_pts_[33] = {};
    lv_obj_t* label_radar_count_      = nullptr;
    lv_obj_t* label_radar_time_       = nullptr;
    lv_obj_t* label_radar_date_       = nullptr;
    lv_obj_t* label_radar_status_left_ = nullptr;
    lv_obj_t* label_radar_status_live_ = nullptr;

    RadarBlip       radar_blips_[Config::MAX_AIRCRAFT];
    lv_obj_t*       list_container_    = nullptr;
    lv_obj_t*       label_list_header_ = nullptr;
    AircraftListRow list_rows_[Config::MAX_AIRCRAFT];

    // --- State ---
    ScreenState   currentScreen;
    String        statusMessage;
    uint32_t      statusClearTime;
    uint8_t       currentBrightness;
    bool          userDismissed_;
    bool          userRequestedRadar_;
    bool          wifiConnected_ = false;  // set true only after WiFi connects via setWifiConnected()
    lv_color_t    home_status_left_color_ = {};
    lv_color_t    home_status_live_color_ = {};
    lv_color_t    home_airspace_color_    = {};
    lv_color_t    radar_status_left_color_ = {};
    lv_color_t    radar_status_live_color_ = {};

    // --- IDF RGB panel (double-buffered, VSYNC-synced) ---
    esp_lcd_panel_handle_t panel_handle_   = nullptr;
    SemaphoreHandle_t      sem_vsync_end_  = nullptr;
    void*                  fb1_            = nullptr;
    void*                  fb2_            = nullptr;
    // flush_pending_: set by flush_cb after draw_bitmap(); cleared by lvgl_task after vsync.
    // lv_display_flush_ready() is called from lvgl_task (after vsync) NOT from flush_cb,
    // so LVGL's buffer-swap tracking stays in phase with the hardware vsync.
    volatile bool          flush_pending_         = false;
    volatile bool          freeze_rendering_      = false;
    volatile bool          freeze_guard_active_   = false;
    uint32_t               freeze_start_ms_       = 0;
    volatile uint32_t      freeze_draw_leaked_    = 0;
    volatile uint32_t      vsync_timeouts_        = 0;
    // Frame timing diagnostics
    volatile uint32_t      last_bitmap_ms_        = 0;  // millis() of the last draw_bitmap call
    volatile bool          log_next_bitmap_       = false;  // set on unfreeze, log first frame after
    volatile uint32_t      lv_timer_last_ms_      = 0;
    volatile uint32_t      lv_timer_max_ms_       = 0;
    volatile uint32_t      lv_skip_cycles_        = 0;
    uint32_t               diag_last_ms_          = 0;
    static bool IRAM_ATTR on_vsync_event(esp_lcd_panel_handle_t panel,
                               const esp_lcd_rgb_panel_event_data_t* edata,
                               void* user_ctx);

    // --- LVGL task / tick timer ---
    esp_timer_handle_t lvgl_tick_timer_  = nullptr;
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

    // --- Update functions ---
    void updateWeatherWidgets(WeatherWidgets& w, const WeatherData& weather, int aircraftCount);
    void update_home_screen(const WeatherData& weather, int aircraftCount);
    void update_radar_screen(const Aircraft* aircraft, int aircraftCount);
    void update_clock(WeatherWidgets& w);

    // --- Event handlers ---
    static void event_topbar_back(lv_event_t* e);
    static void event_show_radar(lv_event_t* e);

    // --- Utilities ---
    String formatTime(time_t timestamp);
    String formatDate(time_t timestamp);
};
