#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <fcntl.h>

#include <chrono>
#include <cstring>
#include <cerrno>
#include <iostream>
#include <thread>

#include "udj1_handler.h"

#include "can_utils.h"
#include "ctrl_manager.h"
#include "logger.h"
#include "thread_priority.h"
#include "thread_safe_store.h"

static double now_time() {
    using clock = std::chrono::steady_clock;
    static const auto t0 = clock::now();
    return std::chrono::duration<double>(clock::now() - t0).count();
}

constexpr int UDP_UDJ1_PORT = 50000;
constexpr int EXPECTED_COUNT = 16;

static const int NODE_ID[EXPECTED_COUNT] = {
    1, 2, 3, 4,
    5, 6, 7, 8,
    9, 10, 11, 12,
    13, 14, 15, 16
};

static std::thread udj1_thread;

static void udj1_loop() {
	set_fifo_priority(80);
    const int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        std::cerr << "[UDJ1] socket() failed" << std::endl;
        return;
    }

    sockaddr_in addr{};
    addr.sin_family      = AF_INET;
    addr.sin_port        = htons(UDP_UDJ1_PORT);
    addr.sin_addr.s_addr = INADDR_ANY;
    if (bind(sock, (sockaddr*)&addr, sizeof(addr)) < 0) {
        std::cerr << "[UDJ1] bind() failed" << std::endl;
        close(sock);
        return;
    }

    int flags = fcntl(sock, F_GETFL, 0);
    if (flags < 0 || fcntl(sock, F_SETFL, flags | O_NONBLOCK) < 0) {
        std::cerr << "[UDJ1] fcntl(O_NONBLOCK) failed" << std::endl;
        close(sock);
        return;
    }

    uint8_t buf[1024];

    while (!g_thread_safe_store.Get<bool>("fin")) {
        if (get_system_state() != SystemState::RUN) {
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
            continue;
        }

        ssize_t len = recvfrom(sock, buf, sizeof(buf), 0, nullptr, nullptr);
        if (len < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
                continue;
            }
            std::cerr << "[UDJ1] recvfrom() failed" << std::endl;
            break;
        }
        if (len < 8 + EXPECTED_COUNT * 4) continue;
        if (std::memcmp(buf, "UDJ1", 4) != 0) continue;

        float* angles = reinterpret_cast<float*>(buf + 8);
		double t = now_time();
		
		logger_push(t, angles);
        for (int i = 0; i < EXPECTED_COUNT; i++) {
            send_position(NODE_ID[i], angles[i]);
        }
    }

    close(sock);
}

void start_udj1_thread() {
    std::cout << "[UDJ1] start / UDJ1パケット受信開始." << std::endl;
    
    udj1_thread = std::thread(udj1_loop);
}

void stop_udj1_thread() {
    if (udj1_thread.joinable()) {
        udj1_thread.join();
    }

    std::cout << "[UDJ1] stopped / 終了しました." << std::endl;
}
