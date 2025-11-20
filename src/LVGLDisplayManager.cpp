// src/LVGLDisplayManager.cpp
// LVGL-based Display Manager Implementation
#include "LVGLDisplayManager.h"
#include "config/Config.h"
#include <lgfx/v1/platforms/esp32s3/Panel_RGB.hpp>
#include <lgfx/v1/platforms/esp32s3/Bus_RGB.hpp>
#include <driver/i2c.h>

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
            cfg.memory_width = 800; cfg.memory_height = 480;
            cfg.panel_width = 800; cfg.panel_height = 480;
            cfg.offset_x = 0; cfg.offset_y = 0;
            _panel_instance.config(cfg);
        }
        
        // Enable PSRAM
        {
            auto cfg = _panel_instance.config_detail();
            cfg.use_psram = 2;
            _panel_instance.config_detail(cfg);
        }
        
        // Configure RGB bus
        {
            auto cfg = _bus_instance.config();
            cfg.panel = &_panel_instance;
            cfg.pin_d0 = GPIO_NUM_8; cfg.pin_d1 = GPIO_NUM_3; cfg.pin_d2 = GPIO_NUM_46;
            cfg.pin_d3 = GPIO_NUM_9; cfg.pin_d4 = GPIO_NUM_1; cfg.pin_d5 = GPIO_NUM_5;
            cfg.pin_d6 = GPIO_NUM_6; cfg.pin_d7 = GPIO_NUM_7; cfg.pin_d8 = GPIO_NUM_15;
            cfg.pin_d9 = GPIO_NUM_16; cfg.pin_d10 = GPIO_NUM_4; cfg.pin_d11 = GPIO_NUM_45;
            cfg.pin_d12 = GPIO_NUM_48; cfg.pin_d13 = GPIO_NUM_47; cfg.pin_d14 = GPIO_NUM_21;
            cfg.pin_d15 = GPIO_NUM_14;
            cfg.pin_henable = GPIO_NUM_40; cfg.pin_vsync = GPIO_NUM_41;
            cfg.pin_hsync = GPIO_NUM_39; cfg.pin_pclk = GPIO_NUM_0;
            cfg.freq_write = 15000000;
            cfg.hsync_polarity = 0; cfg.hsync_front_porch = 8;
            cfg.hsync_pulse_width = 4; cfg.hsync_back_porch = 43;
            cfg.vsync_polarity = 0; cfg.vsync_front_porch = 8;
            cfg.vsync_pulse_width = 4; cfg.vsync_back_porch = 12;
            cfg.pclk_active_neg = 1; cfg.de_idle_high = 0; cfg.pclk_idle_high = 0;
            _bus_instance.config(cfg);
        }
        _panel_instance.setBus(&_bus_instance);
        
        // Configure backlight
        {
            auto cfg = _light_instance.config();
            cfg.pin_bl = GPIO_NUM_2;
            _light_instance.config(cfg);
        }
        _panel_instance.light(&_light_instance);
        
        // Configure touch (GT911)
        {
            auto cfg = _touch_instance.config();
            cfg.x_min = 0; cfg.x_max = 800; cfg.y_min = 0; cfg.y_max = 480;
            cfg.i2c_addr = 0x14; cfg.pin_sda = GPIO_NUM_19; cfg.pin_scl = GPIO_NUM_20;
            cfg.pin_int = GPIO_NUM_NC; cfg.pin_rst = GPIO_NUM_NC;
            cfg.i2c_port = 1; cfg.freq = 400000; cfg.bus_shared = false;
            _touch_instance.config(cfg);
            _panel_instance.setTouch(&_touch_instance);
        }
        setPanel(&_panel_instance);
    }
};

// Define buffer sizes for LVGL
#define LVGL_BUFFER_SIZE (800 * 40)  // 40 lines buffer

// Static instance for callbacks
static LVGLDisplayManager* s_instance = nullptr;

