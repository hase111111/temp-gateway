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
    std::array<double, 16> last_send_pos{0.0};

    // ☆ ここの値の正負を変更すると，キャリブレーション時の回転方向が変わる．
    const std::array<float, 16> vec_pm{-1.0f, -1.0f, 1.0f,
                                 -1.0f, -1.0f, -1.0f,
                                 -1.0f, -1.0f, 1.0f,
                                 1.0f, 1.0f, -1.0f,
                                 -1.0f, -1.0f,
                                 -1.0f, -1.0f};

    // ☆ 関節ごとの許容誤差値（ポテンショメータ値の差分）．
    const std::array<float, 16> clam_val{
        50.0f,50.0f,200.0f,
        50.0f,50.0f,200.0f,
        50.0f,50.0f,200.0f,
        50.0f,50.0f,200.0f,
       100.0f,100.0f,
       100.0f,100.0f};

    constexpr int kCalibGroupSize = 3;  // 3関節ずつキャリブレーションを行う.
    constexpr int kCalibEndIndex = 12;  // 12 ~ 15 はキャリブレーション不要（胴体関節）
    int current_group = 0;
    int last_logged_group = -1;

    // 初期化：キャリブレーション不要な関節は最初から完了にする.
    for (int i = 0; i < 16; ++i) {
        calibrated[i] = (i >= kCalibEndIndex);
    }

    while(true) {        
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

        const int group_start = current_group * kCalibGroupSize;
        const int group_end = group_start + kCalibGroupSize;
        if (current_group != last_logged_group && group_start < kCalibEndIndex) {
            std::cout << "[CTRL] Calib group " << current_group
                      << " (" << group_start << ".." << (group_end - 1) << ")"
                      << " start" << std::endl;
            last_logged_group = current_group;
        }

        for (int i = 0; i < 16; ++i) {
            // 基本的にプログラム上ではモータの index は 0 始まりなので注意．
            // CAN に送る段階で +1 する．
            if (calibrated[i]) {
                continue;
            }
            if (i < kCalibEndIndex && (i < group_start || i >= group_end)) {
                continue;
            }

            const int now = pot_values[i / 3][i % 3];
            const float now_rot = static_cast<float>(now) / 4095.0f + 1.5f;
            const int target = POT_DEFAULT_ANGLES[i];
            const float target_rot = static_cast<float>(target) / 4095.0f + 1.5f;

            std::cout << "[CTRL] Joint " << i
                    << ": pot=" << now << " (" << now_rot << " rot), "
                    << "target=" << target << " (" << target_rot << " rot)"
                    << std::endl;
            
            // ポテンショメータ値を目標値に合わせるように ODrive に送信する.
            const float rot_diff = 0.1f;  // 一回に送る回転量の差分(回転速度)
            if (std::abs(target - now) > clam_val[i]) {
                last_send_pos[i] += ((now < target) ? rot_diff : -rot_diff) * vec_pm[i];
                const float send_pos = last_send_pos[i];
                send_position(i + 1, send_pos);
            } else {
                calibrated[i] = true;
            }

        }

        bool group_done = true;
        for (int i = group_start; i < group_end && i < kCalibEndIndex; ++i) {
            if (!calibrated[i]) {
                group_done = false;
                break;
            }
        }
        if (group_done && group_start < kCalibEndIndex) {
            std::cout << "[CTRL] Calib group " << current_group
                      << " (" << group_start << ".." << (group_end - 1) << ")"
                      << " done" << std::endl;
            ++current_group;
        }
        
        std::cout << std::endl << std::endl;

        // 少し待つ.
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    // ODriveに絶対位置として送信する.
    for (int i = 0; i < 16; ++i) {
        send_set_absolute_position(i + 1, 0.0f);
    }

    // 少し待つ.
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));

    std::cout << "[CTRL] Potentiometer zero calibration done. /"
        " ポテンショメータゼロ点キャリブレーションを完了しました." << std::endl;
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

                // ☆ 丹下さんのやつは同時にキャリブレーション始めると不安定になったので，無理だったら待ってみて．
                // std::this_thread::sleep_for(std::chrono::seconds(1));
            }
            
            std::this_thread::sleep_for(std::chrono::seconds(15));
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
