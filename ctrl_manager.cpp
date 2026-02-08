#include "ctrl_manager.h"
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <atomic>
#include <chrono>
#include <cstring>
#include <thread>

#include "can_utils.h"
#include "thread_safe_store.h"

constexpr int CTRL_PORT = 60000;

constexpr uint32_t AXIS_STATE_FULL_CALIBRATION_SEQUENCE = 3;
constexpr uint32_t AXIS_STATE_CLOSED_LOOP_CONTROL = 8;

static const int NODE_ID[16] = {
    33,34,3,4,5,6,7,8,9,10,11,12,13,14,15,16
};

static std::atomic<SystemState> state{SystemState::INIT};
static std::thread ctrl_thread;

SystemState get_system_state() {
    return state.load();
}

static void ctrl_loop() {
    const int sock = socket(AF_INET, SOCK_DGRAM, 0);

    sockaddr_in addr{};
    addr.sin_family      = AF_INET;
    addr.sin_port        = htons(CTRL_PORT);
    addr.sin_addr.s_addr = INADDR_ANY;
    [[maybe_unused]] auto _ = bind(sock, (sockaddr*)&addr, sizeof(addr));

    uint8_t buf[8];

    while (!g_thread_safe_store.Get<bool>("fin")) {
        ssize_t len = recvfrom(sock, buf, sizeof(buf), 0, nullptr, nullptr);
        if (len < 6) continue;
        if (std::memcmp(buf, "CTRL", 4) != 0) continue;

        uint8_t cmd = buf[4];

        if (cmd == 1 && state == SystemState::INIT) {
            for (int id : NODE_ID)
                send_axis_state(id, AXIS_STATE_FULL_CALIBRATION_SEQUENCE);

            std::this_thread::sleep_for(std::chrono::seconds(10));
            state = SystemState::CALIBRATED;
        }

        else if (cmd == 2 && state == SystemState::CALIBRATED) {
            for (int id : NODE_ID)
                send_axis_state(id, AXIS_STATE_CLOSED_LOOP_CONTROL);

            state = SystemState::READY;
        }

        else if (cmd == 6 && state == SystemState::READY) {
            state = SystemState::RUN;
        }

        else if (cmd == 7 && state == SystemState::RUN) {
            state = SystemState::READY;
        }
    }
}

void start_ctrl_thread() {
    ctrl_thread = std::thread(ctrl_loop);
}

void stop_ctrl_thread() {
    if (ctrl_thread.joinable()) {
        ctrl_thread.join();
    }
}
