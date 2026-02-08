#include "logger.h"
#include <sys/stat.h>
#include <array>
#include <atomic>
#include <chrono>
#include <ctime>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

#include "thread_priority.h"
#include "global_variable.h"

constexpr int JOINT_NUM = 16;
constexpr double FLUSH_INTERVAL = 0.3;
const char* LOG_DIR = "logs";

struct LogRow {
    double time;
    float joint[JOINT_NUM];
};

static std::queue<LogRow> log_queue;
static std::mutex log_mutex;
static std::atomic<bool> running{false};
static std::thread writer_thread;

static double now_sec() {
    using clock = std::chrono::steady_clock;
    static const auto t0 = clock::now();
    return std::chrono::duration<double>(clock::now() - t0).count();
}

static void writer_loop() {
	set_fifo_priority(10);
    mkdir(LOG_DIR, 0755);

    auto t = std::chrono::system_clock::now();
    auto tt = std::chrono::system_clock::to_time_t(t);

    std::array<char, 32> ts_buf{};
    std::tm tm_buf{};
    localtime_r(&tt, &tm_buf);
    std::strftime(ts_buf.data(), ts_buf.size(), "%Y%m%d_%H%M%S", &tm_buf);
    const std::string log_path = std::string(LOG_DIR) + "/log_udp_" + ts_buf.data() + ".csv";

    std::ofstream ofs(log_path);
    
    if (!ofs.is_open()) {
    std::cerr << "[LOGGER] file open failed" << std::endl;
	}
    ofs << "time";
    for (int i = 0; i < JOINT_NUM; i++) ofs << ",joint_" << i;
    ofs << "\n";

    std::vector<LogRow> buffer;
    auto last_flush = std::chrono::steady_clock::now();

    while (!g_thread_safe_store.Get<bool>("fin") && (running || !log_queue.empty())) {
        {
            std::lock_guard<std::mutex> lk(log_mutex);
            while (!log_queue.empty()) {
                buffer.push_back(log_queue.front());
                log_queue.pop();
            }
        }

        auto now = std::chrono::steady_clock::now();
        if (!buffer.empty() &&
            std::chrono::duration<double>(now - last_flush).count() >= FLUSH_INTERVAL) {

            for (auto& r : buffer) {
                ofs << std::fixed << std::setprecision(6) << r.time;
                for (float v : r.joint) ofs << "," << v;
                ofs << "\n";
            }
            ofs.flush();
            buffer.clear();
            last_flush = now;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
}

void start_logger_thread() {
	std::cout << "[LOGGER] start / ログ書き込み開始." << std::endl;
    running = true;
    writer_thread = std::thread(writer_loop);
}

void stop_logger_thread() {
    // 書き込みを終了する．(loopが終了する．)
    running = false;

    // 書き込みスレッドの終了を待つ．
    if (writer_thread.joinable()) {
        writer_thread.join();
    }
    std::cout << "[LOGGER] stopped / 終了しました." << std::endl;
}

void logger_push(double time, const float* joint) {
    LogRow r{};
    r.time = time;
    std::memcpy(r.joint, joint, sizeof(float) * JOINT_NUM);

    std::lock_guard<std::mutex> lk(log_mutex);
    if (log_queue.size() < 5000) {
        log_queue.push(r);
    }
}
