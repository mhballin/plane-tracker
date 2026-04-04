// src/core/SerialCommandHandler.h
#pragma once

#include <Arduino.h>
#include "LVGLDisplayManager.h"

namespace core {

class SerialCommandHandler {
public:
    explicit SerialCommandHandler(LVGLDisplayManager* display);

    // Call from App::tick(). Drains Serial buffer and dispatches commands.
    // Also handles raw touch dump if that mode is active.
    void tick();

    bool rawTouchMode() const { return rawTouchMode_; }

private:
    LVGLDisplayManager* display_;
    String buffer_;
    bool rawTouchMode_;

    void dispatch(const String& command);
    void runI2CScan();
    void dumpRawTouch();
};

}  // namespace core
