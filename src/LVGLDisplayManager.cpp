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
#include "data/CoastlinePortland.h"
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
    , screen_radar(nullptr)
    , currentScreen(SCREEN_HOME)
    , lastScreenChange(0)
    , lastUserInteraction(0)
    , statusMessage("")
    , statusClearTime(0)
    , lastUpdateTime(0)
    , currentBrightness(255)
    , userDismissed_(false)
    , list_selected_idx_(-1)
{
    s_instance = this;
}

// Destructor
LVGLDisplayManager::~LVGLDisplayManager() {
    // Acquire lock so the task completes any in-progress handler cycle before being deleted
    lv_lock();
    if (lvgl_task_handle_) {
        vTaskDelete(lvgl_task_handle_);
        lvgl_task_handle_ = nullptr;
    }
    lv_unlock();
    if (lvgl_tick_timer_) {
        esp_timer_stop(lvgl_tick_timer_);
        esp_timer_delete(lvgl_tick_timer_);
        lvgl_tick_timer_ = nullptr;
    }
    delete lcd;
    lcd = nullptr;
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
        args.callback = [](void*) { lv_tick_inc(1); };  // lv_tick_inc is IRQ-safe; intentionally outside lv_lock
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
            esp_timer_delete(lvgl_tick_timer_);
            lvgl_tick_timer_ = nullptr;
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
    lv_indev_set_disp(lv_indev, lv_display);   // LVGL 9: explicitly bind indev to display
    lv_indev_set_scroll_limit(lv_indev, 20);   // 20px before tap becomes scroll (default 10 too sensitive)
    
    // Build screens
    build_home_screen();
    build_radar_screen();

    // Load home screen at boot
    lv_screen_load(screen_home);
    currentScreen = SCREEN_HOME;
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
    lv_label_set_text(w.label_status_live, "IDLE");
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
            snprintf(buf, sizeof(buf), "OPENSKY OK / %s", ts);
            lv_label_set_text(w.label_status_left, buf);
        } else {
            lv_label_set_text(w.label_status_left, "NO AIRCRAFT DETECTED");
        }
    }
    if (w.label_status_live) {
        if (aircraftCount > 0) {
            lv_obj_set_style_text_color(w.label_status_live, COLOR_SUCCESS, 0);
            lv_label_set_text(w.label_status_live, "LIVE");
        } else {
            lv_obj_set_style_text_color(w.label_status_live, COLOR_TEXT_DIM, 0);
            lv_label_set_text(w.label_status_live, "IDLE");
        }
    }
}

void LVGLDisplayManager::build_home_screen() {
    screen_home = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(screen_home, COLOR_BG, 0);
    lv_obj_set_style_bg_opa(screen_home, LV_OPA_COVER, 0);

    buildTopBar(screen_home, homeWidgets);
    buildStatusBar(screen_home, homeWidgets);

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

    // Left: weather panel (flex-fill)
    lv_obj_t* wx_col = lv_obj_create(body);
    lv_obj_set_flex_grow(wx_col, 1);
    lv_obj_set_height(wx_col, 396);
    buildWeatherPanel(wx_col, homeWidgets);

    // Right: airspace status (310px)
    lv_obj_t* ap_col = lv_obj_create(body);
    lv_obj_set_size(ap_col, 310, 396);
    buildAirspacePanel(ap_col);
}

