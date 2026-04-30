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
// Touch + backlight only — the RGB display is driven by IDF esp_lcd directly.
// Panel_Device has no display bus: init() only initialises touch and PWM backlight.
class LGFX_Panel : public lgfx::LGFX_Device {
public:
    lgfx::Panel_NULL    _panel_instance;
    lgfx::Light_PWM     _light_instance;
    lgfx::Touch_GT911   _touch_instance;

    LGFX_Panel() {
        {
            auto cfg = _panel_instance.config();
            cfg.memory_width  = hal::Elecrow5Inch::PANEL_WIDTH;
            cfg.memory_height = hal::Elecrow5Inch::PANEL_HEIGHT;
            cfg.panel_width   = hal::Elecrow5Inch::PANEL_WIDTH;
            cfg.panel_height  = hal::Elecrow5Inch::PANEL_HEIGHT;
            cfg.offset_x = 0; cfg.offset_y = 0;
            _panel_instance.config(cfg);
        }
        {
            auto cfg = _light_instance.config();
            cfg.pin_bl = hal::Elecrow5Inch::PIN_BACKLIGHT;
            _light_instance.config(cfg);
        }
        _panel_instance.light(&_light_instance);
        {
            auto cfg = _touch_instance.config();
            cfg.x_min      = 0;
            cfg.x_max      = hal::Elecrow5Inch::PANEL_WIDTH;
            cfg.y_min      = 0;
            cfg.y_max      = hal::Elecrow5Inch::PANEL_HEIGHT;
            cfg.i2c_addr   = hal::Elecrow5Inch::TOUCH_I2C_ADDR;
            cfg.pin_sda    = hal::Elecrow5Inch::TOUCH_PIN_SDA;
            cfg.pin_scl    = hal::Elecrow5Inch::TOUCH_PIN_SCL;
            cfg.pin_int    = hal::Elecrow5Inch::TOUCH_PIN_INT;
            cfg.pin_rst    = hal::Elecrow5Inch::TOUCH_PIN_RST;
            cfg.i2c_port   = hal::Elecrow5Inch::TOUCH_I2C_PORT;
            cfg.freq       = hal::Elecrow5Inch::TOUCH_I2C_FREQ;
            cfg.bus_shared = false;
            _touch_instance.config(cfg);
            _panel_instance.setTouch(&_touch_instance);
        }
        setPanel(&_panel_instance);
    }
};

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
    , statusMessage("")
    , statusClearTime(0)
    , currentBrightness(255)
    , userDismissed_(false)
    , userRequestedRadar_(false)
    , panel_handle_(nullptr)
    , sem_vsync_end_(nullptr)
    , fb1_(nullptr)
    , fb2_(nullptr)
{
    s_instance = this;
}

// Destructor
LVGLDisplayManager::~LVGLDisplayManager() {
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
    if (panel_handle_) {
        esp_lcd_panel_del(panel_handle_);
        panel_handle_ = nullptr;
    }
    if (sem_vsync_end_) {
        vSemaphoreDelete(sem_vsync_end_);
        sem_vsync_end_ = nullptr;
    }
    delete lcd;
    lcd = nullptr;
    s_instance = nullptr;
}

