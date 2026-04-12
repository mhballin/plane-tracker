// src/LVGLDisplayManager.cpp
// LVGL-based Display Manager Implementation
#include "LVGLDisplayManager.h"
#include "config/Config.h"
#include "hal/ElecrowDisplayProfile.h"
// Undef Arduino.h macros that clash with GeoUtils constexpr names
#ifdef DEG_TO_RAD
#undef DEG_TO_RAD
#endif
#ifdef RAD_TO_DEG
#undef RAD_TO_DEG
#endif
#include "utils/GeoUtils.h"
#include <lgfx/v1/platforms/esp32s3/Panel_RGB.hpp>
#include <lgfx/v1/platforms/esp32s3/Bus_RGB.hpp>

// Define LGFX class with Elecrow 5" RGB panel configuration
class LGFX_Panel : public lgfx::LGFX_Device {
public:
    lgfx::Bus_RGB _bus_instance;
    lgfx::Panel_RGB _panel_instance;
    lgfx::Light_PWM _light_instance;
    lgfx::Touch_GT911 _touch_instance;

    LGFX_Panel(void) {
        // Configure panel
        {
            auto cfg = _panel_instance.config();
            cfg.memory_width = hal::Elecrow5Inch::PANEL_WIDTH;
            cfg.memory_height = hal::Elecrow5Inch::PANEL_HEIGHT;
            cfg.panel_width = hal::Elecrow5Inch::PANEL_WIDTH;
            cfg.panel_height = hal::Elecrow5Inch::PANEL_HEIGHT;
            cfg.offset_x = 0; cfg.offset_y = 0;
            _panel_instance.config(cfg);
        }
        
        // Enable PSRAM
        {
            auto cfg = _panel_instance.config_detail();
            cfg.use_psram = hal::Elecrow5Inch::USE_PSRAM_FRAMEBUFFER;
            _panel_instance.config_detail(cfg);
        }
        
        // Configure RGB bus
        {
            auto cfg = _bus_instance.config();
            cfg.panel = &_panel_instance;
            cfg.pin_d0 = hal::Elecrow5Inch::PIN_D0;
            cfg.pin_d1 = hal::Elecrow5Inch::PIN_D1;
            cfg.pin_d2 = hal::Elecrow5Inch::PIN_D2;
            cfg.pin_d3 = hal::Elecrow5Inch::PIN_D3;
            cfg.pin_d4 = hal::Elecrow5Inch::PIN_D4;
            cfg.pin_d5 = hal::Elecrow5Inch::PIN_D5;
            cfg.pin_d6 = hal::Elecrow5Inch::PIN_D6;
            cfg.pin_d7 = hal::Elecrow5Inch::PIN_D7;
            cfg.pin_d8 = hal::Elecrow5Inch::PIN_D8;
            cfg.pin_d9 = hal::Elecrow5Inch::PIN_D9;
            cfg.pin_d10 = hal::Elecrow5Inch::PIN_D10;
            cfg.pin_d11 = hal::Elecrow5Inch::PIN_D11;
            cfg.pin_d12 = hal::Elecrow5Inch::PIN_D12;
            cfg.pin_d13 = hal::Elecrow5Inch::PIN_D13;
            cfg.pin_d14 = hal::Elecrow5Inch::PIN_D14;
            cfg.pin_d15 = hal::Elecrow5Inch::PIN_D15;
            cfg.pin_henable = hal::Elecrow5Inch::PIN_HENABLE;
            cfg.pin_vsync = hal::Elecrow5Inch::PIN_VSYNC;
            cfg.pin_hsync = hal::Elecrow5Inch::PIN_HSYNC;
            cfg.pin_pclk = hal::Elecrow5Inch::PIN_PCLK;
            cfg.freq_write = hal::Elecrow5Inch::RGB_FREQ_WRITE;
            cfg.hsync_polarity = hal::Elecrow5Inch::HSYNC_POLARITY;
            cfg.hsync_front_porch = hal::Elecrow5Inch::HSYNC_FRONT_PORCH;
            cfg.hsync_pulse_width = hal::Elecrow5Inch::HSYNC_PULSE_WIDTH;
            cfg.hsync_back_porch = hal::Elecrow5Inch::HSYNC_BACK_PORCH;
            cfg.vsync_polarity = hal::Elecrow5Inch::VSYNC_POLARITY;
            cfg.vsync_front_porch = hal::Elecrow5Inch::VSYNC_FRONT_PORCH;
            cfg.vsync_pulse_width = hal::Elecrow5Inch::VSYNC_PULSE_WIDTH;
            cfg.vsync_back_porch = hal::Elecrow5Inch::VSYNC_BACK_PORCH;
            cfg.pclk_active_neg = hal::Elecrow5Inch::PCLK_ACTIVE_NEG;
            cfg.de_idle_high = hal::Elecrow5Inch::DE_IDLE_HIGH;
            cfg.pclk_idle_high = hal::Elecrow5Inch::PCLK_IDLE_HIGH;
            _bus_instance.config(cfg);
        }
        _panel_instance.setBus(&_bus_instance);
        
        // Configure backlight
        {
            auto cfg = _light_instance.config();
            cfg.pin_bl = hal::Elecrow5Inch::PIN_BACKLIGHT;
            _light_instance.config(cfg);
        }
        _panel_instance.light(&_light_instance);
        
        // Configure touch (GT911)
        {
            auto cfg = _touch_instance.config();
            cfg.x_min = 0;
            cfg.x_max = hal::Elecrow5Inch::PANEL_WIDTH;
            cfg.y_min = 0;
            cfg.y_max = hal::Elecrow5Inch::PANEL_HEIGHT;
            cfg.i2c_addr = hal::Elecrow5Inch::TOUCH_I2C_ADDR;
            cfg.pin_sda = hal::Elecrow5Inch::TOUCH_PIN_SDA;
            cfg.pin_scl = hal::Elecrow5Inch::TOUCH_PIN_SCL;
            cfg.pin_int = hal::Elecrow5Inch::TOUCH_PIN_INT;
            cfg.pin_rst = hal::Elecrow5Inch::TOUCH_PIN_RST;
            cfg.i2c_port = hal::Elecrow5Inch::TOUCH_I2C_PORT;
            cfg.freq = hal::Elecrow5Inch::TOUCH_I2C_FREQ;
            cfg.bus_shared = false;
            _touch_instance.config(cfg);
            _panel_instance.setTouch(&_touch_instance);
        }
        setPanel(&_panel_instance);
    }
};

// Define buffer sizes for LVGL
#define LVGL_BUFFER_SIZE (hal::Elecrow5Inch::PANEL_WIDTH * 40)  // 40 lines buffer

// Static instance for callbacks
static LVGLDisplayManager* s_instance = nullptr;

// Color definitions (LVGL uses 0xRRGGBB format for true colors)
// Aviation dashboard dark theme
#define COLOR_BG             lv_color_hex(0x0e1726)
#define COLOR_TOPBAR         lv_color_hex(0x101f33)
#define COLOR_PANEL          lv_color_hex(0x162033)
#define COLOR_INSET          lv_color_hex(0x0a1428)
#define COLOR_STATUSBAR      lv_color_hex(0x060e1a)
#define COLOR_ACCENT         lv_color_hex(0x00d4ff)
#define COLOR_AMBER          lv_color_hex(0xf59e0b)
#define COLOR_SUCCESS        lv_color_hex(0x22c55e)
#define COLOR_TEXT_PRIMARY   lv_color_hex(0xeaf6ff)
#define COLOR_TEXT_SECONDARY lv_color_hex(0x5a8aaa)
#define COLOR_TEXT_DIM       lv_color_hex(0x2a5070)
#define COLOR_BORDER         lv_color_hex(0x1e3a54)
#define COLOR_BORDER_ACCENT  lv_color_hex(0x004466)
#define COLOR_TEXT_ON_ACCENT lv_color_hex(0x060e1a)
#define COLOR_DESCENT        lv_color_hex(0xef4444)

