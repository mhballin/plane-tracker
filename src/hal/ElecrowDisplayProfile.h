#pragma once

#include <Arduino.h>
#include <driver/i2c.h>

namespace hal {
namespace Elecrow5Inch {

constexpr int PANEL_WIDTH = 800;
constexpr int PANEL_HEIGHT = 480;
constexpr int PANEL_ROTATION = 2;

constexpr gpio_num_t PIN_D0 = GPIO_NUM_8;
constexpr gpio_num_t PIN_D1 = GPIO_NUM_3;
constexpr gpio_num_t PIN_D2 = GPIO_NUM_46;
constexpr gpio_num_t PIN_D3 = GPIO_NUM_9;
constexpr gpio_num_t PIN_D4 = GPIO_NUM_1;
constexpr gpio_num_t PIN_D5 = GPIO_NUM_5;
constexpr gpio_num_t PIN_D6 = GPIO_NUM_6;
constexpr gpio_num_t PIN_D7 = GPIO_NUM_7;
constexpr gpio_num_t PIN_D8 = GPIO_NUM_15;
constexpr gpio_num_t PIN_D9 = GPIO_NUM_16;
constexpr gpio_num_t PIN_D10 = GPIO_NUM_4;
constexpr gpio_num_t PIN_D11 = GPIO_NUM_45;
constexpr gpio_num_t PIN_D12 = GPIO_NUM_48;
constexpr gpio_num_t PIN_D13 = GPIO_NUM_47;
constexpr gpio_num_t PIN_D14 = GPIO_NUM_21;
constexpr gpio_num_t PIN_D15 = GPIO_NUM_14;

constexpr gpio_num_t PIN_HENABLE = GPIO_NUM_40;
constexpr gpio_num_t PIN_VSYNC = GPIO_NUM_41;
constexpr gpio_num_t PIN_HSYNC = GPIO_NUM_39;
constexpr gpio_num_t PIN_PCLK = GPIO_NUM_0;

constexpr uint32_t RGB_FREQ_WRITE = 15000000;
constexpr uint8_t HSYNC_POLARITY = 0;
constexpr uint16_t HSYNC_FRONT_PORCH = 8;
constexpr uint16_t HSYNC_PULSE_WIDTH = 4;
constexpr uint16_t HSYNC_BACK_PORCH = 43;
constexpr uint8_t VSYNC_POLARITY = 0;
constexpr uint16_t VSYNC_FRONT_PORCH = 8;
constexpr uint16_t VSYNC_PULSE_WIDTH = 4;
constexpr uint16_t VSYNC_BACK_PORCH = 12;
constexpr uint8_t PCLK_ACTIVE_NEG = 1;
constexpr uint8_t DE_IDLE_HIGH = 0;
constexpr uint8_t PCLK_IDLE_HIGH = 0;

constexpr int USE_PSRAM_FRAMEBUFFER = 2;
constexpr gpio_num_t PIN_BACKLIGHT = GPIO_NUM_2;

constexpr uint8_t TOUCH_I2C_ADDR = 0x14;
constexpr gpio_num_t TOUCH_PIN_SDA = GPIO_NUM_19;
constexpr gpio_num_t TOUCH_PIN_SCL = GPIO_NUM_20;
constexpr gpio_num_t TOUCH_PIN_INT = GPIO_NUM_NC;
constexpr gpio_num_t TOUCH_PIN_RST = GPIO_NUM_NC;
constexpr i2c_port_t TOUCH_I2C_PORT = I2C_NUM_1;
constexpr uint32_t TOUCH_I2C_FREQ = 400000;

}  // namespace Elecrow5Inch
}  // namespace hal
