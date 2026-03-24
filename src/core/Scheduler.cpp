#include "core/Scheduler.h"

namespace core {

Scheduler::Scheduler() {
    reset();
}

int8_t Scheduler::addTask(uint32_t intervalMs, bool runImmediately) {
    if (intervalMs == 0) {
        return INVALID_TASK;
    }

    for (uint8_t i = 0; i < MAX_TASKS; ++i) {
        if (!tasks_[i].active) {
            tasks_[i].active = true;
            tasks_[i].intervalMs = intervalMs;
            tasks_[i].lastRunMs = runImmediately ? (millis() - intervalMs) : millis();
            return static_cast<int8_t>(i);
        }
    }

    return INVALID_TASK;
}

bool Scheduler::due(int8_t taskId, uint32_t nowMs) const {
    if (taskId < 0 || taskId >= static_cast<int8_t>(MAX_TASKS)) {
        return false;
    }

    const TaskSlot& slot = tasks_[taskId];
    if (!slot.active || slot.intervalMs == 0) {
        return false;
    }

    return (nowMs - slot.lastRunMs) >= slot.intervalMs;
}

void Scheduler::markRun(int8_t taskId, uint32_t nowMs) {
    if (taskId < 0 || taskId >= static_cast<int8_t>(MAX_TASKS)) {
        return;
    }

    if (!tasks_[taskId].active) {
        return;
    }

    tasks_[taskId].lastRunMs = nowMs;
}

void Scheduler::reset() {
    for (TaskSlot& slot : tasks_) {
        slot.intervalMs = 0;
        slot.lastRunMs = 0;
        slot.active = false;
    }
}

}  // namespace core