// Constructor
LVGLDisplayManager::LVGLDisplayManager()
    : lcd(nullptr)
    , lv_display(nullptr)
    , lv_indev(nullptr)
    , screen_home(nullptr)
    , screen_home_empty(nullptr)
    , screen_aircraft(nullptr)
    , radar_container(nullptr)
    , label_contact_count(nullptr)
    , btn_view_planes(nullptr)
    , label_callsign(nullptr)
    , label_distance(nullptr)
    , label_airline(nullptr)
    , label_aircraft_type(nullptr)
    , label_squawk(nullptr)
    , label_route_main(nullptr)
    , label_route_sub(nullptr)
    , label_altitude(nullptr)
    , label_velocity(nullptr)
    , label_heading(nullptr)
    , label_vert_speed(nullptr)
    , label_status_aircraft(nullptr)
    , btn_back_home(nullptr)
    , currentScreen(SCREEN_HOME)
    , homeHasAircraft(false)
    , lastScreenChange(0)
    , lastUserInteraction(0)
    , statusMessage("")
    , statusClearTime(0)
    , lastUpdateTime(0)
    , currentBrightness(255)
{
    for (int i = 0; i < Config::MAX_AIRCRAFT; i++) {
        radar_blips[i] = nullptr;
    }
    s_instance = this;
}

// Destructor
LVGLDisplayManager::~LVGLDisplayManager() {
    if (lcd) {
        delete lcd;
        lcd = nullptr;
    }
    s_instance = nullptr;
}