void LVGLDisplayManager::buildAirspacePanel(lv_obj_t* parent) {
    lv_obj_set_style_bg_color(parent, COLOR_PANEL, 0);
    lv_obj_set_style_bg_opa(parent, LV_OPA_COVER, 0);
    lv_obj_set_style_border_side(parent, LV_BORDER_SIDE_LEFT, 0);
    lv_obj_set_style_border_color(parent, COLOR_BORDER, 0);
    lv_obj_set_style_border_width(parent, 1, 0);
    lv_obj_set_style_radius(parent, 0, 0);
    lv_obj_set_style_pad_all(parent, 10, 0);
    lv_obj_clear_flag(parent, LV_OBJ_FLAG_SCROLLABLE);

    // Header
    lv_obj_t* hdr = lv_label_create(parent);
    lv_obj_set_style_text_font(hdr, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(hdr, COLOR_TEXT_DIM, 0);
    lv_label_set_text(hdr, "LOCAL AIRSPACE");
    lv_obj_set_pos(hdr, 10, 10);

    label_airspace_range_ = lv_label_create(parent);
    lv_obj_set_style_text_font(label_airspace_range_, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(label_airspace_range_, COLOR_TEXT_DIM, 0);
    lv_label_set_text(label_airspace_range_, "25 nm radius");
    lv_obj_set_pos(label_airspace_range_, 10, 28);

    // Dim radar circle (240px diameter, centered in 310px panel)
    airspace_circle_ = lv_obj_create(parent);
    lv_obj_set_size(airspace_circle_, 240, 240);
    lv_obj_set_pos(airspace_circle_, 25, 48);
    lv_obj_set_style_radius(airspace_circle_, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(airspace_circle_, COLOR_INSET, 0);
    lv_obj_set_style_bg_opa(airspace_circle_, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(airspace_circle_, COLOR_BORDER, 0);
    lv_obj_set_style_border_width(airspace_circle_, 1, 0);
    lv_obj_set_style_pad_all(airspace_circle_, 0, 0);
    lv_obj_set_style_clip_corner(airspace_circle_, true, 0);
    lv_obj_clear_flag(airspace_circle_, (lv_obj_flag_t)(LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE));

    // Home dot (center, dim)
    lv_obj_t* home = lv_obj_create(airspace_circle_);
    lv_obj_set_size(home, 6, 6);
    lv_obj_center(home);
    lv_obj_set_style_radius(home, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(home, COLOR_TEXT_DIM, 0);
    lv_obj_set_style_border_width(home, 0, 0);
    lv_obj_clear_flag(home, LV_OBJ_FLAG_CLICKABLE);

    // Project coastline to pixel coords (120px radius for 240px circle)
    for (int i = 0; i < GeoUtils::COASTLINE_PORTLAND_LEN; i++) {
        auto p = GeoUtils::latLonToRadarPx(
            Config::HOME_LAT, Config::HOME_LON,
            GeoUtils::COASTLINE_PORTLAND[i].lat, GeoUtils::COASTLINE_PORTLAND[i].lon,
            Config::RADAR_MAX_RANGE_NM, 120);
        airspace_pts_[i] = {(lv_value_precise_t)p.x, (lv_value_precise_t)p.y};
    }

    airspace_coastline_ = lv_line_create(airspace_circle_);
    lv_line_set_points(airspace_coastline_, airspace_pts_, GeoUtils::COASTLINE_PORTLAND_LEN);
    lv_obj_set_style_line_color(airspace_coastline_, lv_color_hex(0x1e3a54), 0);
    lv_obj_set_style_line_width(airspace_coastline_, 2, 0);
    lv_obj_set_style_line_opa(airspace_coastline_, LV_OPA_COVER, 0);

    // Status labels below circle
    label_airspace_status_ = lv_label_create(parent);
    lv_obj_set_style_text_font(label_airspace_status_, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(label_airspace_status_, COLOR_SUCCESS, 0);
    lv_label_set_text(label_airspace_status_, "\xe2\x97\x8f AIRSPACE CLEAR");
    lv_obj_set_pos(label_airspace_status_, 10, 298);

    label_airspace_sub_ = lv_label_create(parent);
    lv_obj_set_style_text_font(label_airspace_sub_, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(label_airspace_sub_, COLOR_TEXT_DIM, 0);
    lv_label_set_text(label_airspace_sub_, "No aircraft within 25nm");
    lv_obj_set_pos(label_airspace_sub_, 10, 318);
}

void LVGLDisplayManager::build_radar_screen() {
    if (screen_radar) return;  // already built — prevent leak on re-init
    screen_radar = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(screen_radar, COLOR_BG, 0);
    lv_obj_set_style_bg_opa(screen_radar, LV_OPA_COVER, 0);

    // === TOP BAR (58px) ===
    lv_obj_t* topbar = lv_obj_create(screen_radar);
    lv_obj_set_size(topbar, hal::Elecrow5Inch::PANEL_WIDTH, 58);
    lv_obj_set_pos(topbar, 0, 0);
    lv_obj_set_style_bg_color(topbar, COLOR_TOPBAR, 0);
    lv_obj_set_style_border_side(topbar, LV_BORDER_SIDE_BOTTOM, 0);
    lv_obj_set_style_border_color(topbar, COLOR_BORDER, 0);
    lv_obj_set_style_border_width(topbar, 1, 0);
    lv_obj_set_style_radius(topbar, 0, 0);
    lv_obj_set_style_pad_all(topbar, 0, 0);
    lv_obj_clear_flag(topbar, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(topbar, event_topbar_back, LV_EVENT_CLICKED, this);

    label_radar_time_ = lv_label_create(topbar);
    lv_obj_set_style_text_font(label_radar_time_, &lv_font_montserrat_22, 0);
    lv_obj_set_style_text_color(label_radar_time_, COLOR_ACCENT, 0);
    lv_label_set_text(label_radar_time_, "--:--");
    lv_obj_align(label_radar_time_, LV_ALIGN_LEFT_MID, 16, -6);

    label_radar_date_ = lv_label_create(topbar);
    lv_obj_set_style_text_font(label_radar_date_, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(label_radar_date_, COLOR_TEXT_DIM, 0);
    lv_label_set_text(label_radar_date_, "--- -- ----");
    lv_obj_align(label_radar_date_, LV_ALIGN_LEFT_MID, 16, 10);

    label_radar_count_ = lv_label_create(topbar);
    lv_obj_set_style_text_font(label_radar_count_, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(label_radar_count_, COLOR_AMBER, 0);
    lv_label_set_text(label_radar_count_, "\xe2\x97\x8f 0 AIRCRAFT NEARBY");
    lv_obj_align(label_radar_count_, LV_ALIGN_CENTER, 0, 0);

    lv_obj_t* hint = lv_label_create(topbar);
    lv_obj_set_style_text_font(hint, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(hint, COLOR_TEXT_DIM, 0);
    lv_label_set_text(hint, "tap bar \xe2\x86\x90 home");
    lv_obj_align(hint, LV_ALIGN_RIGHT_MID, -16, 0);

    // === BODY ===
    lv_obj_t* body = lv_obj_create(screen_radar);
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

    // Left panel: radar (490px)
    lv_obj_t* radar_col = lv_obj_create(body);
    lv_obj_set_size(radar_col, 490, 396);
    lv_obj_set_style_bg_opa(radar_col, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(radar_col, 0, 0);
    lv_obj_set_style_pad_all(radar_col, 0, 0);
    lv_obj_clear_flag(radar_col, LV_OBJ_FLAG_SCROLLABLE);

    // Radar circle: 380px diameter, centered in 490px column -> pos (55, 8)
    radar_circle_ = lv_obj_create(radar_col);
    lv_obj_set_size(radar_circle_, 380, 380);
    lv_obj_set_pos(radar_circle_, 55, 8);
    lv_obj_set_style_radius(radar_circle_, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(radar_circle_, COLOR_INSET, 0);
    lv_obj_set_style_bg_opa(radar_circle_, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(radar_circle_, COLOR_BORDER, 0);
    lv_obj_set_style_border_width(radar_circle_, 1, 0);
    lv_obj_set_style_pad_all(radar_circle_, 0, 0);
    lv_obj_set_style_clip_corner(radar_circle_, true, 0);
    lv_obj_clear_flag(radar_circle_, (lv_obj_flag_t)(LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE));

    // Inner range ring (12.5nm = half of 25nm = 190px diameter, centered)
    lv_obj_t* ring = lv_obj_create(radar_circle_);
    lv_obj_set_size(ring, 190, 190);
    lv_obj_center(ring);
    lv_obj_set_style_radius(ring, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_opa(ring, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_color(ring, lv_color_hex(0x1a3048), 0);
    lv_obj_set_style_border_width(ring, 1, 0);
    lv_obj_clear_flag(ring, (lv_obj_flag_t)(LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE));

    // Cardinal labels (N/S/E/W) at circle edge
    struct CardinalLabel { const char* txt; lv_align_t align; int ox; int oy; };
    const CardinalLabel cards[4] = {
        {"N", LV_ALIGN_TOP_MID,    0,  4},
        {"S", LV_ALIGN_BOTTOM_MID, 0, -4},
        {"E", LV_ALIGN_RIGHT_MID, -6,  0},
        {"W", LV_ALIGN_LEFT_MID,   6,  0},
    };
    for (const auto& c : cards) {
        lv_obj_t* lbl = lv_label_create(radar_circle_);
        lv_obj_set_style_text_font(lbl, &lv_font_montserrat_12, 0);
        lv_obj_set_style_text_color(lbl, COLOR_TEXT_DIM, 0);
        lv_label_set_text(lbl, c.txt);
        lv_obj_align(lbl, c.align, c.ox, c.oy);
    }

    // Project coastline to pixel coords once (circleRadius = Config::RADAR_CIRCLE_RADIUS = 190)
    for (int i = 0; i < GeoUtils::COASTLINE_PORTLAND_LEN; i++) {
        auto p = GeoUtils::latLonToRadarPx(
            Config::HOME_LAT, Config::HOME_LON,
            GeoUtils::COASTLINE_PORTLAND[i].lat, GeoUtils::COASTLINE_PORTLAND[i].lon,
            Config::RADAR_MAX_RANGE_NM, Config::RADAR_CIRCLE_RADIUS);
        radar_pts_[i] = {(lv_value_precise_t)p.x, (lv_value_precise_t)p.y};
    }

    radar_coastline_ = lv_line_create(radar_circle_);
    lv_line_set_points(radar_coastline_, radar_pts_, GeoUtils::COASTLINE_PORTLAND_LEN);
    lv_obj_set_style_line_color(radar_coastline_, lv_color_hex(0x2a5f8a), 0);
    lv_obj_set_style_line_width(radar_coastline_, 2, 0);
    lv_obj_set_style_line_opa(radar_coastline_, LV_OPA_COVER, 0);

    // PWM airport marker: + cross
    auto pwm = GeoUtils::latLonToRadarPx(
        Config::HOME_LAT, Config::HOME_LON,
        Config::PWM_LAT, Config::PWM_LON,
        Config::RADAR_MAX_RANGE_NM, Config::RADAR_CIRCLE_RADIUS);
    // static: must outlive the lv_line objects that reference them
    static lv_point_precise_t pwm_h[2], pwm_v[2];
    pwm_h[0] = {(lv_value_precise_t)(pwm.x - 7), (lv_value_precise_t)pwm.y};
    pwm_h[1] = {(lv_value_precise_t)(pwm.x + 7), (lv_value_precise_t)pwm.y};
    pwm_v[0] = {(lv_value_precise_t)pwm.x, (lv_value_precise_t)(pwm.y - 7)};
    pwm_v[1] = {(lv_value_precise_t)pwm.x, (lv_value_precise_t)(pwm.y + 7)};

    lv_obj_t* ph = lv_line_create(radar_circle_);
    lv_line_set_points(ph, pwm_h, 2);
    lv_obj_set_style_line_color(ph, COLOR_AMBER, 0);
    lv_obj_set_style_line_width(ph, 2, 0);

    lv_obj_t* pv = lv_line_create(radar_circle_);
    lv_line_set_points(pv, pwm_v, 2);
    lv_obj_set_style_line_color(pv, COLOR_AMBER, 0);
    lv_obj_set_style_line_width(pv, 2, 0);

    lv_obj_t* pwm_lbl = lv_label_create(radar_circle_);
    lv_obj_set_style_text_font(pwm_lbl, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(pwm_lbl, COLOR_AMBER, 0);
    lv_label_set_text(pwm_lbl, "PWM");
    lv_obj_set_pos(pwm_lbl, pwm.x + 9, pwm.y - 8);

    // Home dot (center, 10px cyan)
    lv_obj_t* home_dot = lv_obj_create(radar_circle_);
    lv_obj_set_size(home_dot, 10, 10);
    lv_obj_center(home_dot);
    lv_obj_set_style_radius(home_dot, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(home_dot, COLOR_ACCENT, 0);
    lv_obj_set_style_border_width(home_dot, 0, 0);
    lv_obj_clear_flag(home_dot, LV_OBJ_FLAG_CLICKABLE);

    // Range labels (near N cardinal, inside circle edge) — lv_font_montserrat_10 not enabled; use _12
    lv_obj_t* rl1 = lv_label_create(radar_col);
    lv_obj_set_style_text_font(rl1, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(rl1, COLOR_TEXT_DIM, 0);
    lv_label_set_text(rl1, "12.5nm");
    lv_obj_set_pos(rl1, 263, 60);

    lv_obj_t* rl2 = lv_label_create(radar_col);
    lv_obj_set_style_text_font(rl2, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(rl2, COLOR_TEXT_DIM, 0);
    lv_label_set_text(rl2, "25nm");
    lv_obj_set_pos(rl2, 263, 15);

    // Pre-allocate aircraft blip objects (all hidden at startup)
    for (int i = 0; i < Config::MAX_AIRCRAFT; i++) {
        RadarBlip& b = radar_blips_[i];

        // Dot (12px filled circle)
        b.dot = lv_obj_create(radar_circle_);
        lv_obj_set_size(b.dot, 12, 12);
        lv_obj_set_style_radius(b.dot, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_color(b.dot, COLOR_ACCENT, 0);
        lv_obj_set_style_border_width(b.dot, 0, 0);
        lv_obj_set_style_pad_all(b.dot, 0, 0);
        lv_obj_set_pos(b.dot, Config::RADAR_CIRCLE_RADIUS - 6,
                               Config::RADAR_CIRCLE_RADIUS - 6);
        lv_obj_add_flag(b.dot, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(b.dot, LV_OBJ_FLAG_CLICKABLE);

        // Heading vector (lv_line, 2 points)
        b.vec_pts[0] = {(lv_value_precise_t)Config::RADAR_CIRCLE_RADIUS,
                        (lv_value_precise_t)Config::RADAR_CIRCLE_RADIUS};
        b.vec_pts[1] = {(lv_value_precise_t)Config::RADAR_CIRCLE_RADIUS,
                        (lv_value_precise_t)(Config::RADAR_CIRCLE_RADIUS - 20)};
        b.vector = lv_line_create(radar_circle_);
        lv_line_set_points(b.vector, b.vec_pts, 2);
        lv_obj_set_style_line_color(b.vector, COLOR_ACCENT, 0);
        lv_obj_set_style_line_width(b.vector, 2, 0);
        lv_obj_add_flag(b.vector, LV_OBJ_FLAG_HIDDEN);

        // Callsign label
        b.label = lv_label_create(radar_circle_);
        lv_obj_set_style_text_font(b.label, &lv_font_montserrat_12, 0);
        lv_obj_set_style_text_color(b.label, COLOR_ACCENT, 0);
        lv_label_set_text(b.label, "");
        lv_obj_set_pos(b.label, Config::RADAR_CIRCLE_RADIUS,
                                 Config::RADAR_CIRCLE_RADIUS);
        lv_obj_add_flag(b.label, LV_OBJ_FLAG_HIDDEN);
    }

    // Right panel: aircraft list (310px)
    lv_obj_t* list_col = lv_obj_create(body);
    lv_obj_set_size(list_col, 310, 396);
    lv_obj_set_style_bg_color(list_col, COLOR_PANEL, 0);
    lv_obj_set_style_bg_opa(list_col, LV_OPA_COVER, 0);
    lv_obj_set_style_border_side(list_col, LV_BORDER_SIDE_LEFT, 0);
    lv_obj_set_style_border_color(list_col, COLOR_BORDER, 0);
    lv_obj_set_style_border_width(list_col, 1, 0);
    lv_obj_set_style_radius(list_col, 0, 0);
    lv_obj_set_style_pad_all(list_col, 0, 0);
    lv_obj_clear_flag(list_col, LV_OBJ_FLAG_SCROLLABLE);

    // Header label
    label_list_header_ = lv_label_create(list_col);
    lv_obj_set_style_text_font(label_list_header_, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(label_list_header_, COLOR_TEXT_DIM, 0);
    lv_label_set_text(label_list_header_, "AIRCRAFT IN RANGE  \xc2\xb7  0");
    lv_obj_set_pos(label_list_header_, 12, 8);

    // Thin divider line
    lv_obj_t* ldiv = lv_obj_create(list_col);
    lv_obj_set_size(ldiv, 286, 1);
    lv_obj_set_pos(ldiv, 12, 26);
    lv_obj_set_style_bg_color(ldiv, COLOR_BORDER, 0);
    lv_obj_set_style_border_width(ldiv, 0, 0);
    lv_obj_clear_flag(ldiv, LV_OBJ_FLAG_CLICKABLE);

    // Scrollable list container
    list_container_ = lv_obj_create(list_col);
    lv_obj_set_size(list_container_, 310, 362);
    lv_obj_set_pos(list_container_, 0, 30);
    lv_obj_set_style_bg_opa(list_container_, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(list_container_, 0, 0);
    lv_obj_set_style_pad_all(list_container_, 0, 0);
    lv_obj_set_style_radius(list_container_, 0, 0);
    lv_obj_set_layout(list_container_, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(list_container_, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(list_container_, LV_FLEX_ALIGN_START,
                           LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_row(list_container_, 0, 0);

    // Pre-allocate rows
    for (int i = 0; i < Config::MAX_AIRCRAFT; i++) {
        AircraftListRow& row = list_rows_[i];

        row.container = lv_obj_create(list_container_);
        lv_obj_set_width(row.container, 310);
        lv_obj_set_height(row.container, 66);
        lv_obj_set_style_bg_color(row.container, COLOR_PANEL, 0);
        lv_obj_set_style_bg_opa(row.container, LV_OPA_COVER, 0);
        lv_obj_set_style_border_side(row.container, LV_BORDER_SIDE_BOTTOM, 0);
        lv_obj_set_style_border_color(row.container, COLOR_BORDER, 0);
        lv_obj_set_style_border_width(row.container, 1, 0);
        lv_obj_set_style_radius(row.container, 0, 0);
        lv_obj_set_style_pad_all(row.container, 0, 0);
        lv_obj_clear_flag(row.container, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_flag(row.container, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(row.container, event_list_row_clicked,
                            LV_EVENT_CLICKED, (void*)(intptr_t)i);
        lv_obj_add_flag(row.container, LV_OBJ_FLAG_HIDDEN);

        // Left accent bar (4px wide)
        row.accent_bar = lv_obj_create(row.container);
        lv_obj_set_size(row.accent_bar, 4, 66);
        lv_obj_set_pos(row.accent_bar, 0, 0);
        lv_obj_set_style_bg_color(row.accent_bar, COLOR_ACCENT, 0);
        lv_obj_set_style_border_width(row.accent_bar, 0, 0);
        lv_obj_set_style_radius(row.accent_bar, 0, 0);
        lv_obj_clear_flag(row.accent_bar, LV_OBJ_FLAG_CLICKABLE);

        row.label_callsign = lv_label_create(row.container);
        lv_obj_set_style_text_font(row.label_callsign, &lv_font_montserrat_16, 0);
        lv_obj_set_style_text_color(row.label_callsign, COLOR_ACCENT, 0);
        lv_label_set_text(row.label_callsign, "------");
        lv_obj_set_pos(row.label_callsign, 14, 8);

        row.label_type_route = lv_label_create(row.container);
        lv_obj_set_style_text_font(row.label_type_route, &lv_font_montserrat_12, 0);
        lv_obj_set_style_text_color(row.label_type_route, COLOR_TEXT_SECONDARY, 0);
        lv_label_set_text(row.label_type_route, "");
        lv_obj_set_pos(row.label_type_route, 14, 30);

        row.label_summary = lv_label_create(row.container);
        lv_obj_set_style_text_font(row.label_summary, &lv_font_montserrat_12, 0);
        lv_obj_set_style_text_color(row.label_summary, COLOR_TEXT_DIM, 0);
        lv_label_set_text(row.label_summary, "");
        lv_obj_set_pos(row.label_summary, 14, 48);

        // Expanded panel (hidden when collapsed)
        row.expanded_panel = lv_obj_create(row.container);
        lv_obj_set_size(row.expanded_panel, 306, 72);
        lv_obj_set_pos(row.expanded_panel, 0, 66);
        lv_obj_set_style_bg_color(row.expanded_panel, lv_color_hex(0x0a1428), 0);
        lv_obj_set_style_bg_opa(row.expanded_panel, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(row.expanded_panel, 0, 0);
        lv_obj_set_style_radius(row.expanded_panel, 0, 0);
        lv_obj_set_style_pad_hor(row.expanded_panel, 14, 0);
        lv_obj_set_style_pad_ver(row.expanded_panel, 6, 0);
        lv_obj_clear_flag(row.expanded_panel,
                          (lv_obj_flag_t)(LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE));
        lv_obj_add_flag(row.expanded_panel, LV_OBJ_FLAG_HIDDEN);

        // Expanded field labels: row 1 (alt / speed / hdg)
        row.label_alt = lv_label_create(row.expanded_panel);
        lv_obj_set_style_text_font(row.label_alt, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(row.label_alt, COLOR_TEXT_PRIMARY, 0);
        lv_label_set_text(row.label_alt, "--,--- ft");
        lv_obj_set_pos(row.label_alt, 0, 4);

        row.label_speed = lv_label_create(row.expanded_panel);
        lv_obj_set_style_text_font(row.label_speed, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(row.label_speed, COLOR_TEXT_PRIMARY, 0);
        lv_label_set_text(row.label_speed, "--- kt");
        lv_obj_set_pos(row.label_speed, 100, 4);

        row.label_hdg = lv_label_create(row.expanded_panel);
        lv_obj_set_style_text_font(row.label_hdg, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(row.label_hdg, COLOR_TEXT_PRIMARY, 0);
        lv_label_set_text(row.label_hdg, "---\xc2\xb0");
        lv_obj_set_pos(row.label_hdg, 195, 4);

        // Expanded field labels: row 2 (dist / status)
        row.label_dist = lv_label_create(row.expanded_panel);
        lv_obj_set_style_text_font(row.label_dist, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(row.label_dist, COLOR_TEXT_PRIMARY, 0);
        lv_label_set_text(row.label_dist, "-.- nm");
        lv_obj_set_pos(row.label_dist, 0, 34);

        row.label_status = lv_label_create(row.expanded_panel);
        lv_obj_set_style_text_font(row.label_status, &lv_font_montserrat_12, 0);
        lv_obj_set_style_text_color(row.label_status, COLOR_TEXT_DIM, 0);
        lv_label_set_text(row.label_status, "");
        lv_obj_set_pos(row.label_status, 100, 36);
    }

    // === STATUS BAR (26px, bottom) ===
    lv_obj_t* sbar = lv_obj_create(screen_radar);
    lv_obj_set_size(sbar, hal::Elecrow5Inch::PANEL_WIDTH, 26);
    lv_obj_align(sbar, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_color(sbar, COLOR_STATUSBAR, 0);
    lv_obj_set_style_border_side(sbar, LV_BORDER_SIDE_TOP, 0);
    lv_obj_set_style_border_color(sbar, COLOR_BORDER, 0);
    lv_obj_set_style_border_width(sbar, 1, 0);
    lv_obj_set_style_radius(sbar, 0, 0);
    lv_obj_set_style_pad_all(sbar, 0, 0);
    lv_obj_clear_flag(sbar, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* sbar_left = lv_label_create(sbar);
    lv_obj_set_style_text_font(sbar_left, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(sbar_left, COLOR_TEXT_DIM, 0);
    lv_label_set_text(sbar_left, "OPENSKY OK");
    lv_obj_align(sbar_left, LV_ALIGN_LEFT_MID, 12, 0);

    lv_obj_t* sbar_hint = lv_label_create(sbar);
    lv_obj_set_style_text_font(sbar_hint, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(sbar_hint, COLOR_TEXT_DIM, 0);
    lv_label_set_text(sbar_hint, "tap top bar to return home");
    lv_obj_align(sbar_hint, LV_ALIGN_CENTER, 0, 0);

    lv_obj_t* sbar_live = lv_label_create(sbar);
    lv_obj_set_style_text_font(sbar_live, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(sbar_live, COLOR_SUCCESS, 0);
    lv_label_set_text(sbar_live, "\xe2\x97\x8f LIVE");
    lv_obj_align(sbar_live, LV_ALIGN_RIGHT_MID, -12, 0);
}

void LVGLDisplayManager::update_home_screen(const WeatherData& weather,
                                             int aircraftCount) {
    update_clock(homeWidgets);
    updateWeatherWidgets(homeWidgets, weather, aircraftCount);

    if (!label_airspace_status_) return;

    if (aircraftCount > 0) {
        char buf[40];
        snprintf(buf, sizeof(buf), "\xe2\x97\x8f %d AIRCRAFT NEARBY", aircraftCount);
        lv_obj_set_style_text_color(label_airspace_status_, COLOR_AMBER, 0);
        lv_label_set_text(label_airspace_status_, buf);
        lv_label_set_text(label_airspace_sub_, "Switching to radar...");
    } else {
        lv_obj_set_style_text_color(label_airspace_status_, COLOR_SUCCESS, 0);
        lv_label_set_text(label_airspace_status_, "\xe2\x97\x8f AIRSPACE CLEAR");
        lv_label_set_text(label_airspace_sub_, "No aircraft within 25nm");
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
        if (homeWidgets.label_status_left) lv_label_set_text(homeWidgets.label_status_left, "");
    }
}

void LVGLDisplayManager::update_radar_screen(const Aircraft* aircraft,
                                               int aircraftCount) {
    if (!label_radar_count_) return;

    // Update top bar clock
    {
        struct tm ti;
        if (getLocalTime(&ti)) {
            char tb[12];
            strftime(tb, sizeof(tb), "%H:%M", &ti);
            lv_label_set_text(label_radar_time_, tb);
            char db[20];
            strftime(db, sizeof(db), "%a %d %b %Y", &ti);
            lv_label_set_text(label_radar_date_, db);
        }
        char cb[32];
        snprintf(cb, sizeof(cb), "\xe2\x97\x8f %d AIRCRAFT NEARBY", aircraftCount);
        lv_label_set_text(label_radar_count_, cb);
    }

    // Update list header
    {
        char hb[40];
        snprintf(hb, sizeof(hb), "AIRCRAFT IN RANGE  \xc2\xb7  %d", aircraftCount);
        lv_label_set_text(label_list_header_, hb);
    }

    // If selected aircraft left range, reset selection to row 0 (or -1 if empty)
    if (list_selected_idx_ >= aircraftCount) {
        if (list_selected_idx_ >= 0 && list_selected_idx_ < Config::MAX_AIRCRAFT) {
            lv_obj_add_flag(list_rows_[list_selected_idx_].expanded_panel, LV_OBJ_FLAG_HIDDEN);
            lv_obj_set_height(list_rows_[list_selected_idx_].container, 66);
        }
        list_selected_idx_ = (aircraftCount > 0) ? 0 : -1;
        if (list_selected_idx_ == 0) {
            lv_obj_remove_flag(list_rows_[0].expanded_panel, LV_OBJ_FLAG_HIDDEN);
            lv_obj_set_height(list_rows_[0].container, 138);
        }
    }

    // Auto-expand row 0 on first entry (no prior selection)
    if (aircraftCount > 0 && list_selected_idx_ < 0) {
        list_selected_idx_ = 0;
        lv_obj_remove_flag(list_rows_[0].expanded_panel, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_height(list_rows_[0].container, 138);
    }

    for (int i = 0; i < Config::MAX_AIRCRAFT; i++) {
        RadarBlip&       blip = radar_blips_[i];
        AircraftListRow& row  = list_rows_[i];

        if (!blip.dot || !row.container) continue;

        if (aircraft && i < aircraftCount && aircraft[i].valid) {
            const Aircraft& a = aircraft[i];

            // --- Compute position ---
            float distNm  = GeoUtils::distanceNm(Config::HOME_LAT, Config::HOME_LON,
                                                   a.latitude, a.longitude);
            float bearing = GeoUtils::bearingDeg(Config::HOME_LAT, Config::HOME_LON,
                                                  a.latitude, a.longitude);
            auto  pos     = GeoUtils::blipPosition(distNm, bearing,
                                                    Config::RADAR_MAX_RANGE_NM,
                                                    Config::RADAR_CIRCLE_RADIUS, 8);

            float altFt   = a.altitude * 3.28084f;
            lv_color_t blipColor = (altFt > 0.0f && altFt < 5000.0f)
                                   ? COLOR_AMBER : COLOR_ACCENT;

            // --- Radar blip ---
            lv_obj_set_pos(blip.dot, pos.x - 6, pos.y - 6);
            lv_obj_set_style_bg_color(blip.dot, blipColor, 0);
            lv_obj_set_style_line_color(blip.vector, blipColor, 0);
            lv_obj_set_style_text_color(blip.label, blipColor, 0);
            lv_obj_remove_flag(blip.dot, LV_OBJ_FLAG_HIDDEN);

            // Heading vector (20px)
            float rad = a.heading * GeoUtils::DEG_TO_RAD;
            blip.vec_pts[0] = {(lv_value_precise_t)pos.x, (lv_value_precise_t)pos.y};
            blip.vec_pts[1] = {
                (lv_value_precise_t)(pos.x + 20.0f * sinf(rad)),
                (lv_value_precise_t)(pos.y - 20.0f * cosf(rad))
            };
            lv_line_set_points(blip.vector, blip.vec_pts, 2);
            lv_obj_remove_flag(blip.vector, LV_OBJ_FLAG_HIDDEN);

            // Callsign label
            lv_label_set_text(blip.label, a.callsign.c_str());
            lv_obj_set_pos(blip.label, pos.x + 10, pos.y - 8);
            lv_obj_remove_flag(blip.label, LV_OBJ_FLAG_HIDDEN);

            // Selection ring (shown on selected blip)
            if (i == list_selected_idx_) {
                lv_obj_set_style_border_color(blip.dot, COLOR_TEXT_PRIMARY, 0);
                lv_obj_set_style_border_width(blip.dot, 2, 0);
                lv_obj_set_style_border_opa(blip.dot, 160, 0);
            } else {
                lv_obj_set_style_border_width(blip.dot, 0, 0);
            }

            // --- List row ---
            lv_obj_remove_flag(row.container, LV_OBJ_FLAG_HIDDEN);
            lv_obj_set_style_bg_color(row.accent_bar, blipColor, 0);
            lv_obj_set_style_text_color(row.label_callsign, blipColor, 0);
            lv_label_set_text(row.label_callsign, a.callsign.c_str());

            // Type · route
            char tr[64];
            if (!a.aircraftType.isEmpty() && !a.origin.isEmpty() && !a.destination.isEmpty()) {
                snprintf(tr, sizeof(tr), "%s  \xc2\xb7  %s \xe2\x86\x92 %s",
                         a.aircraftType.c_str(), a.origin.c_str(), a.destination.c_str());
            } else if (!a.origin.isEmpty() && !a.destination.isEmpty()) {
                snprintf(tr, sizeof(tr), "%s \xe2\x86\x92 %s",
                         a.origin.c_str(), a.destination.c_str());
            } else if (!a.aircraftType.isEmpty()) {
                snprintf(tr, sizeof(tr), "%s", a.aircraftType.c_str());
            } else {
                snprintf(tr, sizeof(tr), "Unknown type");
            }
            lv_label_set_text(row.label_type_route, tr);

            // Summary line (collapsed)
            char sm[80];
            float speedKt = a.velocity * 1.94384f;
            snprintf(sm, sizeof(sm), "%.0f ft  \xc2\xb7  %.0f kt  \xc2\xb7  %s  \xc2\xb7  %.1f nm",
                     altFt, speedKt, GeoUtils::cardinalDir(bearing), distNm);
            lv_label_set_text(row.label_summary, sm);

            // Expanded fields
            char buf[32];
            snprintf(buf, sizeof(buf), "%.0f ft", altFt);
            lv_label_set_text(row.label_alt, buf);

            snprintf(buf, sizeof(buf), "%.0f kt", speedKt);
            lv_label_set_text(row.label_speed, buf);

            snprintf(buf, sizeof(buf), "%.0f\xc2\xb0 %s",
                     a.heading, GeoUtils::cardinalDir(a.heading));
            lv_label_set_text(row.label_hdg, buf);

            snprintf(buf, sizeof(buf), "%.1f nm %s",
                     distNm, GeoUtils::cardinalDir(bearing));
            lv_label_set_text(row.label_dist, buf);

            // STATUS field
            float distToPwmNm = GeoUtils::distanceNm(Config::PWM_LAT, Config::PWM_LON,
                                                       a.latitude, a.longitude);
            float bearingToPwm = GeoUtils::bearingDeg(a.latitude, a.longitude,
                                                       Config::PWM_LAT, Config::PWM_LON);
            float hdgDiff = fabsf(fmodf(a.heading - bearingToPwm + 360.0f, 360.0f));
            if (hdgDiff > 180.0f) hdgDiff = 360.0f - hdgDiff;

            const char* statusTxt;
            if (altFt >= 5000.0f) {
                statusTxt = "CRUISING";
            } else if (distToPwmNm < 15.0f && hdgDiff < 30.0f) {
                statusTxt = "ON APPROACH";
            } else if (distToPwmNm < 15.0f) {
                statusTxt = "DEPARTING";
            } else {
                statusTxt = "LOW / OVERFLYING";
            }
            lv_obj_set_style_text_color(row.label_status, blipColor, 0);
            lv_label_set_text(row.label_status, statusTxt);

        } else {
            // Hide blip and row
            lv_obj_add_flag(blip.dot,    LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(blip.vector, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(blip.label,  LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(row.container, LV_OBJ_FLAG_HIDDEN);
        }
    }
}

// Main update function
void LVGLDisplayManager::update(const WeatherData& weather, const Aircraft* aircraft, int aircraftCount) {
    lv_lock();
    if (currentScreen == SCREEN_HOME) {
        update_home_screen(weather, aircraftCount);
    } else if (currentScreen == SCREEN_RADAR) {
        update_radar_screen(aircraft, aircraftCount);
    }
    lv_unlock();
}

// Tick is a no-op — the dedicated lvgl_task owns lv_timer_handler now.
void LVGLDisplayManager::tick(uint32_t /*period_ms*/) {}

// Screen management
void LVGLDisplayManager::setScreen(ScreenState screen) {
    lv_lock();
    if (currentScreen == screen) {
        lv_unlock();
        return;
    }
    switch (screen) {
        case SCREEN_HOME:
            currentScreen = screen;
            lastScreenChange = millis();
            if (screen_home) lv_screen_load_anim(screen_home, LV_SCR_LOAD_ANIM_FADE_IN, 200, 0, false);
            break;
        case SCREEN_RADAR:
            if (!screen_radar) { lv_unlock(); return; }  // not built yet — stay on current screen
            currentScreen = screen;
            lastScreenChange = millis();
            lv_screen_load_anim(screen_radar, LV_SCR_LOAD_ANIM_FADE_IN, 200, 0, false);
            break;
    }
    lv_unlock();
}

// Check if should return to home
bool LVGLDisplayManager::shouldReturnToHome() {
    if (currentScreen == SCREEN_HOME) return false;
    
    unsigned long idleTime = millis() - lastUserInteraction;
    return idleTime >= Config::UI_AUTO_HOME_MS;
}

bool LVGLDisplayManager::wasUserDismissed() {
    lv_lock();
    bool result = userDismissed_;
    userDismissed_ = false;
    lv_unlock();
    return result;
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
    lv_lock();
    currentBrightness = brightness;
    if (lcd) {
        lcd->setBrightness(brightness);
    }
    lv_unlock();
}

// Timestamp tracking
void LVGLDisplayManager::setLastUpdateTimestamp(time_t timestamp) {
    lv_lock();
    lastUpdateTime = timestamp;
    lv_unlock();
}

void LVGLDisplayManager::setStatusMessage(const String& msg) {
    lv_lock();
    statusMessage = msg;
    statusClearTime = millis() + Config::UI_STATUS_MS;
    if (homeWidgets.label_status_left) lv_label_set_text(homeWidgets.label_status_left, msg.c_str());
    lv_unlock();
}

// Event callbacks — registered in Task 6/8
void LVGLDisplayManager::event_list_row_clicked(lv_event_t* e) {
    int idx = (int)(intptr_t)lv_event_get_user_data(e);
    if (s_instance) s_instance->onListRowClicked(idx);
}

void LVGLDisplayManager::onListRowClicked(int idx) {
    if (idx < 0 || idx >= Config::MAX_AIRCRAFT) return;
    if (!list_rows_[idx].container) return;

    if (list_selected_idx_ == idx) {
        // Tap currently selected row → collapse
        lv_obj_add_flag(list_rows_[idx].expanded_panel, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_height(list_rows_[idx].container, 66);
        list_selected_idx_ = -1;
        if (radar_blips_[idx].dot)
            lv_obj_set_style_border_width(radar_blips_[idx].dot, 0, 0);
        return;
    }

    // Collapse previously selected
    if (list_selected_idx_ >= 0 && list_selected_idx_ < Config::MAX_AIRCRAFT) {
        int prev = list_selected_idx_;
        lv_obj_add_flag(list_rows_[prev].expanded_panel, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_height(list_rows_[prev].container, 66);
        if (radar_blips_[prev].dot)
            lv_obj_set_style_border_width(radar_blips_[prev].dot, 0, 0);
    }

    // Expand new row
    lv_obj_remove_flag(list_rows_[idx].expanded_panel, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_height(list_rows_[idx].container, 138);  // 66 header + 72 expanded
    list_selected_idx_ = idx;

    // Highlight corresponding radar blip
    if (radar_blips_[idx].dot) {
        lv_obj_set_style_border_color(radar_blips_[idx].dot, COLOR_TEXT_PRIMARY, 0);
        lv_obj_set_style_border_width(radar_blips_[idx].dot, 2, 0);
        lv_obj_set_style_border_opa(radar_blips_[idx].dot, 160, 0);
    }
}

void LVGLDisplayManager::event_topbar_back(lv_event_t* e) {
    (void)e;
    if (s_instance) s_instance->userDismissed_ = true;
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