// Phase 1: hardware + LVGL core + FreeRTOS task.
// Call this BEFORE WiFi init so the task stack is allocated while internal SRAM is
// still available (WiFi DMA buffers also need internal SRAM).
bool LVGLDisplayManager::initHardware() {
    Serial.println("[LVGL] Initializing hardware (IDF double-buffer)...");

    sem_vsync_end_ = xSemaphoreCreateBinary();
    if (!sem_vsync_end_) {
        Serial.println("[LVGL] Failed to create vsync semaphore");
        return false;
    }

    // --- IDF RGB panel: two PSRAM framebuffers, VSYNC-synced swap ---
    esp_lcd_rgb_panel_config_t panel_config = {};
    panel_config.clk_src                         = LCD_CLK_SRC_DEFAULT;
    panel_config.timings.pclk_hz                 = hal::Elecrow5Inch::RGB_FREQ_WRITE;
    panel_config.timings.h_res                   = hal::Elecrow5Inch::PANEL_WIDTH;
    panel_config.timings.v_res                   = hal::Elecrow5Inch::PANEL_HEIGHT;
    panel_config.timings.hsync_pulse_width       = hal::Elecrow5Inch::HSYNC_PULSE_WIDTH;
    panel_config.timings.hsync_back_porch        = hal::Elecrow5Inch::HSYNC_BACK_PORCH;
    panel_config.timings.hsync_front_porch       = hal::Elecrow5Inch::HSYNC_FRONT_PORCH;
    panel_config.timings.vsync_pulse_width       = hal::Elecrow5Inch::VSYNC_PULSE_WIDTH;
    panel_config.timings.vsync_back_porch        = hal::Elecrow5Inch::VSYNC_BACK_PORCH;
    panel_config.timings.vsync_front_porch       = hal::Elecrow5Inch::VSYNC_FRONT_PORCH;
    // Polarity: HAL 0 = active LOW → IDF "idle high" → idle_low = 0
    panel_config.timings.flags.hsync_idle_low    = hal::Elecrow5Inch::HSYNC_POLARITY;
    panel_config.timings.flags.vsync_idle_low    = hal::Elecrow5Inch::VSYNC_POLARITY;
    panel_config.timings.flags.de_idle_high      = hal::Elecrow5Inch::DE_IDLE_HIGH;
    panel_config.timings.flags.pclk_active_neg   = hal::Elecrow5Inch::PCLK_ACTIVE_NEG;
    panel_config.timings.flags.pclk_idle_high    = hal::Elecrow5Inch::PCLK_IDLE_HIGH;
    panel_config.data_width                      = 16;   // RGB565: 16 data lines
    panel_config.bits_per_pixel                  = 16;
    panel_config.num_fbs                         = 2;    // true double-buffering
    // bounce_buffer_size_px is intentionally 0 (GDMA reads PSRAM directly).
    //
    // Setting this to e.g. 800*10 would reduce PSRAM bus contention artifacts but
    // causes a "Cache disabled but cached memory region accessed" hard fault on boot:
    // the IDF bounce-buffer copy task accesses PSRAM while WiFi init briefly disables
    // the shared flash/PSRAM cache. Fixing this requires CONFIG_SPIRAM_FETCH_INSTRUCTIONS=y
    // (places code in PSRAM so cache-disable is safe), which is a framework-level
    // sdkconfig change not yet applied to this project.
    panel_config.bounce_buffer_size_px           = 0;
    panel_config.psram_trans_align               = 64;
    panel_config.hsync_gpio_num                  = hal::Elecrow5Inch::PIN_HSYNC;
    panel_config.vsync_gpio_num                  = hal::Elecrow5Inch::PIN_VSYNC;
    panel_config.de_gpio_num                     = hal::Elecrow5Inch::PIN_HENABLE;
    panel_config.pclk_gpio_num                   = hal::Elecrow5Inch::PIN_PCLK;
    panel_config.disp_gpio_num                   = GPIO_NUM_NC;
    panel_config.data_gpio_nums[0]               = hal::Elecrow5Inch::PIN_D0;
    panel_config.data_gpio_nums[1]               = hal::Elecrow5Inch::PIN_D1;
    panel_config.data_gpio_nums[2]               = hal::Elecrow5Inch::PIN_D2;
    panel_config.data_gpio_nums[3]               = hal::Elecrow5Inch::PIN_D3;
    panel_config.data_gpio_nums[4]               = hal::Elecrow5Inch::PIN_D4;
    panel_config.data_gpio_nums[5]               = hal::Elecrow5Inch::PIN_D5;
    panel_config.data_gpio_nums[6]               = hal::Elecrow5Inch::PIN_D6;
    panel_config.data_gpio_nums[7]               = hal::Elecrow5Inch::PIN_D7;
    panel_config.data_gpio_nums[8]               = hal::Elecrow5Inch::PIN_D8;
    panel_config.data_gpio_nums[9]               = hal::Elecrow5Inch::PIN_D9;
    panel_config.data_gpio_nums[10]              = hal::Elecrow5Inch::PIN_D10;
    panel_config.data_gpio_nums[11]              = hal::Elecrow5Inch::PIN_D11;
    panel_config.data_gpio_nums[12]              = hal::Elecrow5Inch::PIN_D12;
    panel_config.data_gpio_nums[13]              = hal::Elecrow5Inch::PIN_D13;
    panel_config.data_gpio_nums[14]              = hal::Elecrow5Inch::PIN_D14;
    panel_config.data_gpio_nums[15]              = hal::Elecrow5Inch::PIN_D15;
    panel_config.flags.fb_in_psram               = 1;    // both FBs in PSRAM

    esp_err_t ret = esp_lcd_new_rgb_panel(&panel_config, &panel_handle_);
    if (ret != ESP_OK) {
        Serial.printf("[LVGL] esp_lcd_new_rgb_panel: %s\n", esp_err_to_name(ret));
        return false;
    }

    esp_lcd_rgb_panel_event_callbacks_t cbs = {};
    cbs.on_vsync = on_vsync_event;
    esp_lcd_rgb_panel_register_event_callbacks(panel_handle_, &cbs, nullptr);

    esp_lcd_panel_reset(panel_handle_);
    esp_lcd_panel_init(panel_handle_);

    // Get both framebuffer pointers (allocated by IDF in PSRAM)
    esp_lcd_rgb_panel_get_frame_buffer(panel_handle_, 2, &fb1_, &fb2_);
    memset(fb1_, 0, (size_t)hal::Elecrow5Inch::PANEL_WIDTH *
                    hal::Elecrow5Inch::PANEL_HEIGHT * sizeof(uint16_t));
    memset(fb2_, 0, (size_t)hal::Elecrow5Inch::PANEL_WIDTH *
                    hal::Elecrow5Inch::PANEL_HEIGHT * sizeof(uint16_t));

    // Backlight + touch via LGFX (no display bus — Panel_Device init-only)
    lcd = new LGFX_Panel();
    if (!lcd) {
        Serial.println("[LVGL] Failed to allocate LGFX touch wrapper");
        return false;
    }
    // Panel_NULL::init() is a no-op (returns false immediately, never touches LEDC/touch).
    // Initialize backlight and touch directly via their sub-instances.
    lcd->_light_instance.init(currentBrightness);
    lcd->_touch_instance.init();

    lv_init();

    // 1-ms hardware tick — lv_tick_inc is IRQ-safe, intentionally outside lv_lock
    {
        esp_timer_create_args_t args = {};
        args.callback = [](void*) { lv_tick_inc(1); };
        args.name = "lvgl_tick";
        args.dispatch_method = ESP_TIMER_TASK;
        esp_err_t err = esp_timer_create(&args, &lvgl_tick_timer_);
        if (err != ESP_OK) {
            Serial.printf("[LVGL] tick timer create: %s\n", esp_err_to_name(err));
            return false;
        }
        err = esp_timer_start_periodic(lvgl_tick_timer_, 1000 /* µs = 1 ms */);
        if (err != ESP_OK) {
            Serial.printf("[LVGL] tick timer start: %s\n", esp_err_to_name(err));
            esp_timer_delete(lvgl_tick_timer_);
            lvgl_tick_timer_ = nullptr;
            return false;
        }
    }

    // FULL mode: LVGL renders the complete 800×480 frame into one IDF framebuffer,
    // then flush_cb calls draw_bitmap (zero-copy vsync swap) to show it.
    // This eliminates the stale back-buffer problem of DIRECT mode: in DIRECT mode
    // only dirty regions are rendered into the draw buffer, leaving non-dirty areas
    // with content from 2 frames ago. After the buffer swap, the stale non-dirty
    // areas briefly show old content — visible as a "flicker" or "jump" on every
    // update. FULL mode renders every pixel fresh each frame, so both buffers are
    // always consistent. Overhead is negligible for a 10-second update interval.
    static constexpr size_t kFbBytes =
        (size_t)hal::Elecrow5Inch::PANEL_WIDTH *
        hal::Elecrow5Inch::PANEL_HEIGHT * sizeof(lv_color_t);

    lv_display = lv_display_create(hal::Elecrow5Inch::PANEL_WIDTH,
                                   hal::Elecrow5Inch::PANEL_HEIGHT);
    lv_display_set_flush_cb(lv_display, flush_cb);
    lv_display_set_buffers(lv_display, fb1_, fb2_,
                           kFbBytes, LV_DISPLAY_RENDER_MODE_FULL);

    lv_indev = lv_indev_create();
    lv_indev_set_type(lv_indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(lv_indev, touchpad_read);
    lv_indev_set_disp(lv_indev, lv_display);

    // 24KB stack: lv_timer_handler() drives flexbox, multi-size fonts, 33-pt polylines,
    // and up to 15 blips × 3 objects + 15 rows × 5 labels. 16KB is borderline and can
    // silently overflow during complex redraws (corrupts adjacent heap, not a clean crash).
    static constexpr uint32_t kLvglStackBytes = 24576;
    BaseType_t rc = xTaskCreatePinnedToCore(
        lvgl_task, "lvgl", kLvglStackBytes, nullptr, 2, &lvgl_task_handle_, 1);
    if (rc != pdPASS) {
        Serial.println("[LVGL] Failed to create LVGL task");
        return false;
    }

    Serial.println("[LVGL] Hardware + double-buffer ready");
    return true;
}

// Phase 2: build LVGL widget trees.
// Call this AFTER WiFi init (large widget objects go to PSRAM via stdlib malloc).
bool LVGLDisplayManager::buildScreens() {
    lv_lock();
    build_home_screen();
    build_radar_screen();
    lv_screen_load(screen_home);
    currentScreen = SCREEN_HOME;
    lv_unlock();
    Serial.println("[LVGL] Screens built");
    return true;
}

// Combined convenience wrapper (used by tests or simple callers)
bool LVGLDisplayManager::initialize() {
    return initHardware() && buildScreens();
}

// Called by the LVGL task once per dirty region (inside lv_timer_handler).
// For RGB panels with num_fbs=2, draw_bitmap() just schedules which PSRAM framebuffer
// the GDMA will scan from at the next vsync — it returns immediately (no bus transfer).
// Calling lv_display_flush_ready() immediately is correct: LVGL will not start rendering
// the next frame until lv_timer_handler() is called again, which is gated behind the
// vsync semaphore in lvgl_task. By the time that gate opens, the hardware has already
// swapped to the new front buffer, so the old front buffer is safe to render into.
// flush_cb is called by lv_timer_handler() for each dirty region.
//
// For the LAST region of each frame we call draw_bitmap() to schedule the buffer swap
// at the next vsync, then set flush_pending_ WITHOUT calling lv_display_flush_ready().
// lvgl_task calls flush_ready() AFTER the vsync semaphore fires (i.e. after the hardware
// has actually swapped buffers). This keeps LVGL's front/back buffer tracking in phase
// with the hardware — calling flush_ready() immediately (before vsync) causes LVGL to
// write dirty regions into the wrong buffer, producing the flickering/stale-buffer bug
// visible as blips and cardinal labels alternating between correct and missing states.
//
// For non-last regions, flush_ready() is called immediately (no buffer swap involved).
void LVGLDisplayManager::flush_cb(lv_display_t* disp, const lv_area_t* area, uint8_t* px_map) {
    if (!s_instance || !s_instance->panel_handle_) {
        lv_display_flush_ready(disp);
        return;
    }
    if (lv_display_flush_is_last(disp)) {
        if (s_instance->freeze_rendering_) {
            // draw_bitmap MUST NOT be called during freeze — this means the flag
            // failed to stop lv_timer_handler. Log and count for diagnosis.
            s_instance->freeze_draw_leaked_++;
            Serial.printf("[FREEZE] !! draw_bitmap DURING FREEZE t=%lu leaked=%lu\n",
                          millis(), (unsigned long)s_instance->freeze_draw_leaked_);
        }
        int32_t w = lv_display_get_horizontal_resolution(disp);
        int32_t h = lv_display_get_vertical_resolution(disp);
        esp_lcd_panel_draw_bitmap(s_instance->panel_handle_, 0, 0, w, h, px_map);
        s_instance->flush_pending_ = true;
        uint32_t now_bm = millis();
        if (s_instance->log_next_bitmap_) {
            Serial.printf("[DRAW] first post-freeze bitmap at t=%lu ms\n", now_bm);
            s_instance->log_next_bitmap_ = false;
        }
        s_instance->last_bitmap_ms_ = now_bm;
    } else {
        lv_display_flush_ready(disp);
    }
}

bool IRAM_ATTR LVGLDisplayManager::on_vsync_event(esp_lcd_panel_handle_t /*panel*/,
                                        const esp_lcd_rgb_panel_event_data_t* /*edata*/,
                                        void* /*user_ctx*/) {
    // Wake lvgl_task so it calls lv_timer_handler() on every vsync.
    // lv_display_flush_ready() is called immediately in flush_cb (not here),
    // which is the correct pattern for RGB panels — the vsync gate in lvgl_task
    // ensures LVGL never renders into the current front buffer.
    BaseType_t awoken = pdFALSE;
    if (s_instance && s_instance->sem_vsync_end_)
        xSemaphoreGiveFromISR(s_instance->sem_vsync_end_, &awoken);
    return awoken == pdTRUE;
}

// Freeze rendering across a blocking network call.
//
// Two-layer guard:
//   1. freeze_rendering_ flag — lvgl_task checks this BEFORE calling lv_timer_handler().
//      Skipping lv_timer_handler() means no new draws or buffer swaps happen regardless
//      of lv_lock state. Both tasks are pinned to Core 1, so volatile bool is sufficient.
//   2. lv_lock() — waits for any render that was ALREADY in progress (lv_timer_handler
//      holds lv_lock internally) to complete before returning. Without this, we could
//      start the HTTP fetch while LVGL is mid-frame.
//
// On unfreeze: lv_unlock() releases the wait-for-in-progress-render guard first, then
// freeze_rendering_ is cleared so lvgl_task resumes rendering.
void LVGLDisplayManager::freezeRendering() {
    freeze_draw_leaked_ = 0;
    freeze_rendering_   = true;
    freeze_start_ms_    = millis();
    lv_lock();
    Serial.printf("[FREEZE] START  t=%lu ms  last_bitmap=%lu ms ago  vsync_timeouts=%lu\n",
                  freeze_start_ms_,
                  last_bitmap_ms_ ? freeze_start_ms_ - last_bitmap_ms_ : 0,
                  (unsigned long)vsync_timeouts_);
}
void LVGLDisplayManager::unfreezeRendering() {
    uint32_t held_ms = millis() - freeze_start_ms_;
    lv_unlock();
    freeze_rendering_ = false;
    log_next_bitmap_  = true;
    Serial.printf("[FREEZE] END    t=%lu ms  held=%lu ms  leaked_draw_bitmaps=%lu\n",
                  millis(), held_ms, (unsigned long)freeze_draw_leaked_);
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
    } else {
        data->state = LV_INDEV_STATE_RELEASED;
    }
}

