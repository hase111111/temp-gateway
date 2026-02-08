#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <chrono>
#include <cstring>
#include <thread>

#include "udj1_handler.h"

#include "can_utils.h"
#include "ctrl_manager.h"
#include "logger.h"
#include "thread_priority.h"

static double now_time() {
    using clock = std::chrono::steady_clock;
    static const auto t0 = clock::now();
    return std::chrono::duration<double>(clock::now() - t0).count();
}

constexpr int UDP_UDJ1_PORT = 50000;
constexpr int EXPECTED_COUNT = 16;

static const int NODE_ID[EXPECTED_COUNT] = {
    1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16
};

void udj1_loop() {
	set_fifo_priority(80);
    int sock = socket(AF_INET, SOCK_DGRAM, 0);

    sockaddr_in addr{};
    addr.sin_family      = AF_INET;
    addr.sin_port        = htons(UDP_UDJ1_PORT);
    addr.sin_addr.s_addr = INADDR_ANY;
    bind(sock, (sockaddr*)&addr, sizeof(addr));

    uint8_t buf[1024];

    while (true) {
        if (get_system_state() != SystemState::RUN) {
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
            continue;
        }

        ssize_t len = recvfrom(sock, buf, sizeof(buf), 0, nullptr, nullptr);
        if (len < 8 + EXPECTED_COUNT * 4) continue;
        if (std::memcmp(buf, "UDJ1", 4) != 0) continue;

        float* angles = reinterpret_cast<float*>(buf + 8);
		double t = now_time();
		
		logger_push(t, angles);
        for (int i = 0; i < EXPECTED_COUNT; i++) {
            send_position(NODE_ID[i], angles[i]);
        }
    }
}
