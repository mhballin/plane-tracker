#pragma once

#include <Arduino.h>

namespace core {

class Scheduler {
public:
    static constexpr uint8_t MAX_TASKS = 12;
    static constexpr int8_t INVALID_TASK = -1;

    Scheduler();

    // intervalMs: how often to run
    // runImmediately: if true, task is due on first call; if false, waits one interval
    // initialDelayMs: optional offset before first run (e.g., 5000 = first run after 5s)
    int8_t addTask(uint32_t intervalMs, bool runImmediately = false, uint32_t initialDelayMs = 0);
    bool due(int8_t taskId, uint32_t nowMs) const;
    void markRun(int8_t taskId, uint32_t nowMs);
    void reset();

private:
    struct TaskSlot {
        uint32_t intervalMs;
        uint32_t lastRunMs;
        bool active;
    };

    TaskSlot tasks_[MAX_TASKS];
};

}  // namespace core
