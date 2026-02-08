#pragma once
#include <atomic>

enum class SystemState {
    INIT,
    CALIBRATED,
    READY,
    RUN
};

void start_ctrl_thread();
SystemState get_system_state();