// Initialize LVGL and display
bool LVGLDisplayManager::initialize() {
    Serial.println("[LVGL] Initializing LVGL display manager...");
    
    // Create LovyanGFX display instance
    lcd = new LGFX_Panel();
    if (!lcd) {
        Serial.println("[LVGL] Failed to allocate LGFX");
        return false;
    }
    
    lcd->init();
    lcd->setRotation(hal::Elecrow5Inch::PANEL_ROTATION);
    lcd->setBrightness(currentBrightness);
    lcd->fillScreen(TFT_BLACK);
    
    // Initialize LVGL
    lv_init();

    // 1-ms hardware tick — accurate regardless of main-loop blocking
    {
        esp_timer_create_args_t args = {};
        args.callback = [](void*) { lv_tick_inc(1); };
        args.name = "lvgl_tick";
        args.dispatch_method = ESP_TIMER_TASK;
        esp_err_t err = esp_timer_create(&args, &lvgl_tick_timer_);
        if (err != ESP_OK) {
            Serial.printf("[LVGL] Failed to create tick timer: %s\n", esp_err_to_name(err));
            return false;
        }
        err = esp_timer_start_periodic(lvgl_tick_timer_, 1000 /* µs = 1 ms */);
        if (err != ESP_OK) {
            Serial.printf("[LVGL] Failed to start tick timer: %s\n", esp_err_to_name(err));
            return false;
        }
    }

    // Create display buffers
    static lv_color_t* buf1 = (lv_color_t*)heap_caps_malloc(LVGL_BUFFER_SIZE * sizeof(lv_color_t), MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
    static lv_color_t* buf2 = (lv_color_t*)heap_caps_malloc(LVGL_BUFFER_SIZE * sizeof(lv_color_t), MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
    
    if (!buf1 || !buf2) {
        Serial.println("[LVGL] Failed to allocate display buffers");
        return false;
    }
    
    // Create LVGL display
    lv_display = lv_display_create(hal::Elecrow5Inch::PANEL_WIDTH, hal::Elecrow5Inch::PANEL_HEIGHT);
    lv_display_set_flush_cb(lv_display, flush_cb);
    lv_display_set_buffers(lv_display, buf1, buf2, LVGL_BUFFER_SIZE * sizeof(lv_color_t), LV_DISPLAY_RENDER_MODE_PARTIAL);
    
    // Create input device (touch)
    lv_indev = lv_indev_create();
    lv_indev_set_type(lv_indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(lv_indev, touchpad_read);
    
    // Build screens
    build_home_screen();
    build_home_empty_screen();
    build_aircraft_screen();

    // Load appropriate home screen (no aircraft at boot)
    lv_screen_load(screen_home_empty);
    currentScreen = SCREEN_HOME;
    homeHasAircraft = false;
    lastUserInteraction = millis();

    // Start LVGL handler task now that display, indev, and screens are all registered
    BaseType_t rc = xTaskCreatePinnedToCore(
        lvgl_task,
        "lvgl",
        8192,
        nullptr,
        2,
        &lvgl_task_handle_,
        1
    );
    if (rc != pdPASS) {
        Serial.println("[LVGL] Failed to create LVGL task");
        return false;
    }
    Serial.println("[LVGL] FreeRTOS task + tick timer started");

    Serial.println("[LVGL] Display initialized successfully");
    return true;
}

// Flush callback for LovyanGFX
void LVGLDisplayManager::flush_cb(lv_display_t* disp, const lv_area_t* area, uint8_t* px_map) {
    if (!s_instance || !s_instance->lcd) return;
    
    uint32_t w = area->x2 - area->x1 + 1;
    uint32_t h = area->y2 - area->y1 + 1;
    
    s_instance->lcd->startWrite();
    s_instance->lcd->setAddrWindow(area->x1, area->y1, w, h);
    s_instance->lcd->writePixels((lgfx::rgb565_t*)px_map, w * h);
    s_instance->lcd->endWrite();
    
    lv_display_flush_ready(disp);
}

// Touch callback
void LVGLDisplayManager::touchpad_read(lv_indev_t* indev, lv_indev_data_t* data) {
    if (!s_instance || !s_instance->lcd) return;
    
    int32_t x = 0, y = 0;
    bool touched = s_instance->lcd->getTouch(&x, &y);
    
    if (touched) {
        data->state = LV_INDEV_STATE_PRESSED;
        data->point.x = x;
        data->point.y = y;
        s_instance->lastUserInteraction = millis();
    } else {
        data->state = LV_INDEV_STATE_RELEASED;
    }
}

// LVGL handler task — runs independently of main loop
void LVGLDisplayManager::lvgl_task(void* /*arg*/) {
    while (true) {
        lv_lock();
        lv_timer_handler();
        lv_unlock();
        vTaskDelay(pdMS_TO_TICKS(5));
    }
}

// Build home screen with modern card-based layout
void LVGLDisplayManager::buildTopBar(lv_obj_t* screen, WeatherWidgets& w) {
    lv_obj_t* bar = lv_obj_create(screen);
    lv_obj_set_size(bar, hal::Elecrow5Inch::PANEL_WIDTH, 58);
    lv_obj_set_pos(bar, 0, 0);
    lv_obj_set_style_bg_color(bar, COLOR_TOPBAR, 0);
    lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, 0);
    lv_obj_set_style_border_side(bar, LV_BORDER_SIDE_BOTTOM, 0);
    lv_obj_set_style_border_color(bar, COLOR_BORDER, 0);
    lv_obj_set_style_border_width(bar, 1, 0);
    lv_obj_set_style_radius(bar, 0, 0);
    lv_obj_set_style_pad_all(bar, 0, 0);
    lv_obj_clear_flag(bar, LV_OBJ_FLAG_SCROLLABLE);

    // Time — cyan, 28px bold, left side
    w.label_time = lv_label_create(bar);
    lv_obj_set_style_text_font(w.label_time, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(w.label_time, COLOR_ACCENT, 0);
    lv_label_set_text(w.label_time, "00:00");
    lv_obj_align(w.label_time, LV_ALIGN_LEFT_MID, 20, -8);

    // Date — secondary, 12px, below time
    w.label_date = lv_label_create(bar);
    lv_obj_set_style_text_font(w.label_date, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(w.label_date, COLOR_TEXT_SECONDARY, 0);
    lv_label_set_text(w.label_date, "Mon, Jan 1");
    lv_obj_align(w.label_date, LV_ALIGN_LEFT_MID, 20, 14);

    // Location — dim, right side
    lv_obj_t* lbl_loc = lv_label_create(bar);
    lv_obj_set_style_text_font(lbl_loc, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(lbl_loc, COLOR_TEXT_DIM, 0);
    lv_label_set_text(lbl_loc, Config::WEATHER_CITY);
    lv_obj_align(lbl_loc, LV_ALIGN_RIGHT_MID, -20, 0);
}

void LVGLDisplayManager::buildStatusBar(lv_obj_t* screen, WeatherWidgets& w) {
    lv_obj_t* bar = lv_obj_create(screen);
    lv_obj_set_size(bar, hal::Elecrow5Inch::PANEL_WIDTH, 26);
    lv_obj_align(bar, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_color(bar, COLOR_STATUSBAR, 0);
    lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, 0);
    lv_obj_set_style_border_side(bar, LV_BORDER_SIDE_TOP, 0);
    lv_obj_set_style_border_color(bar, COLOR_BORDER, 0);
    lv_obj_set_style_border_width(bar, 1, 0);
    lv_obj_set_style_radius(bar, 0, 0);
    lv_obj_set_style_pad_hor(bar, 12, 0);
    lv_obj_set_style_pad_ver(bar, 0, 0);
    lv_obj_clear_flag(bar, LV_OBJ_FLAG_SCROLLABLE);

    w.label_status_left = lv_label_create(bar);
    lv_obj_set_style_text_font(w.label_status_left, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(w.label_status_left, COLOR_TEXT_DIM, 0);
    lv_label_set_text(w.label_status_left, "INITIALIZING");
    lv_obj_align(w.label_status_left, LV_ALIGN_LEFT_MID, 0, 0);

    w.label_status_live = lv_label_create(bar);
    lv_obj_set_style_text_font(w.label_status_live, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(w.label_status_live, COLOR_TEXT_DIM, 0);
    lv_label_set_text(w.label_status_live, "● IDLE");
    lv_obj_align(w.label_status_live, LV_ALIGN_RIGHT_MID, 0, 0);
}

void LVGLDisplayManager::buildWeatherPanel(lv_obj_t* parent, WeatherWidgets& w) {
    lv_obj_set_style_bg_opa(parent, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(parent, 0, 0);
    lv_obj_set_style_pad_all(parent, 0, 0);
    lv_obj_clear_flag(parent, LV_OBJ_FLAG_SCROLLABLE);

    // Internal padding container
    lv_obj_t* pad = lv_obj_create(parent);
    lv_obj_set_size(pad, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_opa(pad, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(pad, 0, 0);
    lv_obj_set_style_pad_hor(pad, 16, 0);
    lv_obj_set_style_pad_ver(pad, 12, 0);
    lv_obj_clear_flag(pad, LV_OBJ_FLAG_SCROLLABLE);

    // Temperature (large, top-left)
    w.label_temperature = lv_label_create(pad);
    lv_obj_set_style_text_font(w.label_temperature, &lv_font_montserrat_48, 0);
    lv_obj_set_style_text_color(w.label_temperature, COLOR_TEXT_PRIMARY, 0);
    lv_label_set_text(w.label_temperature, "--\xc2\xb0");
    lv_obj_set_pos(w.label_temperature, 0, 0);

    // Condition (to right of temperature)
    w.label_weather_desc = lv_label_create(pad);
    lv_obj_set_style_text_font(w.label_weather_desc, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(w.label_weather_desc, COLOR_TEXT_SECONDARY, 0);
    lv_label_set_text(w.label_weather_desc, "Loading...");
    lv_obj_set_pos(w.label_weather_desc, 110, 4);

    // Feels-like
    w.label_feels_like = lv_label_create(pad);
    lv_obj_set_style_text_font(w.label_feels_like, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(w.label_feels_like, COLOR_TEXT_DIM, 0);
    lv_label_set_text(w.label_feels_like, "Feels: --\xc2\xb0");
    lv_obj_set_pos(w.label_feels_like, 110, 24);

    // Hi/Lo
    w.label_temp_range = lv_label_create(pad);
    lv_obj_set_style_text_font(w.label_temp_range, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(w.label_temp_range, COLOR_TEXT_DIM, 0);
    lv_label_set_text(w.label_temp_range, "H: --\xc2\xb0  L: --\xc2\xb0");
    lv_obj_set_pos(w.label_temp_range, 110, 42);

    // Details strip (border top + bottom)
    lv_obj_t* strip = lv_obj_create(pad);
    lv_obj_set_size(strip, LV_PCT(100), 36);
    lv_obj_set_pos(strip, 0, 68);
    lv_obj_set_style_bg_opa(strip, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_side(strip, (lv_border_side_t)(LV_BORDER_SIDE_TOP | LV_BORDER_SIDE_BOTTOM), 0);
    lv_obj_set_style_border_color(strip, COLOR_BORDER, 0);
    lv_obj_set_style_border_width(strip, 1, 0);
    lv_obj_set_style_radius(strip, 0, 0);
    lv_obj_set_style_pad_all(strip, 0, 0);
    lv_obj_clear_flag(strip, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* lbl_wind_hdr = lv_label_create(strip);
    lv_obj_set_style_text_font(lbl_wind_hdr, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(lbl_wind_hdr, COLOR_TEXT_DIM, 0);
    lv_label_set_text(lbl_wind_hdr, "WIND");
    lv_obj_set_pos(lbl_wind_hdr, 0, 4);

    w.label_wind = lv_label_create(strip);
    lv_obj_set_style_text_font(w.label_wind, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(w.label_wind, COLOR_TEXT_PRIMARY, 0);
    lv_label_set_text(w.label_wind, "-- mph");
    lv_obj_set_pos(w.label_wind, 0, 18);

    lv_obj_t* lbl_hum_hdr = lv_label_create(strip);
    lv_obj_set_style_text_font(lbl_hum_hdr, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(lbl_hum_hdr, COLOR_TEXT_DIM, 0);
    lv_label_set_text(lbl_hum_hdr, "HUMIDITY");
    lv_obj_set_pos(lbl_hum_hdr, 90, 4);

    w.label_humidity_val = lv_label_create(strip);
    lv_obj_set_style_text_font(w.label_humidity_val, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(w.label_humidity_val, COLOR_TEXT_PRIMARY, 0);
    lv_label_set_text(w.label_humidity_val, "--%");
    lv_obj_set_pos(w.label_humidity_val, 90, 18);

    w.label_sunrise = lv_label_create(strip);
    lv_obj_set_style_text_font(w.label_sunrise, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(w.label_sunrise, COLOR_AMBER, 0);
    lv_label_set_text(w.label_sunrise, LV_SYMBOL_UP " --:--");
    lv_obj_align(w.label_sunrise, LV_ALIGN_RIGHT_MID, -70, 0);

    w.label_sunset = lv_label_create(strip);
    lv_obj_set_style_text_font(w.label_sunset, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(w.label_sunset, COLOR_AMBER, 0);
    lv_label_set_text(w.label_sunset, LV_SYMBOL_DOWN " --:--");
    lv_obj_align(w.label_sunset, LV_ALIGN_RIGHT_MID, 0, 0);

    // 5-day forecast header
    lv_obj_t* fc_header = lv_label_create(pad);
    lv_obj_set_style_text_font(fc_header, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(fc_header, COLOR_TEXT_DIM, 0);
    lv_label_set_text(fc_header, "5-DAY FORECAST");
    lv_obj_set_pos(fc_header, 0, 112);

    // 5 forecast rows
    for (int i = 0; i < 5; i++) {
        int y = 130 + i * 46;
        WeatherWidgets::ForecastRow& row = w.forecast[i];

        row.container = lv_obj_create(pad);
        lv_obj_set_size(row.container, LV_PCT(100), 40);
        lv_obj_set_pos(row.container, 0, y);
        lv_obj_set_style_bg_color(row.container, COLOR_INSET, 0);
        lv_obj_set_style_bg_opa(row.container, LV_OPA_COVER, 0);
        lv_obj_set_style_border_color(row.container, COLOR_BORDER, 0);
        lv_obj_set_style_border_width(row.container, 1, 0);
        lv_obj_set_style_radius(row.container, 4, 0);
        lv_obj_set_style_pad_hor(row.container, 10, 0);
        lv_obj_set_style_pad_ver(row.container, 0, 0);
        lv_obj_clear_flag(row.container, LV_OBJ_FLAG_SCROLLABLE);

        row.label_day = lv_label_create(row.container);
        lv_obj_set_style_text_font(row.label_day, &lv_font_montserrat_12, 0);
        lv_obj_set_style_text_color(row.label_day, COLOR_TEXT_SECONDARY, 0);
        lv_label_set_text(row.label_day, "---");
        lv_obj_align(row.label_day, LV_ALIGN_LEFT_MID, 0, 0);

        row.label_cond = lv_label_create(row.container);
        lv_obj_set_style_text_font(row.label_cond, &lv_font_montserrat_12, 0);
        lv_obj_set_style_text_color(row.label_cond, COLOR_TEXT_DIM, 0);
        lv_label_set_text(row.label_cond, "");
        lv_obj_align(row.label_cond, LV_ALIGN_CENTER, 0, 0);

        row.label_hi = lv_label_create(row.container);
        lv_obj_set_style_text_font(row.label_hi, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(row.label_hi, COLOR_TEXT_PRIMARY, 0);
        lv_label_set_text(row.label_hi, "--\xc2\xb0");
        lv_obj_align(row.label_hi, LV_ALIGN_RIGHT_MID, -30, 0);

        row.label_lo = lv_label_create(row.container);
        lv_obj_set_style_text_font(row.label_lo, &lv_font_montserrat_12, 0);
        lv_obj_set_style_text_color(row.label_lo, COLOR_TEXT_DIM, 0);
        lv_label_set_text(row.label_lo, "--\xc2\xb0");
        lv_obj_align(row.label_lo, LV_ALIGN_RIGHT_MID, 0, 0);
    }
}

void LVGLDisplayManager::updateWeatherWidgets(WeatherWidgets& w,
                                               const WeatherData& weather,
                                               int aircraftCount) {
    // All widgets are built together; if label_temperature exists, the others do too.
    if (!w.label_temperature) return;

    char buf[64];

    snprintf(buf, sizeof(buf), "%.0f\xc2\xb0""F", weather.temperature);
    lv_label_set_text(w.label_temperature, buf);

    lv_label_set_text(w.label_weather_desc, weather.description.c_str());

    snprintf(buf, sizeof(buf), "Feels: %.0f\xc2\xb0""F", weather.feelsLike);
    lv_label_set_text(w.label_feels_like, buf);

    snprintf(buf, sizeof(buf), "H: %.0f\xc2\xb0  L: %.0f\xc2\xb0", weather.tempMax, weather.tempMin);
    lv_label_set_text(w.label_temp_range, buf);

    snprintf(buf, sizeof(buf), "%.0f mph", weather.windSpeed);
    lv_label_set_text(w.label_wind, buf);

    snprintf(buf, sizeof(buf), "%.0f%%", weather.humidity);
    lv_label_set_text(w.label_humidity_val, buf);

    snprintf(buf, sizeof(buf), LV_SYMBOL_UP " %s", formatTime(weather.sunrise).c_str());
    lv_label_set_text(w.label_sunrise, buf);

    snprintf(buf, sizeof(buf), LV_SYMBOL_DOWN " %s", formatTime(weather.sunset).c_str());
    lv_label_set_text(w.label_sunset, buf);

    for (int i = 0; i < 5; i++) {
        if (!w.forecast[i].container) continue;
        if (i < (int)weather.forecast.size()) {
            const auto& day = weather.forecast[i];
            lv_label_set_text(w.forecast[i].label_day,  day.dayName.c_str());
            lv_label_set_text(w.forecast[i].label_cond, day.condition.c_str());
            snprintf(buf, sizeof(buf), "%.0f\xc2\xb0", day.tempMax);
            lv_label_set_text(w.forecast[i].label_hi, buf);
            snprintf(buf, sizeof(buf), "%.0f\xc2\xb0", day.tempMin);
            lv_label_set_text(w.forecast[i].label_lo, buf);
        } else {
            lv_label_set_text(w.forecast[i].label_day,  "-");
            lv_label_set_text(w.forecast[i].label_cond, "");
            lv_label_set_text(w.forecast[i].label_hi,   "--\xc2\xb0");
            lv_label_set_text(w.forecast[i].label_lo,   "--\xc2\xb0");
        }
    }

    // Status bar
    if (w.label_status_left) {
        if (aircraftCount > 0) {
            char ts[16];
            struct tm ti;
            getLocalTime(&ti);
            strftime(ts, sizeof(ts), "%H:%M", &ti);
            snprintf(buf, sizeof(buf), "OPENSKY OK \xc2\xb7 %s", ts);
            lv_label_set_text(w.label_status_left, buf);
        } else {
            lv_label_set_text(w.label_status_left, "NO AIRCRAFT DETECTED");
        }
    }
    if (w.label_status_live) {
        if (aircraftCount > 0) {
            lv_obj_set_style_text_color(w.label_status_live, COLOR_SUCCESS, 0);
            lv_label_set_text(w.label_status_live, "\xe2\x97\x8f LIVE");
        } else {
            lv_obj_set_style_text_color(w.label_status_live, COLOR_TEXT_DIM, 0);
            lv_label_set_text(w.label_status_live, "\xe2\x97\x8f IDLE");
        }
    }
}

void LVGLDisplayManager::build_home_screen() {
    screen_home = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(screen_home, COLOR_BG, 0);
    lv_obj_set_style_bg_opa(screen_home, LV_OPA_COVER, 0);

    buildTopBar(screen_home, homeWidgets);
    buildStatusBar(screen_home, homeWidgets);

    // Body: 800 x 396px, between top bar (58) and status bar (26)
    lv_obj_t* body = lv_obj_create(screen_home);
    lv_obj_set_size(body, hal::Elecrow5Inch::PANEL_WIDTH, 396);
    lv_obj_set_pos(body, 0, 58);
    lv_obj_set_style_bg_opa(body, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(body, 0, 0);
    lv_obj_set_style_pad_all(body, 0, 0);
    lv_obj_set_style_radius(body, 0, 0);
    lv_obj_clear_flag(body, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_layout(body, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(body, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(body, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);

    // Weather panel (left, fills remaining space)
    lv_obj_t* weather_col = lv_obj_create(body);
    lv_obj_set_flex_grow(weather_col, 1);
    lv_obj_set_height(weather_col, 396);
    buildWeatherPanel(weather_col, homeWidgets);

    // Radar panel (right, fixed 310px)
    lv_obj_t* radar_col = lv_obj_create(body);
    lv_obj_set_size(radar_col, 310, 396);
    buildRadarPanel(radar_col);
}

void LVGLDisplayManager::buildRadarPanel(lv_obj_t* parent) {
    lv_obj_set_style_bg_color(parent, COLOR_PANEL, 0);
    lv_obj_set_style_bg_opa(parent, LV_OPA_COVER, 0);
    lv_obj_set_style_border_side(parent, LV_BORDER_SIDE_LEFT, 0);
    lv_obj_set_style_border_color(parent, COLOR_BORDER, 0);
    lv_obj_set_style_border_width(parent, 1, 0);
    lv_obj_set_style_radius(parent, 0, 0);
    lv_obj_set_style_pad_all(parent, 10, 0);
    lv_obj_clear_flag(parent, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_set_layout(parent, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(parent, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(parent, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(parent, 8, 0);
    lv_obj_set_style_pad_column(parent, 8, 0);

    // Section header
    lv_obj_t* hdr = lv_label_create(parent);
    lv_obj_set_style_text_font(hdr, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(hdr, COLOR_TEXT_DIM, 0);
    lv_label_set_text(hdr, "AREA CONTACTS");

    // Radar circle (190x190)
    radar_container = lv_obj_create(parent);
    lv_obj_set_size(radar_container, 190, 190);
    lv_obj_set_style_radius(radar_container, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(radar_container, COLOR_INSET, 0);
    lv_obj_set_style_bg_opa(radar_container, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(radar_container, COLOR_BORDER_ACCENT, 0);
    lv_obj_set_style_border_width(radar_container, 1, 0);
    lv_obj_set_style_pad_all(radar_container, 0, 0);
    lv_obj_clear_flag(radar_container, (lv_obj_flag_t)(LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE));

    // Range rings at 33% and 66% of radius
    const int ring_sizes[2] = { 64, 126 };
    for (int i = 0; i < 2; i++) {
        lv_obj_t* ring = lv_obj_create(radar_container);
        lv_obj_set_size(ring, ring_sizes[i], ring_sizes[i]);
        lv_obj_center(ring);
        lv_obj_set_style_radius(ring, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_opa(ring, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_color(ring, lv_color_hex(0x1a3048), 0);
        lv_obj_set_style_border_width(ring, 1, 0);
        lv_obj_clear_flag(ring, (lv_obj_flag_t)(LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE));
    }

    // Home dot (center, 8px cyan)
    lv_obj_t* home_dot = lv_obj_create(radar_container);
    lv_obj_set_size(home_dot, 8, 8);
    lv_obj_center(home_dot);
    lv_obj_set_style_radius(home_dot, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(home_dot, COLOR_ACCENT, 0);
    lv_obj_set_style_border_width(home_dot, 0, 0);
    lv_obj_clear_flag(home_dot, LV_OBJ_FLAG_CLICKABLE);

    // Pre-create aircraft blips (all hidden)
    for (int i = 0; i < Config::MAX_AIRCRAFT; i++) {
        radar_blips[i] = lv_obj_create(radar_container);
        lv_obj_set_size(radar_blips[i], 8, 8);
        lv_obj_set_style_radius(radar_blips[i], LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_color(radar_blips[i], COLOR_ACCENT, 0);
        lv_obj_set_style_border_width(radar_blips[i], 0, 0);
        lv_obj_set_pos(radar_blips[i], 91, 91);
        lv_obj_add_flag(radar_blips[i], LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(radar_blips[i], LV_OBJ_FLAG_CLICKABLE);
    }

    // Contact count
    label_contact_count = lv_label_create(parent);
    lv_obj_set_style_text_font(label_contact_count, &lv_font_montserrat_26, 0);
    lv_obj_set_style_text_color(label_contact_count, COLOR_ACCENT, 0);
    lv_label_set_text(label_contact_count, "0");

    lv_obj_t* lbl_units = lv_label_create(parent);
    lv_obj_set_style_text_font(lbl_units, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(lbl_units, COLOR_TEXT_DIM, 0);
    lv_label_set_text(lbl_units, "CONTACTS");

    // VIEW AIRCRAFT button
    btn_view_planes = lv_button_create(parent);
    lv_obj_set_size(btn_view_planes, 220, 36);
    lv_obj_set_style_bg_color(btn_view_planes, COLOR_ACCENT, 0);
    lv_obj_set_style_radius(btn_view_planes, 4, 0);

    lv_obj_t* btn_lbl = lv_label_create(btn_view_planes);
    lv_obj_set_style_text_font(btn_lbl, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(btn_lbl, COLOR_TEXT_ON_ACCENT, 0);
    lv_label_set_text(btn_lbl, LV_SYMBOL_RIGHT "  VIEW AIRCRAFT");
    lv_obj_center(btn_lbl);

    // Tapping anywhere on panel navigates to aircraft list
    lv_obj_add_event_cb(parent, event_btn_view_planes, LV_EVENT_CLICKED, this);
}

void LVGLDisplayManager::update_home_screen(const WeatherData& weather,
                                             const Aircraft* aircraft,
                                             int aircraftCount) {
    // Switch home screen variant if aircraft count changed
    bool nowHas = (aircraftCount > 0);
    if (nowHas != homeHasAircraft) {
        homeHasAircraft = nowHas;
        lv_screen_load(homeHasAircraft ? screen_home : screen_home_empty);
    }

    // Update weather labels on whichever screen is active
    WeatherWidgets& w = homeHasAircraft ? homeWidgets : emptyWidgets;
    update_clock(w);
    updateWeatherWidgets(w, weather, aircraftCount);

    if (!homeHasAircraft) return;

    // Update radar panel
    char count_buf[8];
    snprintf(count_buf, sizeof(count_buf), "%d", aircraftCount);
    lv_label_set_text(label_contact_count, count_buf);

    for (int i = 0; i < Config::MAX_AIRCRAFT; i++) {
        if (!radar_blips[i]) continue;

        if (aircraft && i < aircraftCount && aircraft[i].valid) {
            const Aircraft& a = aircraft[i];
            float distNm = GeoUtils::distanceNm(
                Config::HOME_LAT, Config::HOME_LON,
                a.latitude, a.longitude);
            float bearing = GeoUtils::bearingDeg(
                Config::HOME_LAT, Config::HOME_LON,
                a.latitude, a.longitude);

            auto pos = GeoUtils::blipPosition(distNm, bearing,
                                               Config::RADAR_MAX_RANGE_NM, 95, 6);
            lv_obj_set_pos(radar_blips[i], pos.x - 4, pos.y - 4);

            // Amber for low altitude (<5000 ft), cyan otherwise
            float altFt = a.altitude * 3.28084f;
            lv_color_t blipColor = (altFt > 0 && altFt < 5000.0f) ? COLOR_AMBER : COLOR_ACCENT;
            lv_obj_set_style_bg_color(radar_blips[i], blipColor, 0);
            lv_obj_remove_flag(radar_blips[i], LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(radar_blips[i], LV_OBJ_FLAG_HIDDEN);
        }
    }
}

void LVGLDisplayManager::build_home_empty_screen() {
    screen_home_empty = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(screen_home_empty, COLOR_BG, 0);
    lv_obj_set_style_bg_opa(screen_home_empty, LV_OPA_COVER, 0);

    buildTopBar(screen_home_empty, emptyWidgets);
    buildStatusBar(screen_home_empty, emptyWidgets);

    // Body: full 800px width, two-column layout
    lv_obj_t* body = lv_obj_create(screen_home_empty);
    lv_obj_set_size(body, hal::Elecrow5Inch::PANEL_WIDTH, 396);
    lv_obj_set_pos(body, 0, 58);
    lv_obj_set_style_bg_opa(body, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(body, 0, 0);
    lv_obj_set_style_pad_all(body, 0, 0);
    lv_obj_set_style_radius(body, 0, 0);
    lv_obj_clear_flag(body, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_layout(body, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(body, LV_FLEX_FLOW_ROW);

    // Left column: current conditions (340px)
    lv_obj_t* left_col = lv_obj_create(body);
    lv_obj_set_size(left_col, 340, 396);
    lv_obj_set_style_bg_opa(left_col, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(left_col, 0, 0);
    lv_obj_set_style_pad_hor(left_col, 20, 0);
    lv_obj_set_style_pad_ver(left_col, 20, 0);
    lv_obj_clear_flag(left_col, LV_OBJ_FLAG_SCROLLABLE);

    // Temperature (48px bold)
    emptyWidgets.label_temperature = lv_label_create(left_col);
    lv_obj_set_style_text_font(emptyWidgets.label_temperature, &lv_font_montserrat_48, 0);
    lv_obj_set_style_text_color(emptyWidgets.label_temperature, COLOR_TEXT_PRIMARY, 0);
    lv_label_set_text(emptyWidgets.label_temperature, "--\xc2\xb0""F");
    lv_obj_set_pos(emptyWidgets.label_temperature, 0, 0);

    // Condition (16px secondary)
    emptyWidgets.label_weather_desc = lv_label_create(left_col);
    lv_obj_set_style_text_font(emptyWidgets.label_weather_desc, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(emptyWidgets.label_weather_desc, COLOR_TEXT_SECONDARY, 0);
    lv_label_set_text(emptyWidgets.label_weather_desc, "Loading...");
    lv_obj_set_pos(emptyWidgets.label_weather_desc, 0, 62);

    // Feels-like (12px dim)
    emptyWidgets.label_feels_like = lv_label_create(left_col);
    lv_obj_set_style_text_font(emptyWidgets.label_feels_like, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(emptyWidgets.label_feels_like, COLOR_TEXT_DIM, 0);
    lv_label_set_text(emptyWidgets.label_feels_like, "Feels: --\xc2\xb0""F");
    lv_obj_set_pos(emptyWidgets.label_feels_like, 0, 84);

    // Hi/Lo (12px dim)
    emptyWidgets.label_temp_range = lv_label_create(left_col);
    lv_obj_set_style_text_font(emptyWidgets.label_temp_range, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(emptyWidgets.label_temp_range, COLOR_TEXT_DIM, 0);
    lv_label_set_text(emptyWidgets.label_temp_range, "H: --\xc2\xb0  L: --\xc2\xb0");
    lv_obj_set_pos(emptyWidgets.label_temp_range, 0, 102);

    // Divider
    lv_obj_t* div = lv_obj_create(left_col);
    lv_obj_set_size(div, 300, 1);
    lv_obj_set_pos(div, 0, 124);
    lv_obj_set_style_bg_color(div, COLOR_BORDER, 0);
    lv_obj_set_style_border_width(div, 0, 0);
    lv_obj_clear_flag(div, LV_OBJ_FLAG_CLICKABLE);

    // Wind
    lv_obj_t* lbl_wind_hdr = lv_label_create(left_col);
    lv_obj_set_style_text_font(lbl_wind_hdr, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(lbl_wind_hdr, COLOR_TEXT_DIM, 0);
    lv_label_set_text(lbl_wind_hdr, "WIND");
    lv_obj_set_pos(lbl_wind_hdr, 0, 134);

    emptyWidgets.label_wind = lv_label_create(left_col);
    lv_obj_set_style_text_font(emptyWidgets.label_wind, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(emptyWidgets.label_wind, COLOR_TEXT_PRIMARY, 0);
    lv_label_set_text(emptyWidgets.label_wind, "-- mph");
    lv_obj_set_pos(emptyWidgets.label_wind, 0, 150);

    // Humidity
    lv_obj_t* lbl_hum_hdr = lv_label_create(left_col);
    lv_obj_set_style_text_font(lbl_hum_hdr, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(lbl_hum_hdr, COLOR_TEXT_DIM, 0);
    lv_label_set_text(lbl_hum_hdr, "HUMIDITY");
    lv_obj_set_pos(lbl_hum_hdr, 100, 134);

    emptyWidgets.label_humidity_val = lv_label_create(left_col);
    lv_obj_set_style_text_font(emptyWidgets.label_humidity_val, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(emptyWidgets.label_humidity_val, COLOR_TEXT_PRIMARY, 0);
    lv_label_set_text(emptyWidgets.label_humidity_val, "--%");
    lv_obj_set_pos(emptyWidgets.label_humidity_val, 100, 150);

    // Sunrise/Sunset (amber, 14px)
    emptyWidgets.label_sunrise = lv_label_create(left_col);
    lv_obj_set_style_text_font(emptyWidgets.label_sunrise, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(emptyWidgets.label_sunrise, COLOR_AMBER, 0);
    lv_label_set_text(emptyWidgets.label_sunrise, LV_SYMBOL_UP " --:--");
    lv_obj_set_pos(emptyWidgets.label_sunrise, 0, 180);

    emptyWidgets.label_sunset = lv_label_create(left_col);
    lv_obj_set_style_text_font(emptyWidgets.label_sunset, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(emptyWidgets.label_sunset, COLOR_AMBER, 0);
    lv_label_set_text(emptyWidgets.label_sunset, LV_SYMBOL_DOWN " --:--");
    lv_obj_set_pos(emptyWidgets.label_sunset, 80, 180);

    // No-contacts watermark
    lv_obj_t* watermark = lv_label_create(screen_home_empty);
    lv_obj_set_style_text_font(watermark, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(watermark, COLOR_BORDER, 0);
    lv_label_set_text(watermark, "NO CONTACTS IN RANGE");
    lv_obj_align(watermark, LV_ALIGN_BOTTOM_MID, 0, -32);

    // Right column: 5-day forecast (flex-fill)
    lv_obj_t* right_col = lv_obj_create(body);
    lv_obj_set_flex_grow(right_col, 1);
    lv_obj_set_height(right_col, 396);
    lv_obj_set_style_bg_opa(right_col, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_side(right_col, LV_BORDER_SIDE_LEFT, 0);
    lv_obj_set_style_border_color(right_col, COLOR_BORDER, 0);
    lv_obj_set_style_border_width(right_col, 1, 0);
    lv_obj_set_style_pad_hor(right_col, 20, 0);
    lv_obj_set_style_pad_ver(right_col, 16, 0);
    lv_obj_clear_flag(right_col, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* fc_hdr = lv_label_create(right_col);
    lv_obj_set_style_text_font(fc_hdr, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(fc_hdr, COLOR_TEXT_DIM, 0);
    lv_label_set_text(fc_hdr, "5-DAY FORECAST");
    lv_obj_set_pos(fc_hdr, 0, 0);

    lv_obj_t* fc_div = lv_obj_create(right_col);
    lv_obj_set_size(fc_div, LV_PCT(100), 1);
    lv_obj_set_pos(fc_div, 0, 18);
    lv_obj_set_style_bg_color(fc_div, COLOR_BORDER, 0);
    lv_obj_set_style_border_width(fc_div, 0, 0);
    lv_obj_clear_flag(fc_div, LV_OBJ_FLAG_CLICKABLE);

    for (int i = 0; i < 5; i++) {
        int y = 26 + i * 60;
        WeatherWidgets::ForecastRow& row = emptyWidgets.forecast[i];

        row.container = lv_obj_create(right_col);
        lv_obj_set_size(row.container, LV_PCT(100), 54);
        lv_obj_set_pos(row.container, 0, y);
        lv_obj_set_style_bg_opa(row.container, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_side(row.container, LV_BORDER_SIDE_BOTTOM, 0);
        lv_obj_set_style_border_color(row.container, COLOR_BORDER, 0);
        lv_obj_set_style_border_width(row.container, 1, 0);
        lv_obj_set_style_radius(row.container, 0, 0);
        lv_obj_set_style_pad_all(row.container, 0, 0);
        lv_obj_clear_flag(row.container, LV_OBJ_FLAG_SCROLLABLE);

        row.label_day = lv_label_create(row.container);
        lv_obj_set_style_text_font(row.label_day, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(row.label_day, COLOR_TEXT_SECONDARY, 0);
        lv_label_set_text(row.label_day, "---");
        lv_obj_align(row.label_day, LV_ALIGN_LEFT_MID, 0, 0);

        row.label_cond = lv_label_create(row.container);
        lv_obj_set_style_text_font(row.label_cond, &lv_font_montserrat_12, 0);
        lv_obj_set_style_text_color(row.label_cond, COLOR_TEXT_DIM, 0);
        lv_label_set_text(row.label_cond, "");
        lv_obj_align(row.label_cond, LV_ALIGN_CENTER, 0, 0);

        row.label_hi = lv_label_create(row.container);
        lv_obj_set_style_text_font(row.label_hi, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(row.label_hi, COLOR_TEXT_PRIMARY, 0);
        lv_label_set_text(row.label_hi, "--\xc2\xb0");
        lv_obj_align(row.label_hi, LV_ALIGN_RIGHT_MID, -30, 0);

        row.label_lo = lv_label_create(row.container);
        lv_obj_set_style_text_font(row.label_lo, &lv_font_montserrat_12, 0);
        lv_obj_set_style_text_color(row.label_lo, COLOR_TEXT_DIM, 0);
        lv_label_set_text(row.label_lo, "--\xc2\xb0");
        lv_obj_align(row.label_lo, LV_ALIGN_RIGHT_MID, 0, 0);
    }
}

void LVGLDisplayManager::build_no_aircraft_screen() {
    // Backward-compat stub: the no-aircraft state is now handled by screen_home_empty,
    // built in build_home_empty_screen(). Nothing to do here.
}

void LVGLDisplayManager::build_aircraft_screen() {
    screen_aircraft = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(screen_aircraft, COLOR_BG, 0);
    lv_obj_set_style_bg_opa(screen_aircraft, LV_OPA_COVER, 0);

    // === TOP BAR (70px — slightly taller than home) ===
    lv_obj_t* topbar = lv_obj_create(screen_aircraft);
    lv_obj_set_size(topbar, hal::Elecrow5Inch::PANEL_WIDTH, 70);
    lv_obj_set_pos(topbar, 0, 0);
    lv_obj_set_style_bg_color(topbar, COLOR_TOPBAR, 0);
    lv_obj_set_style_border_side(topbar, LV_BORDER_SIDE_BOTTOM, 0);
    lv_obj_set_style_border_color(topbar, COLOR_BORDER, 0);
    lv_obj_set_style_border_width(topbar, 1, 0);
    lv_obj_set_style_radius(topbar, 0, 0);
    lv_obj_set_style_pad_all(topbar, 0, 0);
    lv_obj_clear_flag(topbar, LV_OBJ_FLAG_SCROLLABLE);

    // Back button (left)
    btn_back_home = lv_button_create(topbar);
    lv_obj_set_size(btn_back_home, 90, 42);
    lv_obj_align(btn_back_home, LV_ALIGN_LEFT_MID, 16, 0);
    lv_obj_set_style_bg_opa(btn_back_home, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_color(btn_back_home, COLOR_ACCENT, 0);
    lv_obj_set_style_border_width(btn_back_home, 1, 0);
    lv_obj_set_style_border_opa(btn_back_home, 80, 0);
    lv_obj_set_style_radius(btn_back_home, 4, 0);
    lv_obj_add_event_cb(btn_back_home, event_btn_back_home, LV_EVENT_CLICKED, this);

    lv_obj_t* back_lbl = lv_label_create(btn_back_home);
    lv_obj_set_style_text_font(back_lbl, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(back_lbl, COLOR_ACCENT, 0);
    lv_label_set_text(back_lbl, LV_SYMBOL_LEFT " BACK");
    lv_obj_center(back_lbl);

    // Callsign (center, 36px bold cyan)
    label_callsign = lv_label_create(topbar);
    lv_obj_set_style_text_font(label_callsign, &lv_font_montserrat_36, 0);
    lv_obj_set_style_text_color(label_callsign, COLOR_ACCENT, 0);
    lv_label_set_text(label_callsign, "---");
    lv_obj_align(label_callsign, LV_ALIGN_CENTER, 0, 0);

    // Distance + bearing (right side, stacked)
    label_distance = lv_label_create(topbar);
    lv_obj_set_style_text_font(label_distance, &lv_font_montserrat_22, 0);
    lv_obj_set_style_text_color(label_distance, COLOR_TEXT_PRIMARY, 0);
    lv_label_set_text(label_distance, "-- nm \xc2\xb7 --\xc2\xb0");
    lv_obj_align(label_distance, LV_ALIGN_RIGHT_MID, -18, -8);

    lv_obj_t* lbl_loc = lv_label_create(topbar);
    lv_obj_set_style_text_font(lbl_loc, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(lbl_loc, COLOR_TEXT_DIM, 0);
    lv_label_set_text(lbl_loc, "FROM " WEATHER_CITY_MACRO);
    lv_obj_align(lbl_loc, LV_ALIGN_RIGHT_MID, -18, 12);

    // === STATUS BAR (26px, bottom) ===
    lv_obj_t* sbar = lv_obj_create(screen_aircraft);
    lv_obj_set_size(sbar, hal::Elecrow5Inch::PANEL_WIDTH, 26);
    lv_obj_align(sbar, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_color(sbar, COLOR_STATUSBAR, 0);
    lv_obj_set_style_border_side(sbar, LV_BORDER_SIDE_TOP, 0);
    lv_obj_set_style_border_color(sbar, COLOR_BORDER, 0);
    lv_obj_set_style_border_width(sbar, 1, 0);
    lv_obj_set_style_radius(sbar, 0, 0);
    lv_obj_set_style_pad_hor(sbar, 12, 0);
    lv_obj_clear_flag(sbar, LV_OBJ_FLAG_SCROLLABLE);

    label_status_aircraft = lv_label_create(sbar);
    lv_obj_set_style_text_font(label_status_aircraft, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(label_status_aircraft, COLOR_TEXT_DIM, 0);
    lv_label_set_text(label_status_aircraft, "");
    lv_obj_align(label_status_aircraft, LV_ALIGN_LEFT_MID, 0, 0);

    // === BODY (top:70, bottom:26, height:384, padding 14 18) ===
    lv_obj_t* body = lv_obj_create(screen_aircraft);
    lv_obj_set_size(body, hal::Elecrow5Inch::PANEL_WIDTH, 384);
    lv_obj_set_pos(body, 0, 70);
    lv_obj_set_style_bg_opa(body, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(body, 0, 0);
    lv_obj_set_style_pad_hor(body, 18, 0);
    lv_obj_set_style_pad_ver(body, 14, 0);
    lv_obj_set_style_radius(body, 0, 0);
    lv_obj_clear_flag(body, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_layout(body, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(body, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(body, 10, 0);
    lv_obj_set_style_pad_column(body, 10, 0);

    // === ROW 1: Identity cards (flex row) ===
    lv_obj_t* row1 = lv_obj_create(body);
    lv_obj_set_size(row1, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(row1, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(row1, 0, 0);
    lv_obj_set_style_pad_all(row1, 0, 0);
    lv_obj_set_style_radius(row1, 0, 0);
    lv_obj_clear_flag(row1, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_layout(row1, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(row1, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row1, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(row1, 10, 0);
    lv_obj_set_style_pad_column(row1, 10, 0);

    // Helper lambda: create an identity card
    auto makeCard = [&](lv_obj_t* parent, const char* labelText, lv_obj_t** valueLabel,
                        bool isSquawk = false) {
        lv_obj_t* card = lv_obj_create(parent);
        lv_obj_set_size(card, LV_SIZE_CONTENT, 52);
        lv_obj_set_style_bg_color(card, COLOR_PANEL, 0);
        lv_obj_set_style_border_color(card, COLOR_BORDER, 0);
        lv_obj_set_style_border_width(card, 1, 0);
        lv_obj_set_style_radius(card, 4, 0);
        lv_obj_set_style_pad_hor(card, 12, 0);
        lv_obj_set_style_pad_ver(card, 6, 0);
        lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_t* lbl = lv_label_create(card);
        lv_obj_set_style_text_font(lbl, &lv_font_montserrat_12, 0);
        lv_obj_set_style_text_color(lbl, COLOR_TEXT_DIM, 0);
        lv_label_set_text(lbl, labelText);
        lv_obj_align(lbl, LV_ALIGN_TOP_LEFT, 0, 0);

        *valueLabel = lv_label_create(card);
        lv_obj_set_style_text_font(*valueLabel, &lv_font_montserrat_16, 0);
        lv_obj_set_style_text_color(*valueLabel,
            isSquawk ? COLOR_AMBER : COLOR_TEXT_PRIMARY, 0);
        lv_label_set_text(*valueLabel, "--");
        lv_obj_align(*valueLabel, LV_ALIGN_BOTTOM_LEFT, 0, 0);
    };

    makeCard(row1, "AIRLINE",  &label_airline);
    makeCard(row1, "AIRCRAFT", &label_aircraft_type);
    makeCard(row1, "SQUAWK",   &label_squawk, true);

    // === ROW 2: Route block ===
    lv_obj_t* route_block = lv_obj_create(body);
    lv_obj_set_size(route_block, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_style_bg_color(route_block, COLOR_INSET, 0);
    lv_obj_set_style_border_color(route_block, COLOR_BORDER_ACCENT, 0);
    lv_obj_set_style_border_width(route_block, 1, 0);
    lv_obj_set_style_radius(route_block, 6, 0);
    lv_obj_set_style_pad_hor(route_block, 18, 0);
    lv_obj_set_style_pad_ver(route_block, 12, 0);
    lv_obj_clear_flag(route_block, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* route_hdr = lv_label_create(route_block);
    lv_obj_set_style_text_font(route_hdr, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(route_hdr, COLOR_TEXT_DIM, 0);
    lv_label_set_text(route_hdr, "ROUTE");
    lv_obj_align(route_hdr, LV_ALIGN_TOP_LEFT, 0, 0);

    label_route_main = lv_label_create(route_block);
    lv_obj_set_style_text_font(label_route_main, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(label_route_main, COLOR_ACCENT, 0);
    lv_label_set_text(label_route_main, "LOOKING UP...");
    lv_obj_align(label_route_main, LV_ALIGN_TOP_LEFT, 0, 18);

    label_route_sub = lv_label_create(route_block);
    lv_obj_set_style_text_font(label_route_sub, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(label_route_sub, COLOR_TEXT_SECONDARY, 0);
    lv_label_set_text(label_route_sub, "");
    lv_obj_align(label_route_sub, LV_ALIGN_TOP_LEFT, 0, 52);

    // === ROW 3: Instrument tiles ===
    lv_obj_t* row3 = lv_obj_create(body);
    lv_obj_set_flex_grow(row3, 1);
    lv_obj_set_width(row3, LV_PCT(100));
    lv_obj_set_style_bg_opa(row3, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(row3, 0, 0);
    lv_obj_set_style_pad_all(row3, 0, 0);
    lv_obj_set_style_radius(row3, 0, 0);
    lv_obj_clear_flag(row3, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_layout(row3, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(row3, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row3, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    auto makeTile = [&](lv_obj_t* parent, const char* tileLabel, lv_obj_t** valueLabel) {
        lv_obj_t* tile = lv_obj_create(parent);
        lv_obj_set_flex_grow(tile, 1);
        lv_obj_set_height(tile, LV_PCT(100));
        lv_obj_set_style_bg_color(tile, COLOR_PANEL, 0);
        lv_obj_set_style_border_color(tile, COLOR_BORDER, 0);
        lv_obj_set_style_border_width(tile, 1, 0);
        lv_obj_set_style_radius(tile, 5, 0);
        lv_obj_set_style_pad_all(tile, 10, 0);
        lv_obj_clear_flag(tile, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_layout(tile, LV_LAYOUT_FLEX);
        lv_obj_set_flex_flow(tile, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_flex_align(tile, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_pad_row(tile, 6, 0);
        lv_obj_set_style_pad_column(tile, 6, 0);

        lv_obj_t* lbl = lv_label_create(tile);
        lv_obj_set_style_text_font(lbl, &lv_font_montserrat_12, 0);
        lv_obj_set_style_text_color(lbl, COLOR_TEXT_DIM, 0);
        lv_label_set_text(lbl, tileLabel);

        *valueLabel = lv_label_create(tile);
        lv_obj_set_style_text_font(*valueLabel, &lv_font_montserrat_22, 0);
        lv_obj_set_style_text_color(*valueLabel, COLOR_TEXT_PRIMARY, 0);
        lv_label_set_text(*valueLabel, "--");
    };

    makeTile(row3, "ALTITUDE",   &label_altitude);
    makeTile(row3, "SPEED",      &label_velocity);
    makeTile(row3, "HEADING",    &label_heading);
    makeTile(row3, "VERT SPEED", &label_vert_speed);
}

// Update aircraft screen
void LVGLDisplayManager::update_aircraft_screen(const Aircraft& aircraft) {
    if (!screen_aircraft) return;

    lv_label_set_text(label_callsign, aircraft.callsign.c_str());

    // Distance + bearing from home
    float distNm = GeoUtils::distanceNm(
        Config::HOME_LAT, Config::HOME_LON,
        aircraft.latitude, aircraft.longitude);
    float bearing = GeoUtils::bearingDeg(
        Config::HOME_LAT, Config::HOME_LON,
        aircraft.latitude, aircraft.longitude);
    const char* card = GeoUtils::cardinalDir(bearing);

    char dist_buf[48];
    snprintf(dist_buf, sizeof(dist_buf), "%.1f nm  \xc2\xb7  %.0f\xc2\xb0 %s", distNm, bearing, card);
    lv_label_set_text(label_distance, dist_buf);

    // Identity cards
    lv_label_set_text(label_airline,
        aircraft.airline.length() > 0 ? aircraft.airline.c_str() : "Unknown");
    lv_label_set_text(label_aircraft_type,
        aircraft.aircraftType.length() > 0 ? aircraft.aircraftType.c_str() : "Aircraft");
    lv_label_set_text(label_squawk,
        aircraft.squawk.length() > 0 ? aircraft.squawk.c_str() : "----");

    // Route block
    if (aircraft.origin.length() > 0 && aircraft.destination.length() > 0) {
        char route_buf[64];
        snprintf(route_buf, sizeof(route_buf), "%s  \xe2\x86\x92  %s",
                 aircraft.origin.c_str(), aircraft.destination.c_str());
        lv_label_set_text(label_route_main, route_buf);
        lv_obj_set_style_text_color(label_route_main, COLOR_ACCENT, 0);
        lv_label_set_text(label_route_sub, "");
    } else if (aircraft.callsign.length() > 0) {
        lv_label_set_text(label_route_main, "LOOKING UP...");
        lv_obj_set_style_text_color(label_route_main, COLOR_TEXT_DIM, 0);
        lv_label_set_text(label_route_sub, "");
    } else {
        lv_label_set_text(label_route_main, "ROUTE UNAVAILABLE");
        lv_obj_set_style_text_color(label_route_main, COLOR_TEXT_DIM, 0);
        lv_label_set_text(label_route_sub, "");
    }

    // Altitude (m -> ft)
    char alt_buf[24];
    snprintf(alt_buf, sizeof(alt_buf), "%.0f ft", aircraft.altitude * 3.28084f);
    lv_label_set_text(label_altitude, alt_buf);

    // Speed (m/s -> kts)
    char vel_buf[24];
    snprintf(vel_buf, sizeof(vel_buf), "%.0f kts", aircraft.velocity * 1.94384f);
    lv_label_set_text(label_velocity, vel_buf);

    // Heading + cardinal
    char head_buf[24];
    snprintf(head_buf, sizeof(head_buf), "%.0f\xc2\xb0 %s",
             aircraft.heading, GeoUtils::cardinalDir(aircraft.heading));
    lv_label_set_text(label_heading, head_buf);

    // Vertical speed (m/s -> fpm), color-coded
    float fpm = aircraft.verticalRate * 196.85f;
    char vs_buf[24];
    snprintf(vs_buf, sizeof(vs_buf), "%+.0f fpm", fpm);
    lv_label_set_text(label_vert_speed, vs_buf);
    if (fpm > 50.0f) {
        lv_obj_set_style_text_color(label_vert_speed, COLOR_SUCCESS, 0);
    } else if (fpm < -50.0f) {
        lv_obj_set_style_text_color(label_vert_speed, COLOR_DESCENT, 0);
    } else {
        lv_obj_set_style_text_color(label_vert_speed, COLOR_TEXT_PRIMARY, 0);
    }
}

// Update clock
void LVGLDisplayManager::update_clock(WeatherWidgets& w) {
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo)) return;

    char time_buf[16];
    strftime(time_buf, sizeof(time_buf), "%I:%M %p", &timeinfo);
    if (w.label_time) lv_label_set_text(w.label_time, time_buf);

    char date_buf[32];
    strftime(date_buf, sizeof(date_buf), "%a, %b %d", &timeinfo);
    if (w.label_date) lv_label_set_text(w.label_date, date_buf);

    if (statusClearTime > 0 && millis() >= statusClearTime) {
        statusClearTime = 0;
        statusMessage = "";
        if (homeWidgets.label_status_left)  lv_label_set_text(homeWidgets.label_status_left,  "");
        if (emptyWidgets.label_status_left) lv_label_set_text(emptyWidgets.label_status_left, "");
        if (label_status_aircraft)          lv_label_set_text(label_status_aircraft,           "");
    }
}

// Main update function
void LVGLDisplayManager::update(const WeatherData& weather, const Aircraft* aircraft, int aircraftCount) {
    if (currentScreen == SCREEN_HOME) {
        update_home_screen(weather, aircraft, aircraftCount);
    } else if (currentScreen == SCREEN_AIRCRAFT_DETAIL && aircraft && aircraft->valid) {
        update_aircraft_screen(*aircraft);
    }
}

// Tick function - call frequently in loop()
void LVGLDisplayManager::tick(uint32_t period_ms) {
    lv_tick_inc(period_ms);
    lv_timer_handler();
}

// Screen management
void LVGLDisplayManager::setScreen(ScreenState screen) {
    ScreenState target = (screen == SCREEN_NO_AIRCRAFT) ? SCREEN_HOME : screen;
    if (currentScreen == target) return;

    currentScreen = target;
    lastScreenChange = millis();

    switch (target) {
        case SCREEN_HOME:
            lv_screen_load(homeHasAircraft ? screen_home : screen_home_empty);
            break;
        case SCREEN_AIRCRAFT_DETAIL:
            lv_screen_load(screen_aircraft);
            break;
        default:
            break;
    }
}

// Check if should return to home
bool LVGLDisplayManager::shouldReturnToHome() {
    if (currentScreen == SCREEN_HOME) return false;
    
    unsigned long idleTime = millis() - lastUserInteraction;
    return idleTime >= Config::UI_AUTO_HOME_MS;
}

// Event callbacks
void LVGLDisplayManager::event_btn_view_planes(lv_event_t* e) {
    LVGLDisplayManager* mgr = (LVGLDisplayManager*)lv_event_get_user_data(e);
    if (mgr) {
        mgr->setScreen(SCREEN_AIRCRAFT_DETAIL);
    }
}

void LVGLDisplayManager::event_btn_back_home(lv_event_t* e) {
    LVGLDisplayManager* mgr = (LVGLDisplayManager*)lv_event_get_user_data(e);
    if (mgr) {
        mgr->setScreen(SCREEN_HOME);
    }
}

// Utility functions
String LVGLDisplayManager::formatTime(time_t timestamp) {
    struct tm timeinfo;
    localtime_r(&timestamp, &timeinfo);
    char buf[16];
    strftime(buf, sizeof(buf), "%H:%M", &timeinfo);
    return String(buf);
}

String LVGLDisplayManager::formatDate(time_t timestamp) {
    struct tm timeinfo;
    localtime_r(&timestamp, &timeinfo);
    char buf[32];
    strftime(buf, sizeof(buf), "%a, %b %d", &timeinfo);
    return String(buf);
}

// Brightness control
void LVGLDisplayManager::setBrightness(uint8_t brightness) {
    currentBrightness = brightness;
    if (lcd) {
        lcd->setBrightness(brightness);
    }
}

// Timestamp tracking
void LVGLDisplayManager::setLastUpdateTimestamp(time_t timestamp) {
    lastUpdateTime = timestamp;
}

void LVGLDisplayManager::setStatusMessage(const String& msg) {
    statusMessage = msg;
    statusClearTime = millis() + Config::UI_STATUS_MS;
    if (homeWidgets.label_status_left)  lv_label_set_text(homeWidgets.label_status_left,  msg.c_str());
    if (emptyWidgets.label_status_left) lv_label_set_text(emptyWidgets.label_status_left, msg.c_str());
    if (label_status_aircraft)          lv_label_set_text(label_status_aircraft,           msg.c_str());
}

// Touch processing (LVGL handles this automatically)
void LVGLDisplayManager::processTouch() {
    // LVGL handles touch automatically via touchpad_read callback
}

// Accessors
lgfx::LGFX_Device* LVGLDisplayManager::getDisplay() {
    return lcd;
}

lgfx::LGFX_Device* LVGLDisplayManager::getLCD() {
    return lcd;
}
