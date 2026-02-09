#include "encoder_logger.h"

#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <unistd.h>
#include <fcntl.h>
#include <linux/can.h>
#include <linux/can/raw.h>
#include <net/if.h>

#include <array>
#include <chrono>
#include <cstring>
#include <fstream>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

#include "global_variable.h"
#include "system_state.h"
#include "time_utils.h"

namespace {
constexpr const char* kLogDir = "logs";
constexpr uint16_t kCmdGetEncoderEstimates = 0x009;

struct EncoderSample {
    double time;
    uint8_t node_id;
    float pos;
    float vel;
};

static std::thread encoder_thread;
static std::vector<EncoderSample> samples;

int open_can_socket(const char* ifname) {
    int s = socket(PF_CAN, SOCK_RAW, CAN_RAW);
    if (s < 0) {
        std::cerr << "[ENC] socket(PF_CAN) failed" << std::endl;
        return -1;
    }

    ifreq ifr{};
    std::strncpy(ifr.ifr_name, ifname, IFNAMSIZ - 1);
    if (ioctl(s, SIOCGIFINDEX, &ifr) < 0) {
        std::cerr << "[ENC] ioctl(SIOCGIFINDEX) failed" << std::endl;
        close(s);
        return -1;
    }

    sockaddr_can addr{};
    addr.can_family = AF_CAN;
    addr.can_ifindex = ifr.ifr_ifindex;

    if (bind(s, (sockaddr*)&addr, sizeof(addr)) < 0) {
        std::cerr << "[ENC] bind(CAN) failed" << std::endl;
        close(s);
        return -1;
    }

    int flags = fcntl(s, F_GETFL, 0);
    if (flags < 0 || fcntl(s, F_SETFL, flags | O_NONBLOCK) < 0) {
        std::cerr << "[ENC] fcntl(O_NONBLOCK) failed" << std::endl;
        close(s);
        return -1;
    }

    return s;
}

void encoder_loop() {
    const int sock = open_can_socket("can0");
    if (sock < 0) {
        return;
    }

    samples.clear();
    samples.reserve(10000);

    while (!g_thread_safe_store.Get<bool>("fin")) {
        if (g_thread_safe_store.Get<SystemState>("system_state") != SystemState::RUN) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        }

        can_frame frame{};
        bool received_any = false;

        for (;;) {
            ssize_t n = read(sock, &frame, sizeof(frame));
            if (n < 0) {
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    break;
                }
                std::cerr << "[ENC] read(CAN) failed" << std::endl;
                close(sock);
                return;
            }
            if (n == 0) {
                break;
            }

            received_any = true;
            const uint16_t cmd = frame.can_id & 0x1F;
            if (cmd != kCmdGetEncoderEstimates || frame.can_dlc != 8) {
                continue;
            }

            const uint8_t node_id = static_cast<uint8_t>((frame.can_id >> 5) & 0x3F);
            float pos = 0.0f;
            float vel = 0.0f;
            std::memcpy(&pos, &frame.data[0], 4);
            std::memcpy(&vel, &frame.data[4], 4);

            const double time = now_time_sec();
            samples.push_back({time, node_id, pos, vel});
        }

        if (!received_any) {
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }
    }

    close(sock);
}

std::string make_log_path() {
    mkdir(kLogDir, 0755);

    auto t = std::chrono::system_clock::now();
    auto tt = std::chrono::system_clock::to_time_t(t);
    std::array<char, 32> ts_buf{};
    std::tm tm_buf{};
    localtime_r(&tt, &tm_buf);
    std::strftime(ts_buf.data(), ts_buf.size(), "%Y%m%d_%H%M%S", &tm_buf);

    return std::string(kLogDir) + "/encoder_" + ts_buf.data() + ".csv";
}

void write_log() {
    if (samples.empty()) {
        std::cout << "[ENC] no samples to write" << std::endl;
        return;
    }

    const std::string path = make_log_path();
    std::ofstream ofs(path);
    if (!ofs.is_open()) {
        std::cerr << "[ENC] file open failed" << std::endl;
        return;
    }

    ofs << "time,node_id,pos,vel\n";
    for (const auto& s : samples) {
        ofs << s.time << ','
            << static_cast<int>(s.node_id) << ','
            << s.pos << ','
            << s.vel << '\n';
    }

    std::cout << "[ENC] wrote " << samples.size() << " samples to " << path << std::endl;
}
}  // namespace

void start_encoder_logger_thread() {
    std::cout << "[ENC] start / encoder logging start." << std::endl;
    encoder_thread = std::thread(encoder_loop);
}

void stop_encoder_logger_thread() {
    if (encoder_thread.joinable()) {
        encoder_thread.join();
    }
    write_log();
    std::cout << "[ENC] stopped / encoder logging stopped." << std::endl;
}
