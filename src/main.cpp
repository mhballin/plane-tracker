#include <Arduino.h>

#include "core/App.h"

#ifndef SMOKE_TEST

static core::App app;

void setup() {
    if (!app.begin()) {
        Serial.println("[FATAL] App failed to initialize");
        while (true) {
            delay(1000);
        }
    }
}

void loop() {
    app.tick();
}

#endif  // SMOKE_TEST
