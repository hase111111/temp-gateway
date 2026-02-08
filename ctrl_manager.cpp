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
#include "global_variable.h"
#include "constants.h"


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

// モータの0点キャリブレーションを行う.
static void calibrate_zero_position() {
    std::cout << "[CTRL] Potentiometer zero calibration command received. / ポテンショメータゼロ点キャリブレーションを行います．" << std::endl;

    // 各関節に対して，ポテンショメータ値を送信する.
    std::array<bool, 16> calibrated{};
    std::array<float, 16> last_send_pos{};
    std::array<float, 16> last_delta{};
    std::array<bool, 16> has_last{};

    // 最初はすべて未キャリブレーション状態にする.
    for (auto& v : calibrated) {
        v = false;
    }
    for (auto& v : has_last) {
        v = false;
    }

    int c = 0;
    while(true) {
        // 20秒で落とす
        if (c++ > 200) {
            std::cout << "[CTRL] Potentiometer zero calibration timeout. / ポテンショメータゼロ点キャリブレーションがタイムアウトしました．" << std::endl;
            break;
        }
        
        bool all_done = true;
        for (const auto v : calibrated) {
            if (!v) {
                all_done = false;
                break;
            }
        }

        if (all_done) {
            break;
        }

        const auto pot_values = g_pot_values.Back();

        for (int i = 0; i < 16; ++i) {
            if (calibrated[i] || i != 2) {
                continue;
            }

            // debug用
            const int now = pot_values[i / 3][i % 3];
            const float now_rot = static_cast<float>(now) / 4095.0f + 1.5f;
            const int target = POT_DEFAULT_ANGLES[i];
            const float target_rot = static_cast<float>(target) / 4095.0f + 1.5f;
            float current_rot{};
            const bool has_pos = get_position_only(i, current_rot);

            std::cout << "[CTRL] Joint " << i
                    << ": pot=" << now << " (" << now_rot << " rot), "
                    << "odrive=" << current_rot << " rot, "
                    << "target=" << target << " (" << target_rot << " rot)"
                    << std::endl;
            
            // ポテンショメータ値を目標値に合わせるようにODriveに送信する.
            const float rot_diff = 0.1f;
            if (std::abs(target - now) > 10) {
                float send_pos = 0.0f;
                if (has_pos) {
                    send_pos = (now < target) ?
                        current_rot + rot_diff :
                        current_rot - rot_diff;
                    last_delta[i] = send_pos - current_rot;
                    last_send_pos[i] = send_pos;
                    has_last[i] = true;
                } else if (has_last[i]) {
                    send_pos = last_send_pos[i] + last_delta[i];
                    last_send_pos[i] = send_pos;
                } else {
                    continue;
                }

                send_position(NODE_ID[i], send_pos);
                std::cout << "[CTRL]   -> sending " << send_pos << " rot to ODrive." << std::endl;
            } else {
                calibrated[i] = true;
            }
        }

        // 少し待つ.
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    // ODriveに絶対位置として送信する.
    for (int i = 0; i < 16; ++i) {
        send_set_absolute_position(NODE_ID[i], 0.0f);
    }

    std::cout << "[CTRL] Potentiometer zero calibration done. / ポテンショメータゼロ点キャリブレーションを完了しました." << std::endl;
}

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
        // const ssize_t len = recvfrom(sock, buf, sizeof(buf), 0, nullptr, nullptr);
        // if (len < 0) {
        //     if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) {
        //         std::this_thread::sleep_for(std::chrono::milliseconds(1));
        //         continue;
        //     }
        //     std::cerr << "[CTRL] recvfrom() failed" << std::endl;
        //     break;
        // }
        // if (len < 6) { continue; }
        // if (std::memcmp(buf, "CTRL", 4) != 0) { continue; }

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
            std::cout << "[CTRL] Calibration sequence sent. / キャリブレーションシーケンスを送信しました." << std::endl;
        } else if (cmd == 2 && state == SystemState::CALIBRATED) {
            // 閉ループ開始にする．
            for (const auto& id : NODE_ID) {
                send_axis_state(id, AXIS_STATE_CLOSED_LOOP_CONTROL);
            }
        } else if (cmd == 3 && state == SystemState::CALIBRATED) {
            // ここでポテンショメータ値をゼロ点キャリブレーションする．
            calibrate_zero_position();

            // READY状態にする．
            g_thread_safe_store.Set<SystemState>("system_state", SystemState::READY);
        } else if (cmd == 6 && state == SystemState::READY) {
            g_thread_safe_store.Set<SystemState>("system_state", SystemState::RUN);
        } else if (cmd == 7 && state == SystemState::RUN) {
            g_thread_safe_store.Set<SystemState>("system_state", SystemState::READY);
        } else if (cmd == 8) {
            for (const auto& id : NODE_ID) {
                stop_odrive(id);
            }
            g_thread_safe_store.Set<SystemState>("system_state", SystemState::INIT);
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
