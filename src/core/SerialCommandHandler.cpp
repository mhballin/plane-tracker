// src/core/SerialCommandHandler.cpp
#include "core/SerialCommandHandler.h"
#include "hal/ElecrowDisplayProfile.h"
#include <Wire.h>

namespace core {

SerialCommandHandler::SerialCommandHandler(LVGLDisplayManager* display)
    : display_(display)
    , buffer_("")
    , rawTouchMode_(false) {}

void SerialCommandHandler::tick() {
    while (Serial.available()) {
        char c = static_cast<char>(Serial.read());
        if (c == '\n' || c == '\r') {
            if (!buffer_.isEmpty()) {
                buffer_.toUpperCase();
                dispatch(buffer_);
                buffer_ = "";
            }
            continue;
        }
        buffer_ += c;
        if (buffer_.length() > 64) {
            buffer_ = buffer_.substring(buffer_.length() - 64);
        }
    }

    if (rawTouchMode_) {
        dumpRawTouch();
    }
}

void SerialCommandHandler::dispatch(const String& command) {
    if (command == "RAW") {
        rawTouchMode_ = !rawTouchMode_;
        Serial.printf("[CMD] RAW mode %s\n", rawTouchMode_ ? "ON" : "OFF");
    } else if (command == "I2CSCAN") {
        runI2CScan();
    } else {
        Serial.printf("[CMD] Unknown command: %s\n", command.c_str());
    }
}

void SerialCommandHandler::runI2CScan() {
    Serial.println("[CMD] Running I2C scan");
    // Wire.end() tears down the bus (including touch controller). This is a
    // diagnostic-only command; the display driver reinitialises touch on next
    // use via its own state, so this is safe for a one-shot scan.
    Wire.end();
    Wire.begin(hal::Elecrow5Inch::TOUCH_PIN_SDA, hal::Elecrow5Inch::TOUCH_PIN_SCL);
    Wire.setClock(hal::Elecrow5Inch::TOUCH_I2C_FREQ);
    int found = 0;
    for (uint8_t address = 1; address < 127; ++address) {
        Wire.beginTransmission(address);
        if (Wire.endTransmission() == 0) {
            Serial.printf("[I2C] 0x%02X\n", address);
            found++;
        }
    }
    Serial.printf("[I2C] Devices found: %d\n", found);
}

void SerialCommandHandler::dumpRawTouch() {
    if (!display_) return;
    auto* lcd = display_->getLCD();
    if (!lcd) return;
    int tx = 0, ty = 0;
    if (lcd->getTouch(&tx, &ty)) {
        Serial.printf("RAW_TOUCH: %d, %d\n", tx, ty);
    }
}

}  // namespace core