// Color definitions (LVGL uses 0xRRGGBB format for true colors)
// "Modern Day" Theme - Clean, Professional, Hides Backlight Bleed
#define COLOR_BG_TOP     lv_color_hex(0xf1f5f9)  // Slate 100 (Light Grey)
#define COLOR_BG_BOTTOM  lv_color_hex(0xe2e8f0)  // Slate 200 (Slightly darker for gradient)
#define COLOR_CARD       lv_color_hex(0xffffff)  // Pure White Cards
#define COLOR_CARD_BORDER lv_color_hex(0xcbd5e1) // Slate 300 (Subtle border)
#define COLOR_ACCENT     lv_color_hex(0x0284c7)  // Sky 600 (Deep Blue)
#define COLOR_TEXT_PRIMARY lv_color_hex(0x0f172a) // Slate 900 (Almost Black)
#define COLOR_TEXT_SECONDARY lv_color_hex(0x64748b) // Slate 500 (Grey text)
#define COLOR_SUCCESS    lv_color_hex(0x22c55e)  // Green 500
#define COLOR_WARNING    lv_color_hex(0xf59e0b)  // Amber 500

// Constructor
LVGLDisplayManager::LVGLDisplayManager()
    : lcd(nullptr)
    , lv_display(nullptr)
    , lv_indev(nullptr)
    , screen_home(nullptr)
    , screen_aircraft(nullptr)
    , screen_no_aircraft(nullptr)
    , label_time(nullptr)
    , label_date(nullptr)
    , label_temperature(nullptr)
    , label_weather_desc(nullptr)
    , label_feels_like(nullptr)   // New
    , label_temp_range(nullptr)   // New
    , label_humidity(nullptr)
    , label_wind(nullptr)
    , arc_humidity(nullptr)
    , label_sunrise(nullptr)
    , label_sunset(nullptr)
    , label_plane_count(nullptr)
    , btn_view_planes(nullptr)
    , label_callsign(nullptr)
    , label_aircraft_type(nullptr)
    , label_airline(nullptr)      // New
    , label_route(nullptr)        // New
    , label_altitude(nullptr)
    , label_velocity(nullptr)
    , label_heading(nullptr)
    , label_latitude(nullptr)
    , label_longitude(nullptr)
    , btn_back_home(nullptr)
    , currentScreen(SCREEN_HOME)
    , lastScreenChange(0)
    , lastUserInteraction(0)
    , statusMessage("")
    , lastUpdateTime(0)
    , currentBrightness(255)
{
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
    lcd->setRotation(2);
    lcd->setBrightness(currentBrightness);
    lcd->fillScreen(TFT_BLACK);
    
    // Initialize LVGL
    lv_init();
    
    // Create display buffers
    static lv_color_t* buf1 = (lv_color_t*)heap_caps_malloc(LVGL_BUFFER_SIZE * sizeof(lv_color_t), MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
    static lv_color_t* buf2 = (lv_color_t*)heap_caps_malloc(LVGL_BUFFER_SIZE * sizeof(lv_color_t), MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
    
    if (!buf1 || !buf2) {
        Serial.println("[LVGL] Failed to allocate display buffers");
        return false;
    }
    
    // Create LVGL display
    lv_display = lv_display_create(800, 480);
    lv_display_set_flush_cb(lv_display, flush_cb);
    lv_display_set_buffers(lv_display, buf1, buf2, LVGL_BUFFER_SIZE * sizeof(lv_color_t), LV_DISPLAY_RENDER_MODE_PARTIAL);
    
    // Create input device (touch)
    lv_indev = lv_indev_create();
    lv_indev_set_type(lv_indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(lv_indev, touchpad_read);
    
    // Build screens
    build_home_screen();
    build_aircraft_screen();
    build_no_aircraft_screen();
    
    // Load home screen
    lv_screen_load(screen_home);
    currentScreen = SCREEN_HOME;
    lastUserInteraction = millis();
    
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

// Build home screen with modern card-based layout
void LVGLDisplayManager::build_home_screen() {
    screen_home = lv_obj_create(NULL);
    // Gradient Background (Hides LCD backlight bleed)
    lv_obj_set_style_bg_color(screen_home, COLOR_BG_TOP, 0);
    lv_obj_set_style_bg_grad_color(screen_home, COLOR_BG_BOTTOM, 0);
    lv_obj_set_style_bg_grad_dir(screen_home, LV_GRAD_DIR_VER, 0);
    
    // Top bar with time/date
    lv_obj_t* top_bar = lv_obj_create(screen_home);
    lv_obj_set_size(top_bar, 800, 80);
    lv_obj_align(top_bar, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_opa(top_bar, LV_OPA_TRANSP, 0); // Transparent background
    lv_obj_set_style_border_width(top_bar, 0, 0);
    lv_obj_set_style_radius(top_bar, 0, 0);
    
    label_time = lv_label_create(top_bar);
    lv_obj_set_style_text_font(label_time, &lv_font_montserrat_32, 0);
    lv_obj_set_style_text_color(label_time, COLOR_TEXT_PRIMARY, 0);
    lv_label_set_text(label_time, "00:00");
    lv_obj_align(label_time, LV_ALIGN_LEFT_MID, 20, -5);
    
    label_date = lv_label_create(top_bar);
    lv_obj_set_style_text_font(label_date, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(label_date, COLOR_TEXT_SECONDARY, 0);
    lv_label_set_text(label_date, "Mon, Jan 1");
    lv_obj_align(label_date, LV_ALIGN_LEFT_MID, 20, 20);
    
    // Weather card (left side)
    lv_obj_t* weather_card = lv_obj_create(screen_home);
    lv_obj_set_size(weather_card, 480, 320);
    lv_obj_align(weather_card, LV_ALIGN_TOP_LEFT, 20, 100);
    lv_obj_set_style_bg_color(weather_card, COLOR_CARD, 0);
    lv_obj_set_style_border_color(weather_card, COLOR_CARD_BORDER, 0); // Add border
    lv_obj_set_style_border_width(weather_card, 2, 0);                 // 2px border
    lv_obj_set_style_shadow_width(weather_card, 20, 0);                // Soft shadow
    lv_obj_set_style_shadow_color(weather_card, lv_color_black(), 0);
    lv_obj_set_style_shadow_opa(weather_card, LV_OPA_30, 0);
    lv_obj_set_style_radius(weather_card, 16, 0);
    lv_obj_set_style_pad_all(weather_card, 20, 0);
    
    // --- Left Column: Current Weather ---
    
    // Temperature (large)
    label_temperature = lv_label_create(weather_card);
    lv_obj_set_style_text_font(label_temperature, &lv_font_montserrat_48, 0);
    lv_obj_set_style_text_color(label_temperature, COLOR_TEXT_PRIMARY, 0);
    lv_label_set_text(label_temperature, "--°F");
    lv_obj_align(label_temperature, LV_ALIGN_TOP_LEFT, 0, 0);
    
    // Weather description
    label_weather_desc = lv_label_create(weather_card);
    lv_obj_set_style_text_font(label_weather_desc, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(label_weather_desc, COLOR_TEXT_SECONDARY, 0);
    lv_label_set_text(label_weather_desc, "Loading...");
    lv_obj_align(label_weather_desc, LV_ALIGN_TOP_LEFT, 0, 60);

    // Feels Like
    label_feels_like = lv_label_create(weather_card);
    lv_obj_set_style_text_font(label_feels_like, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(label_feels_like, COLOR_TEXT_SECONDARY, 0);
    lv_label_set_text(label_feels_like, "Feels: --°F");
    lv_obj_align(label_feels_like, LV_ALIGN_TOP_LEFT, 0, 85);

    // Temp Range
    label_temp_range = lv_label_create(weather_card);
    lv_obj_set_style_text_font(label_temp_range, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(label_temp_range, COLOR_TEXT_SECONDARY, 0);
    lv_label_set_text(label_temp_range, "H: --° L: --°");
    lv_obj_align(label_temp_range, LV_ALIGN_TOP_LEFT, 0, 105);
    
    // Wind
    label_wind = lv_label_create(weather_card);
    lv_obj_set_style_text_font(label_wind, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(label_wind, COLOR_TEXT_SECONDARY, 0);
    lv_label_set_text(label_wind, "Wind: -- mph");
    lv_obj_align(label_wind, LV_ALIGN_TOP_LEFT, 0, 135);
    
    // Sunrise
    label_sunrise = lv_label_create(weather_card);
    lv_obj_set_style_text_font(label_sunrise, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(label_sunrise, COLOR_TEXT_SECONDARY, 0);
    lv_label_set_text(label_sunrise, LV_SYMBOL_UP " --:--");
    lv_obj_align(label_sunrise, LV_ALIGN_TOP_LEFT, 0, 155);
    
    // Sunset
    label_sunset = lv_label_create(weather_card);
    lv_obj_set_style_text_font(label_sunset, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(label_sunset, COLOR_TEXT_SECONDARY, 0);
    lv_label_set_text(label_sunset, LV_SYMBOL_DOWN " --:--");
    lv_obj_align(label_sunset, LV_ALIGN_TOP_LEFT, 0, 175);

    // Humidity arc (circular gauge) - Bottom Left
    arc_humidity = lv_arc_create(weather_card);
    lv_obj_set_size(arc_humidity, 80, 80); // Smaller arc
    lv_arc_set_range(arc_humidity, 0, 100);
    lv_arc_set_value(arc_humidity, 0);
    lv_obj_set_style_arc_color(arc_humidity, COLOR_ACCENT, LV_PART_INDICATOR);
    lv_obj_set_style_arc_width(arc_humidity, 8, LV_PART_INDICATOR);
    lv_obj_set_style_arc_width(arc_humidity, 8, LV_PART_MAIN);
    lv_obj_align(arc_humidity, LV_ALIGN_BOTTOM_LEFT, 10, 0);
    lv_obj_remove_flag(arc_humidity, LV_OBJ_FLAG_CLICKABLE);
    
    label_humidity = lv_label_create(arc_humidity);
    lv_obj_set_style_text_font(label_humidity, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(label_humidity, COLOR_TEXT_PRIMARY, 0);
    lv_label_set_text(label_humidity, "--%");
    lv_obj_center(label_humidity);
    
    // --- Right Column: 5-Day Forecast ---
    int forecast_x = 200;
    int row_height = 45;
    int start_y = 10;

    lv_obj_t* forecast_title = lv_label_create(weather_card);
    lv_obj_set_style_text_font(forecast_title, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(forecast_title, COLOR_TEXT_SECONDARY, 0);
    lv_label_set_text(forecast_title, "5-Day Forecast");
    lv_obj_align(forecast_title, LV_ALIGN_TOP_LEFT, forecast_x, -5);
    
    // Create 5 rows
    for(int i=0; i<5; i++) {
        int y_pos = start_y + 25 + (i * row_height);
        
        // Container for alignment
        forecast_rows[i].container = lv_obj_create(weather_card);
        lv_obj_set_size(forecast_rows[i].container, 240, row_height);
        lv_obj_align(forecast_rows[i].container, LV_ALIGN_TOP_LEFT, forecast_x, y_pos);
        lv_obj_set_style_bg_opa(forecast_rows[i].container, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(forecast_rows[i].container, 0, 0);
        lv_obj_set_style_pad_all(forecast_rows[i].container, 0, 0);
        
        // Day Name (Mon, Tue...)
        forecast_rows[i].label_day = lv_label_create(forecast_rows[i].container);
        lv_obj_set_style_text_font(forecast_rows[i].label_day, &lv_font_montserrat_16, 0);
        lv_obj_set_style_text_color(forecast_rows[i].label_day, COLOR_TEXT_PRIMARY, 0);
        lv_label_set_text(forecast_rows[i].label_day, "---");
        lv_obj_align(forecast_rows[i].label_day, LV_ALIGN_LEFT_MID, 0, 0);
        
        // Condition (Rain, Clear...)
        forecast_rows[i].label_condition = lv_label_create(forecast_rows[i].container);
        lv_obj_set_style_text_font(forecast_rows[i].label_condition, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(forecast_rows[i].label_condition, COLOR_TEXT_SECONDARY, 0);
        lv_label_set_text(forecast_rows[i].label_condition, "");
        lv_obj_align(forecast_rows[i].label_condition, LV_ALIGN_CENTER, -10, 0);
        
        // Temp (H/L)
        forecast_rows[i].label_temp = lv_label_create(forecast_rows[i].container);
        lv_obj_set_style_text_font(forecast_rows[i].label_temp, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(forecast_rows[i].label_temp, COLOR_TEXT_PRIMARY, 0);
        lv_label_set_text(forecast_rows[i].label_temp, "--/--");
        lv_obj_align(forecast_rows[i].label_temp, LV_ALIGN_RIGHT_MID, 0, 0);
    }
    
    // Aircraft card (right side)
    lv_obj_t* aircraft_card = lv_obj_create(screen_home);
    lv_obj_set_size(aircraft_card, 260, 320);
    lv_obj_align(aircraft_card, LV_ALIGN_TOP_RIGHT, -20, 100);
    lv_obj_set_style_bg_color(aircraft_card, COLOR_CARD, 0);
    lv_obj_set_style_border_color(aircraft_card, COLOR_CARD_BORDER, 0); // Add border
    lv_obj_set_style_border_width(aircraft_card, 2, 0);
    lv_obj_set_style_shadow_width(aircraft_card, 20, 0);
    lv_obj_set_style_shadow_color(aircraft_card, lv_color_black(), 0);
    lv_obj_set_style_shadow_opa(aircraft_card, LV_OPA_30, 0);
    lv_obj_set_style_radius(aircraft_card, 16, 0);
    lv_obj_set_style_pad_all(aircraft_card, 20, 0);
    
    // Aircraft icon (using LVGL symbol)
    lv_obj_t* plane_icon = lv_label_create(aircraft_card);
    lv_obj_set_style_text_font(plane_icon, &lv_font_montserrat_48, 0);
    lv_obj_set_style_text_color(plane_icon, COLOR_ACCENT, 0);
    lv_label_set_text(plane_icon, LV_SYMBOL_GPS);
    lv_obj_align(plane_icon, LV_ALIGN_TOP_MID, 0, 20);
    
    // Plane count
    label_plane_count = lv_label_create(aircraft_card);
    lv_obj_set_style_text_font(label_plane_count, &lv_font_montserrat_32, 0);
    lv_obj_set_style_text_color(label_plane_count, COLOR_TEXT_PRIMARY, 0);
    lv_label_set_text(label_plane_count, "0");
    lv_obj_align(label_plane_count, LV_ALIGN_CENTER, 0, 0);
    
    lv_obj_t* label_planes_text = lv_label_create(aircraft_card);
    lv_obj_set_style_text_font(label_planes_text, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(label_planes_text, COLOR_TEXT_SECONDARY, 0);
    lv_label_set_text(label_planes_text, "Aircraft Nearby");
    lv_obj_align(label_planes_text, LV_ALIGN_CENTER, 0, 35);
    
    // View planes button
    btn_view_planes = lv_button_create(aircraft_card);
    lv_obj_set_size(btn_view_planes, 200, 50);
    lv_obj_align(btn_view_planes, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_color(btn_view_planes, COLOR_ACCENT, 0);
    lv_obj_set_style_radius(btn_view_planes, 12, 0);
    lv_obj_add_event_cb(btn_view_planes, event_btn_view_planes, LV_EVENT_CLICKED, this);
    
    lv_obj_t* btn_label = lv_label_create(btn_view_planes);
    lv_obj_set_style_text_color(btn_label, lv_color_hex(0xffffff), 0); // White text on blue button
    lv_label_set_text(btn_label, "View Aircraft");
    lv_obj_center(btn_label);
}

// Build aircraft detail screen
void LVGLDisplayManager::build_aircraft_screen() {
    screen_aircraft = lv_obj_create(NULL);
    // Gradient Background
    lv_obj_set_style_bg_color(screen_aircraft, COLOR_BG_TOP, 0);
    lv_obj_set_style_bg_grad_color(screen_aircraft, COLOR_BG_BOTTOM, 0);
    lv_obj_set_style_bg_grad_dir(screen_aircraft, LV_GRAD_DIR_VER, 0);
    
    // Back button
    btn_back_home = lv_button_create(screen_aircraft);
    lv_obj_set_size(btn_back_home, 120, 50);
    lv_obj_align(btn_back_home, LV_ALIGN_TOP_LEFT, 20, 20);
    lv_obj_set_style_bg_color(btn_back_home, COLOR_CARD, 0);
    lv_obj_set_style_border_color(btn_back_home, COLOR_CARD_BORDER, 0);
    lv_obj_set_style_border_width(btn_back_home, 2, 0);
    lv_obj_add_event_cb(btn_back_home, event_btn_back_home, LV_EVENT_CLICKED, this);
    
    lv_obj_t* back_label = lv_label_create(btn_back_home);
    lv_label_set_text(back_label, LV_SYMBOL_LEFT " Back");
    lv_obj_center(back_label);
    
    // Aircraft info card
    lv_obj_t* aircraft_card = lv_obj_create(screen_aircraft);
    lv_obj_set_size(aircraft_card, 760, 370);
    lv_obj_align(aircraft_card, LV_ALIGN_CENTER, 0, 10);
    lv_obj_set_style_bg_color(aircraft_card, COLOR_CARD, 0);
    lv_obj_set_style_border_color(aircraft_card, COLOR_CARD_BORDER, 0);
    lv_obj_set_style_border_width(aircraft_card, 2, 0);
    lv_obj_set_style_shadow_width(aircraft_card, 20, 0);
    lv_obj_set_style_shadow_color(aircraft_card, lv_color_black(), 0);
    lv_obj_set_style_shadow_opa(aircraft_card, LV_OPA_30, 0);
    lv_obj_set_style_radius(aircraft_card, 16, 0);
    lv_obj_set_style_pad_all(aircraft_card, 30, 0);
    
    // Callsign (large)
    label_callsign = lv_label_create(aircraft_card);
    lv_obj_set_style_text_font(label_callsign, &lv_font_montserrat_48, 0);
    lv_obj_set_style_text_color(label_callsign, COLOR_ACCENT, 0);
    lv_label_set_text(label_callsign, "---");
    lv_obj_align(label_callsign, LV_ALIGN_TOP_LEFT, 0, 0);
    
    // Aircraft type
    label_aircraft_type = lv_label_create(aircraft_card);
    lv_obj_set_style_text_font(label_aircraft_type, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(label_aircraft_type, COLOR_TEXT_SECONDARY, 0);
    lv_label_set_text(label_aircraft_type, "Aircraft Type");
    lv_obj_align(label_aircraft_type, LV_ALIGN_TOP_LEFT, 0, 70);

    // Airline
    label_airline = lv_label_create(aircraft_card);
    lv_obj_set_style_text_font(label_airline, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(label_airline, COLOR_TEXT_PRIMARY, 0);
    lv_label_set_text(label_airline, "Airline Name");
    lv_obj_align(label_airline, LV_ALIGN_TOP_LEFT, 0, 95);

    // Route
    label_route = lv_label_create(aircraft_card);
    lv_obj_set_style_text_font(label_route, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(label_route, COLOR_TEXT_SECONDARY, 0);
    lv_label_set_text(label_route, "Origin -> Destination");
    lv_obj_align(label_route, LV_ALIGN_TOP_LEFT, 0, 120);
    
    // Grid for metrics
    int y_offset = 160;
    int spacing = 50;
    
    label_altitude = lv_label_create(aircraft_card);
    lv_obj_set_style_text_font(label_altitude, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(label_altitude, COLOR_TEXT_PRIMARY, 0);
    lv_label_set_text(label_altitude, "Altitude: --- ft");
    lv_obj_align(label_altitude, LV_ALIGN_TOP_LEFT, 0, y_offset);
    
    label_velocity = lv_label_create(aircraft_card);
    lv_obj_set_style_text_font(label_velocity, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(label_velocity, COLOR_TEXT_PRIMARY, 0);
    lv_label_set_text(label_velocity, "Speed: --- kts");
    lv_obj_align(label_velocity, LV_ALIGN_TOP_LEFT, 0, y_offset + spacing);
    
    label_heading = lv_label_create(aircraft_card);
    lv_obj_set_style_text_font(label_heading, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(label_heading, COLOR_TEXT_PRIMARY, 0);
    lv_label_set_text(label_heading, "Heading: ---°");
    lv_obj_align(label_heading, LV_ALIGN_TOP_LEFT, 0, y_offset + spacing * 2);
    
    label_latitude = lv_label_create(aircraft_card);
    lv_obj_set_style_text_font(label_latitude, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(label_latitude, COLOR_TEXT_SECONDARY, 0);
    lv_label_set_text(label_latitude, "Lat: ---");
    lv_obj_align(label_latitude, LV_ALIGN_BOTTOM_LEFT, 0, -20);
    
    label_longitude = lv_label_create(aircraft_card);
    lv_obj_set_style_text_font(label_longitude, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(label_longitude, COLOR_TEXT_SECONDARY, 0);
    lv_label_set_text(label_longitude, "Lon: ---");
    lv_obj_align(label_longitude, LV_ALIGN_BOTTOM_LEFT, 250, -20);
}

// Build no aircraft screen
void LVGLDisplayManager::build_no_aircraft_screen() {
    screen_no_aircraft = lv_obj_create(NULL);
    // Gradient Background
    lv_obj_set_style_bg_color(screen_no_aircraft, COLOR_BG_TOP, 0);
    lv_obj_set_style_bg_grad_color(screen_no_aircraft, COLOR_BG_BOTTOM, 0);
    lv_obj_set_style_bg_grad_dir(screen_no_aircraft, LV_GRAD_DIR_VER, 0);
    
    // Back button
    lv_obj_t* btn_back = lv_button_create(screen_no_aircraft);
    lv_obj_set_size(btn_back, 120, 50);
    lv_obj_align(btn_back, LV_ALIGN_TOP_LEFT, 20, 20);
    lv_obj_set_style_bg_color(btn_back, COLOR_CARD, 0);
    lv_obj_set_style_border_color(btn_back, COLOR_CARD_BORDER, 0);
    lv_obj_set_style_border_width(btn_back, 2, 0);
    lv_obj_add_event_cb(btn_back, event_btn_back_home, LV_EVENT_CLICKED, this);
    
    lv_obj_t* back_label = lv_label_create(btn_back);
    lv_label_set_text(back_label, LV_SYMBOL_LEFT " Back");
    lv_obj_center(back_label);
    
    // Message
    lv_obj_t* msg_label = lv_label_create(screen_no_aircraft);
    lv_obj_set_style_text_font(msg_label, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(msg_label, COLOR_TEXT_SECONDARY, 0);
    lv_label_set_text(msg_label, "No Aircraft Detected");
    lv_obj_center(msg_label);
}

// Update home screen
void LVGLDisplayManager::update_home_screen(const WeatherData& weather, int aircraftCount) {
    if (!screen_home) return;
    
    // Update time/date
    update_clock();
    
    // Update temperature
    char temp_buf[32];
    snprintf(temp_buf, sizeof(temp_buf), "%.0f°F", weather.temperature);
    lv_label_set_text(label_temperature, temp_buf);
    
    // Update weather description
    lv_label_set_text(label_weather_desc, weather.description.c_str());
    
    // Update Feels Like
    char feels_buf[32];
    snprintf(feels_buf, sizeof(feels_buf), "Feels: %.0f°F", weather.feelsLike);
    lv_label_set_text(label_feels_like, feels_buf);

    // Update Temp Range
    char range_buf[32];
    snprintf(range_buf, sizeof(range_buf), "H: %.0f° L: %.0f°", weather.tempMax, weather.tempMin);
    lv_label_set_text(label_temp_range, range_buf);
    
    // Update humidity arc
    lv_arc_set_value(arc_humidity, (int)weather.humidity);
    char hum_buf[16];
    snprintf(hum_buf, sizeof(hum_buf), "%.0f%%", weather.humidity);
    lv_label_set_text(label_humidity, hum_buf);
    
    // Update wind
    char wind_buf[32];
    snprintf(wind_buf, sizeof(wind_buf), "Wind: %.0f mph", weather.windSpeed);
    lv_label_set_text(label_wind, wind_buf);
    
    // Update sunrise/sunset
    char sunrise_buf[32];
    snprintf(sunrise_buf, sizeof(sunrise_buf), LV_SYMBOL_UP " %s", formatTime(weather.sunrise).c_str());
    lv_label_set_text(label_sunrise, sunrise_buf);
    
    char sunset_buf[32];
    snprintf(sunset_buf, sizeof(sunset_buf), LV_SYMBOL_DOWN " %s", formatTime(weather.sunset).c_str());
    lv_label_set_text(label_sunset, sunset_buf);
    
    // Update Forecast
    for(int i=0; i<5; i++) {
        if (i < weather.forecast.size()) {
            const auto& day = weather.forecast[i];
            lv_label_set_text(forecast_rows[i].label_day, day.dayName.c_str());
            lv_label_set_text(forecast_rows[i].label_condition, day.condition.c_str());
            
            char f_temp_buf[32];
            snprintf(f_temp_buf, sizeof(f_temp_buf), "%.0f/%.0f", day.tempMax, day.tempMin);
            lv_label_set_text(forecast_rows[i].label_temp, f_temp_buf);
        } else {
            lv_label_set_text(forecast_rows[i].label_day, "-");
            lv_label_set_text(forecast_rows[i].label_condition, "");
            lv_label_set_text(forecast_rows[i].label_temp, "--/--");
        }
    }
    
    // Update aircraft count
    char count_buf[16];
    snprintf(count_buf, sizeof(count_buf), "%d", aircraftCount);
    lv_label_set_text(label_plane_count, count_buf);
    
    // Enable/disable view button based on count
    if (aircraftCount > 0) {
        lv_obj_remove_state(btn_view_planes, LV_STATE_DISABLED);
    } else {
        lv_obj_add_state(btn_view_planes, LV_STATE_DISABLED);
    }
}

// Update aircraft screen
void LVGLDisplayManager::update_aircraft_screen(const Aircraft& aircraft) {
    if (!screen_aircraft) return;
    
    // Update callsign
    lv_label_set_text(label_callsign, aircraft.callsign.c_str());
    
    // Update aircraft type
    lv_label_set_text(label_aircraft_type, aircraft.aircraftType.c_str());

    // Update airline
    lv_label_set_text(label_airline, aircraft.airline.length() > 0 ? aircraft.airline.c_str() : "Unknown Airline");

    // Update route
    if (aircraft.origin.length() > 0 && aircraft.destination.length() > 0) {
        char route_buf[64];
        snprintf(route_buf, sizeof(route_buf), "%s -> %s", aircraft.origin.c_str(), aircraft.destination.c_str());
        lv_label_set_text(label_route, route_buf);
    } else {
        lv_label_set_text(label_route, "Route Unknown");
    }
    
    // Update altitude (m to ft)
    char alt_buf[64];
    snprintf(alt_buf, sizeof(alt_buf), "Altitude: %.0f ft", aircraft.altitude * 3.28084f);
    lv_label_set_text(label_altitude, alt_buf);
    
    // Update velocity (m/s to kts)
    char vel_buf[64];
    snprintf(vel_buf, sizeof(vel_buf), "Speed: %.0f kts", aircraft.velocity * 1.94384f);
    lv_label_set_text(label_velocity, vel_buf);
    
    // Update heading
    char head_buf[64];
    snprintf(head_buf, sizeof(head_buf), "Heading: %.0f°", aircraft.heading);
    lv_label_set_text(label_heading, head_buf);
    
    // Update coordinates
    char lat_buf[32];
    snprintf(lat_buf, sizeof(lat_buf), "Lat: %.4f", aircraft.latitude);
    lv_label_set_text(label_latitude, lat_buf);
    
    char lon_buf[32];
    snprintf(lon_buf, sizeof(lon_buf), "Lon: %.4f", aircraft.longitude);
    lv_label_set_text(label_longitude, lon_buf);
}

// Update clock
void LVGLDisplayManager::update_clock() {
    time_t now = time(nullptr);
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo)) return;
    
    char time_buf[16];
    strftime(time_buf, sizeof(time_buf), "%H:%M", &timeinfo);
    lv_label_set_text(label_time, time_buf);
    
    char date_buf[32];
    strftime(date_buf, sizeof(date_buf), "%a, %b %d", &timeinfo);
    lv_label_set_text(label_date, date_buf);
}

// Main update function
void LVGLDisplayManager::update(const WeatherData& weather, const Aircraft* aircraft, int aircraftCount) {
    if (currentScreen == SCREEN_HOME) {
        update_home_screen(weather, aircraftCount);
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
    if (currentScreen == screen) return;
    
    currentScreen = screen;
    lastScreenChange = millis();
    
    switch (screen) {
        case SCREEN_HOME:
            lv_screen_load(screen_home);
            break;
        case SCREEN_AIRCRAFT_DETAIL:
            lv_screen_load(screen_aircraft);
            break;
        case SCREEN_NO_AIRCRAFT:
            lv_screen_load(screen_no_aircraft);
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

// Status message (for compatibility - not displayed in LVGL UI yet)
void LVGLDisplayManager::setStatusMessage(const String& msg) {
    statusMessage = msg;
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