// LVGL handler task — paced by VSYNC.
//
// lv_timer_handler() acquires lv_lock() INTERNALLY (lv_timer.c:81).
// We must NOT hold lv_lock() before calling it.
//
// flush_cb calls lv_display_flush_ready() immediately (after draw_bitmap for last area).
// lv_timer_handler() won't render the next frame within the same call — it returns after
// flushing. The vsync gate here ensures the hardware has swapped buffers before we
// render the next frame, so we never write into the currently-displayed buffer.
void LVGLDisplayManager::lvgl_task(void* /*arg*/) {
    while (true) {
        BaseType_t got_vsync = xSemaphoreTake(s_instance->sem_vsync_end_, pdMS_TO_TICKS(60));  // 60ms > 43ms frame @ 10MHz pclk

        if (got_vsync == pdFALSE) {
            // 40 ms elapsed without a vsync ISR firing — hardware may have stalled.
            uint32_t n = ++s_instance->vsync_timeouts_;
            if (n == 1 || n % 20 == 0) {
                Serial.printf("[LVGL] vsync TIMEOUT #%lu at t=%lu ms  flush_pending=%d  freeze=%d\n",
                              (unsigned long)n, millis(),
                              (int)s_instance->flush_pending_,
                              (int)s_instance->freeze_rendering_);
            }
        } else {
            s_instance->vsync_timeouts_ = 0;  // reset streak on successful vsync
        }

        if (s_instance->flush_pending_) {
            s_instance->flush_pending_ = false;
            lv_display_flush_ready(s_instance->lv_display);
        }

        if (!s_instance->freeze_rendering_) {
            lv_timer_handler();
        }

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
    if (!w.label_temperature) return;

    // Only call lv_label_set_text when content actually changed — avoids dirtying
    // the widget (and triggering a PARTIAL flush) when data hasn't changed.
    auto setLbl = [](lv_obj_t* lbl, const char* text) {
        if (lbl && strcmp(lv_label_get_text(lbl), text) != 0)
            lv_label_set_text(lbl, text);
    };

    char buf[64];

    snprintf(buf, sizeof(buf), "%.0f\xc2\xb0""F", weather.temperature);
    setLbl(w.label_temperature, buf);

    setLbl(w.label_weather_desc, weather.description.c_str());

    snprintf(buf, sizeof(buf), "Feels: %.0f\xc2\xb0""F", weather.feelsLike);
    setLbl(w.label_feels_like, buf);

    snprintf(buf, sizeof(buf), "H: %.0f\xc2\xb0  L: %.0f\xc2\xb0", weather.tempMax, weather.tempMin);
    setLbl(w.label_temp_range, buf);

    snprintf(buf, sizeof(buf), "%.0f mph", weather.windSpeed);
    setLbl(w.label_wind, buf);

    snprintf(buf, sizeof(buf), "%.0f%%", weather.humidity);
    setLbl(w.label_humidity_val, buf);

    snprintf(buf, sizeof(buf), LV_SYMBOL_UP " %s", formatTime(weather.sunrise).c_str());
    setLbl(w.label_sunrise, buf);

    snprintf(buf, sizeof(buf), LV_SYMBOL_DOWN " %s", formatTime(weather.sunset).c_str());
    setLbl(w.label_sunset, buf);

    for (int i = 0; i < 5; i++) {
        if (!w.forecast[i].container) continue;
        if (i < (int)weather.forecast.size()) {
            const auto& day = weather.forecast[i];
            setLbl(w.forecast[i].label_day,  day.dayName.c_str());
            setLbl(w.forecast[i].label_cond, day.condition.c_str());
            snprintf(buf, sizeof(buf), "%.0f\xc2\xb0", day.tempMax);
            setLbl(w.forecast[i].label_hi, buf);
            snprintf(buf, sizeof(buf), "%.0f\xc2\xb0", day.tempMin);
            setLbl(w.forecast[i].label_lo, buf);
        } else {
            setLbl(w.forecast[i].label_day,  "-");
            setLbl(w.forecast[i].label_cond, "");
            setLbl(w.forecast[i].label_hi,   "--\xc2\xb0");
            setLbl(w.forecast[i].label_lo,   "--\xc2\xb0");
        }
    }

    // Status bar
    if (w.label_status_left) {
        if (!wifiConnected_) {
            lv_obj_set_style_text_color(w.label_status_left, COLOR_DESCENT, 0);
            setLbl(w.label_status_left, "NO WIFI SIGNAL");
        } else if (aircraftCount > 0) {
            char ts[16];
            struct tm ti;
            getLocalTime(&ti);
            strftime(ts, sizeof(ts), "%H:%M", &ti);
            snprintf(buf, sizeof(buf), "OPENSKY OK / %s", ts);
            lv_obj_set_style_text_color(w.label_status_left, COLOR_TEXT_DIM, 0);
            setLbl(w.label_status_left, buf);
        } else {
            lv_obj_set_style_text_color(w.label_status_left, COLOR_TEXT_DIM, 0);
            setLbl(w.label_status_left, "NO AIRCRAFT DETECTED");
        }
    }
    if (w.label_status_live) {
        if (!wifiConnected_) {
            lv_obj_set_style_text_color(w.label_status_live, COLOR_DESCENT, 0);
            setLbl(w.label_status_live, "OFFLINE");
        } else if (aircraftCount > 0) {
            lv_obj_set_style_text_color(w.label_status_live, COLOR_SUCCESS, 0);
            setLbl(w.label_status_live, "LIVE");
        } else {
            lv_obj_set_style_text_color(w.label_status_live, COLOR_TEXT_DIM, 0);
            setLbl(w.label_status_live, "IDLE");
        }
    }
}

void LVGLDisplayManager::build_home_screen() {
    screen_home = lv_obj_create(NULL);
    lv_obj_clear_flag(screen_home, LV_OBJ_FLAG_SCROLLABLE);
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
    lv_label_set_text(label_airspace_status_, LV_SYMBOL_BULLET " AIRSPACE CLEAR");
    lv_obj_set_pos(label_airspace_status_, 10, 298);

    label_airspace_sub_ = lv_label_create(parent);
    lv_obj_set_style_text_font(label_airspace_sub_, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(label_airspace_sub_, COLOR_TEXT_DIM, 0);
    lv_label_set_text(label_airspace_sub_, "No aircraft within 25nm");
    lv_obj_set_pos(label_airspace_sub_, 10, 318);

    // Tapping anywhere on the airspace panel navigates to radar
    lv_obj_add_flag(parent, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(parent, event_show_radar, LV_EVENT_CLICKED, this);
}

void LVGLDisplayManager::build_radar_screen() {
    if (screen_radar) return;  // already built — prevent leak on re-init
    screen_radar = lv_obj_create(NULL);
    lv_obj_clear_flag(screen_radar, LV_OBJ_FLAG_SCROLLABLE);
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
    lv_label_set_text(label_radar_count_, LV_SYMBOL_BULLET " 0 AIRCRAFT NEARBY");
    lv_obj_align(label_radar_count_, LV_ALIGN_CENTER, 0, 0);

    // Back button: bordered, clearly tappable
    lv_obj_t* back_btn = lv_obj_create(topbar);
    lv_obj_set_size(back_btn, 90, 34);
    lv_obj_align(back_btn, LV_ALIGN_RIGHT_MID, -12, 0);
    lv_obj_set_style_bg_color(back_btn, COLOR_INSET, 0);
    lv_obj_set_style_bg_opa(back_btn, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(back_btn, COLOR_ACCENT, 0);
    lv_obj_set_style_border_opa(back_btn, 100, 0);
    lv_obj_set_style_border_width(back_btn, 1, 0);
    lv_obj_set_style_radius(back_btn, 4, 0);
    lv_obj_set_style_pad_all(back_btn, 0, 0);
    lv_obj_clear_flag(back_btn, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(back_btn, event_topbar_back, LV_EVENT_CLICKED, this);

    lv_obj_t* back_lbl = lv_label_create(back_btn);
    lv_obj_set_style_text_font(back_lbl, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(back_lbl, COLOR_TEXT_SECONDARY, 0);
    lv_label_set_text(back_lbl, LV_SYMBOL_LEFT " HOME");
    lv_obj_center(back_lbl);

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

    // PWM airport marker: small amber dot (subtle reference, no label)
    {
        auto pwm = GeoUtils::latLonToRadarPx(
            Config::HOME_LAT, Config::HOME_LON,
            Config::PWM_LAT, Config::PWM_LON,
            Config::RADAR_MAX_RANGE_NM, Config::RADAR_CIRCLE_RADIUS);
        lv_obj_t* pwm_dot = lv_obj_create(radar_circle_);
        lv_obj_set_size(pwm_dot, 6, 6);
        lv_obj_set_pos(pwm_dot, pwm.x - 3, pwm.y - 3);
        lv_obj_set_style_radius(pwm_dot, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_color(pwm_dot, COLOR_AMBER, 0);
        lv_obj_set_style_bg_opa(pwm_dot, 180, 0);
        lv_obj_set_style_border_width(pwm_dot, 0, 0);
        lv_obj_clear_flag(pwm_dot, LV_OBJ_FLAG_CLICKABLE);
    }

    // Range labels (inside circle edge, near N/S axis) — use radar_col as parent so they
    // render on top of the circle without being clipped by its circular mask
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
    lv_label_set_text(label_list_header_, "AIRCRAFT IN RANGE  |  0");
    lv_obj_set_pos(label_list_header_, 12, 8);

    // Thin divider line
    lv_obj_t* ldiv = lv_obj_create(list_col);
    lv_obj_set_size(ldiv, 286, 1);
    lv_obj_set_pos(ldiv, 12, 26);
    lv_obj_set_style_bg_color(ldiv, COLOR_BORDER, 0);
    lv_obj_set_style_border_width(ldiv, 0, 0);
    lv_obj_clear_flag(ldiv, LV_OBJ_FLAG_CLICKABLE);

    // Aircraft list container
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
    lv_obj_clear_flag(list_container_, LV_OBJ_FLAG_SCROLLABLE);

    // Pre-allocate rows
    for (int i = 0; i < Config::MAX_AIRCRAFT; i++) {
        AircraftListRow& row = list_rows_[i];

        row.container = lv_obj_create(list_container_);
        lv_obj_set_width(row.container, 310);
        lv_obj_set_height(row.container, 84);
        lv_obj_set_style_bg_color(row.container, COLOR_PANEL, 0);
        lv_obj_set_style_bg_opa(row.container, LV_OPA_COVER, 0);
        lv_obj_set_style_border_side(row.container, LV_BORDER_SIDE_BOTTOM, 0);
        lv_obj_set_style_border_color(row.container, COLOR_BORDER, 0);
        lv_obj_set_style_border_width(row.container, 1, 0);
        lv_obj_set_style_radius(row.container, 0, 0);
        lv_obj_set_style_pad_all(row.container, 0, 0);
        lv_obj_clear_flag(row.container, (lv_obj_flag_t)(LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE));
        lv_obj_add_flag(row.container, LV_OBJ_FLAG_HIDDEN);

        // Left accent bar (4px wide)
        row.accent_bar = lv_obj_create(row.container);
        lv_obj_set_size(row.accent_bar, 4, 84);
        lv_obj_set_pos(row.accent_bar, 0, 0);
        lv_obj_set_style_bg_color(row.accent_bar, COLOR_ACCENT, 0);
        lv_obj_set_style_border_width(row.accent_bar, 0, 0);
        lv_obj_set_style_radius(row.accent_bar, 0, 0);
        lv_obj_clear_flag(row.accent_bar, LV_OBJ_FLAG_CLICKABLE);

        // Airline name (primary — biggest text in row)
        row.label_callsign = lv_label_create(row.container);
        lv_obj_set_style_text_font(row.label_callsign, &lv_font_montserrat_16, 0);
        lv_obj_set_style_text_color(row.label_callsign, COLOR_ACCENT, 0);
        lv_label_set_text(row.label_callsign, "------");
        lv_obj_set_pos(row.label_callsign, 14, 8);

        // Route line: "CITY, CC → CITY, CC · CALLSIGN"
        row.label_type_route = lv_label_create(row.container);
        lv_obj_set_style_text_font(row.label_type_route, &lv_font_montserrat_12, 0);
        lv_obj_set_style_text_color(row.label_type_route, COLOR_TEXT_SECONDARY, 0);
        lv_label_set_text(row.label_type_route, "");
        lv_obj_set_pos(row.label_type_route, 14, 30);

        // Aircraft type (one step up from before — 14px)
        row.label_type = lv_label_create(row.container);
        lv_obj_set_style_text_font(row.label_type, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(row.label_type, COLOR_TEXT_DIM, 0);
        lv_label_set_text(row.label_type, "");
        lv_obj_set_pos(row.label_type, 14, 48);

        // Stats line: alt / speed / bearing / distance
        row.label_summary = lv_label_create(row.container);
        lv_obj_set_style_text_font(row.label_summary, &lv_font_montserrat_12, 0);
        lv_obj_set_style_text_color(row.label_summary, COLOR_TEXT_DIM, 0);
        lv_label_set_text(row.label_summary, "");
        lv_obj_set_pos(row.label_summary, 14, 65);

        // Expanded panel intentionally not pre-allocated — too many style objects
        // for the LVGL pool across MAX_AIRCRAFT rows. All key data shown in label_summary.
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

    label_radar_status_left_ = lv_label_create(sbar);
    lv_obj_set_style_text_font(label_radar_status_left_, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(label_radar_status_left_, COLOR_TEXT_DIM, 0);
    lv_label_set_text(label_radar_status_left_, "OPENSKY OK");
    lv_obj_align(label_radar_status_left_, LV_ALIGN_LEFT_MID, 12, 0);

    label_radar_status_live_ = lv_label_create(sbar);
    lv_obj_set_style_text_font(label_radar_status_live_, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(label_radar_status_live_, COLOR_SUCCESS, 0);
    lv_label_set_text(label_radar_status_live_, LV_SYMBOL_BULLET " LIVE");
    lv_obj_align(label_radar_status_live_, LV_ALIGN_RIGHT_MID, -12, 0);
}

void LVGLDisplayManager::update_home_screen(const WeatherData& weather,
                                             int aircraftCount) {
    update_clock(homeWidgets);
    updateWeatherWidgets(homeWidgets, weather, aircraftCount);

    if (!label_airspace_status_) return;

    if (aircraftCount > 0) {
        char buf[40];
        snprintf(buf, sizeof(buf), "%d AIRCRAFT NEARBY", aircraftCount);
        lv_obj_set_style_text_color(label_airspace_status_, COLOR_AMBER, 0);
        lv_label_set_text(label_airspace_status_, buf);
        lv_label_set_text(label_airspace_sub_, "");
    } else {
        lv_obj_set_style_text_color(label_airspace_status_, COLOR_SUCCESS, 0);
        lv_label_set_text(label_airspace_status_, LV_SYMBOL_BULLET " AIRSPACE CLEAR");
        lv_label_set_text(label_airspace_sub_, "No aircraft within 25nm");
    }
}


// Update clock
void LVGLDisplayManager::update_clock(WeatherWidgets& w) {
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo)) return;

    char time_buf[16];
    strftime(time_buf, sizeof(time_buf), "%I:%M %p", &timeinfo);
    if (w.label_time && strcmp(lv_label_get_text(w.label_time), time_buf) != 0)
        lv_label_set_text(w.label_time, time_buf);

    char date_buf[32];
    strftime(date_buf, sizeof(date_buf), "%a, %b %d", &timeinfo);
    if (w.label_date && strcmp(lv_label_get_text(w.label_date), date_buf) != 0)
        lv_label_set_text(w.label_date, date_buf);

    if (statusClearTime > 0 && millis() >= statusClearTime) {
        statusClearTime = 0;
        statusMessage = "";
        if (homeWidgets.label_status_left) lv_label_set_text(homeWidgets.label_status_left, "");
    }
}

static void blip_anim_x(void* obj, int32_t v) { lv_obj_set_x((lv_obj_t*)obj, v); }
static void blip_anim_y(void* obj, int32_t v) { lv_obj_set_y((lv_obj_t*)obj, v); }

void LVGLDisplayManager::update_radar_screen(const Aircraft* aircraft,
                                               int aircraftCount) {
    if (!label_radar_count_) return;

    auto setLbl = [](lv_obj_t* lbl, const char* text) {
        if (lbl && strcmp(lv_label_get_text(lbl), text) != 0)
            lv_label_set_text(lbl, text);
    };

    // Update top bar clock
    {
        struct tm ti;
        if (getLocalTime(&ti)) {
            char tb[12];
            strftime(tb, sizeof(tb), "%H:%M", &ti);
            setLbl(label_radar_time_, tb);
            char db[20];
            strftime(db, sizeof(db), "%a %d %b %Y", &ti);
            setLbl(label_radar_date_, db);
        }
        char cb[32];
        snprintf(cb, sizeof(cb), LV_SYMBOL_BULLET " %d AIRCRAFT NEARBY", aircraftCount);
        setLbl(label_radar_count_, cb);
    }

    // Update list header
    {
        char hb[40];
        snprintf(hb, sizeof(hb), "AIRCRAFT IN RANGE  |  %d", aircraftCount);
        setLbl(label_list_header_, hb);
    }

    // Status bar — reflects WiFi / data state
    if (label_radar_status_left_ && label_radar_status_live_) {
        if (!wifiConnected_) {
            lv_obj_set_style_text_color(label_radar_status_left_, COLOR_DESCENT, 0);
            setLbl(label_radar_status_left_, "NO SIGNAL");
            lv_obj_set_style_text_color(label_radar_status_live_, COLOR_TEXT_DIM, 0);
            setLbl(label_radar_status_live_, LV_SYMBOL_BULLET " OFFLINE");
        } else {
            lv_obj_set_style_text_color(label_radar_status_left_, COLOR_TEXT_DIM, 0);
            setLbl(label_radar_status_left_, "OPENSKY OK");
            lv_obj_set_style_text_color(label_radar_status_live_, COLOR_SUCCESS, 0);
            setLbl(label_radar_status_live_, LV_SYMBOL_BULLET " LIVE");
        }
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

            // --- Radar blip position (animated) ---
            bool firstPlacement = !blip.placed;
            bool dotMoved       = false;
            {
                int32_t nx = pos.x - 6, ny = pos.y - 6;
                if (firstPlacement) {
                    // First appearance — snap directly; no animation from undefined position.
                    lv_obj_set_pos(blip.dot, nx, ny);
                    blip.targetDotX = nx;
                    blip.targetDotY = ny;
                    blip.placed = true;
                } else if (blip.targetDotX != nx || blip.targetDotY != ny) {
                    // Aircraft moved — animate from current (possibly mid-animation) position.
                    dotMoved = true;
                    int32_t fromX = lv_obj_get_x(blip.dot);
                    int32_t fromY = lv_obj_get_y(blip.dot);
                    blip.targetDotX = nx;
                    blip.targetDotY = ny;
                    lv_anim_del(blip.dot, blip_anim_x);
                    lv_anim_del(blip.dot, blip_anim_y);
                    lv_anim_t da;
                    lv_anim_init(&da);
                    lv_anim_set_var(&da, blip.dot);
                    lv_anim_set_duration(&da, 2000);
                    lv_anim_set_path_cb(&da, lv_anim_path_ease_in_out);
                    lv_anim_set_exec_cb(&da, blip_anim_x);
                    lv_anim_set_values(&da, fromX, nx);
                    lv_anim_start(&da);
                    lv_anim_set_exec_cb(&da, blip_anim_y);
                    lv_anim_set_values(&da, fromY, ny);
                    lv_anim_start(&da);
                }
            }
            // lv_obj_set_style_* always marks dirty even when the value is identical —
            // guard all color calls so redraws only happen when altitude band changes.
            if (!lv_color_eq(blip.lastColor, blipColor)) {
                lv_obj_set_style_bg_color(blip.dot, blipColor, 0);
                lv_obj_set_style_line_color(blip.vector, blipColor, 0);
                lv_obj_set_style_text_color(blip.label, blipColor, 0);
                blip.lastColor = blipColor;
            }
            lv_obj_remove_flag(blip.dot, LV_OBJ_FLAG_HIDDEN);  // idempotent in LVGL

            // Heading vector (20px) — guard lv_line_set_points the same way:
            // aircraft positions update every 30s but display ticks every 10s.
            float rad = a.heading * GeoUtils::DEG_TO_RAD;
            {
                lv_value_precise_t np0x = (lv_value_precise_t)pos.x;
                lv_value_precise_t np0y = (lv_value_precise_t)pos.y;
                lv_value_precise_t np1x = (lv_value_precise_t)(pos.x + 20.0f * sinf(rad));
                lv_value_precise_t np1y = (lv_value_precise_t)(pos.y - 20.0f * cosf(rad));
                if (blip.vec_pts[0].x != np0x || blip.vec_pts[0].y != np0y ||
                    blip.vec_pts[1].x != np1x || blip.vec_pts[1].y != np1y) {
                    blip.vec_pts[0] = {np0x, np0y};
                    blip.vec_pts[1] = {np1x, np1y};
                    lv_line_set_points(blip.vector, blip.vec_pts, 2);
                }
            }
            lv_obj_remove_flag(blip.vector, LV_OBJ_FLAG_HIDDEN);

            // Callsign label — animates in sync with the dot
            setLbl(blip.label, a.callsign.c_str());
            {
                int32_t lx = pos.x + 10, ly = pos.y - 8;
                if (firstPlacement) {
                    lv_obj_set_pos(blip.label, lx, ly);
                } else if (dotMoved) {
                    int32_t fromLX = lv_obj_get_x(blip.label);
                    int32_t fromLY = lv_obj_get_y(blip.label);
                    lv_anim_del(blip.label, blip_anim_x);
                    lv_anim_del(blip.label, blip_anim_y);
                    lv_anim_t la;
                    lv_anim_init(&la);
                    lv_anim_set_var(&la, blip.label);
                    lv_anim_set_duration(&la, 2000);
                    lv_anim_set_path_cb(&la, lv_anim_path_ease_in_out);
                    lv_anim_set_exec_cb(&la, blip_anim_x);
                    lv_anim_set_values(&la, fromLX, lx);
                    lv_anim_start(&la);
                    lv_anim_set_exec_cb(&la, blip_anim_y);
                    lv_anim_set_values(&la, fromLY, ly);
                    lv_anim_start(&la);
                }
            }
            lv_obj_remove_flag(blip.label, LV_OBJ_FLAG_HIDDEN);

            // --- List row ---
            lv_obj_remove_flag(row.container, LV_OBJ_FLAG_HIDDEN);
            if (!lv_color_eq(row.lastColor, blipColor)) {
                lv_obj_set_style_bg_color(row.accent_bar, blipColor, 0);
                lv_obj_set_style_text_color(row.label_callsign, blipColor, 0);
                row.lastColor = blipColor;
            }

            // Row 1: "Airline  CALLSIGN" or just callsign if airline unknown
            {
                char ln1[48];
                if (a.airline.isEmpty()) {
                    snprintf(ln1, sizeof(ln1), "%s", a.callsign.c_str());
                } else {
                    snprintf(ln1, sizeof(ln1), "%s  %s", a.airline.c_str(), a.callsign.c_str());
                }
                setLbl(row.label_callsign, ln1);
            }

            // Row 2: "CITY > CITY"
            {
                char tr[64];
                const String& org = a.originDisplay.isEmpty()      ? a.origin      : a.originDisplay;
                const String& dst = a.destinationDisplay.isEmpty() ? a.destination : a.destinationDisplay;
                if (!org.isEmpty() && !dst.isEmpty()) {
                    snprintf(tr, sizeof(tr), "%s > %s", org.c_str(), dst.c_str());
                } else {
                    tr[0] = '\0';
                }
                setLbl(row.label_type_route, tr);
            }

            // Aircraft type
            setLbl(row.label_type, a.aircraftType.isEmpty() ? "" : a.aircraftType.c_str());

            // Stats line
            float speedKt = a.velocity * 1.94384f;
            char sm[80];
            snprintf(sm, sizeof(sm), "%.0f ft / %.0f kt / %s / %.1f nm",
                     altFt, speedKt, GeoUtils::cardinalDir(bearing), distNm);
            setLbl(row.label_summary, sm);

        } else {
            // Aircraft gone — cancel any in-flight animation and reset for next appearance.
            lv_anim_del(blip.dot,   blip_anim_x);
            lv_anim_del(blip.dot,   blip_anim_y);
            lv_anim_del(blip.label, blip_anim_x);
            lv_anim_del(blip.label, blip_anim_y);
            blip.placed = false;
            lv_obj_add_flag(blip.dot,      LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(blip.vector,   LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(blip.label,    LV_OBJ_FLAG_HIDDEN);
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
            if (screen_home) lv_screen_load(screen_home);
            break;
        case SCREEN_RADAR:
            if (!screen_radar) { lv_unlock(); return; }
            currentScreen = screen;
            lv_screen_load(screen_radar);
            break;
    }
    lv_unlock();
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

void LVGLDisplayManager::setWifiConnected(bool connected) {
    lv_lock();
    wifiConnected_ = connected;
    lv_unlock();
}

void LVGLDisplayManager::setStatusMessage(const String& msg) {
    lv_lock();
    statusMessage = msg;
    statusClearTime = millis() + Config::UI_STATUS_MS;
    if (homeWidgets.label_status_left) lv_label_set_text(homeWidgets.label_status_left, msg.c_str());
    lv_unlock();
}

void LVGLDisplayManager::event_topbar_back(lv_event_t* e) {
    (void)e;
    if (s_instance) s_instance->userDismissed_ = true;
}

void LVGLDisplayManager::event_show_radar(lv_event_t* e) {
    (void)e;
    if (s_instance) s_instance->userRequestedRadar_ = true;
}

bool LVGLDisplayManager::wasUserRequestedRadar() {
    lv_lock();
    bool result = userRequestedRadar_;
    userRequestedRadar_ = false;
    lv_unlock();
    return result;
}

lgfx::LGFX_Device* LVGLDisplayManager::getLCD() {
    return lcd;
}
