#include "ctrl_manager.h"
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <fcntl.h>

#include <chrono>
#include <cstring>
#include <cerrno>
#include <iostream>
#include <thread>

#include "can_utils.h"
#include "thread_safe_store.h"

constexpr int CTRL_PORT = 60000;

constexpr uint32_t AXIS_STATE_FULL_CALIBRATION_SEQUENCE = 3;
constexpr uint32_t AXIS_STATE_CLOSED_LOOP_CONTROL = 8;

static const int NODE_ID[16] = {
    1,2,3,4,
    5,6,7,8,
    9,10,11,12,
    13,14,15,16
};

static std::thread ctrl_thread;

static void ctrl_loop() {
    const int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        std::cerr << "[CTRL] socket() failed" << std::endl;
        return;
    }

    sockaddr_in addr{};
    addr.sin_family      = AF_INET;
    addr.sin_port        = htons(CTRL_PORT);
    addr.sin_addr.s_addr = INADDR_ANY;
    if (bind(sock, (sockaddr*)&addr, sizeof(addr)) < 0) {
        std::cerr << "[CTRL] bind() failed" << std::endl;
        close(sock);
        return;
    }

    const int flags = fcntl(sock, F_GETFL, 0);
    if (flags < 0 || fcntl(sock, F_SETFL, flags | O_NONBLOCK) < 0) {
        std::cerr << "[CTRL] fcntl(O_NONBLOCK) failed" << std::endl;
        close(sock);
        return;
    }

    uint8_t buf[8];

    std::cout << "[CTRL] listening CTRL on " << CTRL_PORT << std::endl;

    while (!g_thread_safe_store.Get<bool>("fin")) {
        const ssize_t len = recvfrom(sock, buf, sizeof(buf), 0, nullptr, nullptr);
        if (len < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
                continue;
            }
            std::cerr << "[CTRL] recvfrom() failed" << std::endl;
            break;
        }
        if (len < 6) { continue; }
        if (std::memcmp(buf, "CTRL", 4) != 0) { continue; }

        // const uint8_t cmd = buf[4];
        const int8_t cmd = static_cast<int8_t>(g_thread_safe_store.Get<int>("cmd"));

        const SystemState state = g_thread_safe_store.Get<SystemState>("system_state");
        if (cmd == 1 && state == SystemState::INIT) {
            std::cout << "[CTRL] Start calibration command received. / キャリブレーション開始コマンドを受信しました." << std::endl;
            for (const auto& id : NODE_ID) {
                send_axis_state(id, AXIS_STATE_FULL_CALIBRATION_SEQUENCE);
            }
            
            std::this_thread::sleep_for(std::chrono::seconds(10));
            g_thread_safe_store.Set<SystemState>("system_state", SystemState::CALIBRATED);
        } else if (cmd == 2 && state == SystemState::CALIBRATED) {
            for (const auto& id : NODE_ID) {
                send_axis_state(id, AXIS_STATE_CLOSED_LOOP_CONTROL);
            }

            g_thread_safe_store.Set<SystemState>("system_state", SystemState::READY);
        } else if (cmd == 6 && state == SystemState::READY) {
            g_thread_safe_store.Set<SystemState>("system_state", SystemState::RUN);
        } else if (cmd == 7 && state == SystemState::RUN) {
            g_thread_safe_store.Set<SystemState>("system_state", SystemState::READY);
        }
    }
    close(sock);
}

void start_ctrl_thread() {
    std::cout << "[CTRL] Start. / コントロールコマンド受信開始." << std::endl;
    
    ctrl_thread = std::thread(ctrl_loop);
}

void stop_ctrl_thread() {
    if (ctrl_thread.joinable()) {
        ctrl_thread.join();
    }

    std::cout << "[CTRL] Stopped. / 終了しました." << std::endl;
}
