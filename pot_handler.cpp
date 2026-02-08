#include "pot_handler.h"
#include <netinet/in.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <linux/can.h>
#include <linux/can/raw.h>
#include <net/if.h>

#include <chrono>
#include <cstring>
#include <iostream>
#include <thread>
#include <array>
#include <vector>

#include "global_variable.h"

// ===== UDP =====
constexpr int POT_RX_PORT = 50010;
constexpr int POT_TX_PORT = 50011;

// ===== CAN =====
constexpr uint32_t CAN_RESP_BASE   = 0x301;   // 0x301 - 0x306
// constexpr int      NUM_PICO        = 6;
// constexpr int      ADC_PER_PICO    = 3;

// ===== Packet =====
static constexpr char POTQ_MAGIC[4] = {'P','O','T','Q'};
static constexpr char POTR_MAGIC[4] = {'P','O','T','R'};

// ===== Internal Variables =====
static std::thread pot_thread{};

// ======================================================

static int open_can_socket(const char* ifname) {
    int s = socket(PF_CAN, SOCK_RAW, CAN_RAW);
    if (s < 0) {
        std::cerr << "[POT] socket(PF_CAN) failed" << std::endl;
        return -1;
    }

    ifreq ifr{};
    std::strcpy(ifr.ifr_name, ifname);
    if (ioctl(s, SIOCGIFINDEX, &ifr) < 0) {
        std::cerr << "[POT] ioctl(SIOCGIFINDEX) failed" << std::endl;
        close(s);
        return -1;
    }

    sockaddr_can addr{};
    addr.can_family  = AF_CAN;
    addr.can_ifindex = ifr.ifr_ifindex;

    if (bind(s, (sockaddr*)&addr, sizeof(addr)) < 0) {
        std::cerr << "[POT] bind(CAN) failed" << std::endl;
        close(s);
        return -1;
    }
    return s;
}

// ======================================================

static void pot_loop() {
    // ----- CAN socket -----
    const int can_sock = open_can_socket("can0");
    if (can_sock < 0) {
        return;
    }

    int flags = fcntl(can_sock, F_GETFL, 0);
    if (flags < 0 || fcntl(can_sock, F_SETFL, flags | O_NONBLOCK) < 0) {
        std::cerr << "[POT] fcntl(CAN O_NONBLOCK) failed" << std::endl;
        close(can_sock);
        return;
    }

    // ----- UDP socket -----
    const int udp_sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (udp_sock < 0) {
        std::cerr << "[POT] socket(AF_INET) failed" << std::endl;
        close(can_sock);
        return;
    }

    sockaddr_in rx_addr{};
    rx_addr.sin_family      = AF_INET;
    rx_addr.sin_port        = htons(POT_RX_PORT);
    rx_addr.sin_addr.s_addr = INADDR_ANY;
    if (bind(udp_sock, (sockaddr*)&rx_addr, sizeof(rx_addr)) < 0) {
        std::cerr << "[POT] bind(UDP) failed" << std::endl;
        close(udp_sock);
        close(can_sock);
        return;
    }

    int flags_ = fcntl(udp_sock, F_GETFL, 0);
    if (flags_ < 0 || fcntl(udp_sock, F_SETFL, flags_ | O_NONBLOCK) < 0) {
        std::cerr << "[POT] fcntl(O_NONBLOCK) failed" << std::endl;
        close(udp_sock);
        close(can_sock);
        return;
    }

    std::cout << "[POT] listening POTQ on " << POT_RX_PORT << std::endl;

    uint8_t buf[1500];
    std::array<std::array<uint16_t, ADC_PER_PICO>, NUM_PICO> latest{};
    auto disp_until = std::chrono::steady_clock::time_point::min();

    while (!g_thread_safe_store.Get<bool>("fin")) {
        if (const auto disp = g_thread_safe_store.TryGet<int>("pot")) {
            if (*disp > 0) {
                disp_until = std::chrono::steady_clock::now()
                           + std::chrono::seconds(*disp);
                g_thread_safe_store.Set<int>("pot", 0);
            }
        }

        sockaddr_in src{};
        socklen_t slen = sizeof(src);
        bool has_request = false;
        uint8_t group_id = 0;
        uint8_t req_id = 0;

        const ssize_t len = recvfrom(udp_sock, buf, sizeof(buf), 0,
                               (sockaddr*)&src, &slen);
        if (len >= 6 && std::memcmp(buf, POTQ_MAGIC, 4) == 0) {
            group_id = buf[4];
            req_id = buf[5];
            has_request = true;
        } else if (len < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
            std::cerr << "[POT] recvfrom() failed" << std::endl;
            break;
        }

        for (;;) {
            can_frame rx{};
            ssize_t r = read(can_sock, &rx, sizeof(rx));
            if (r < 0) {
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    break;
                }
                std::cerr << "[POT] read(CAN) failed" << std::endl;
                break;
            }
            if (r == 0) {
                break;
            }

            if (rx.can_id < CAN_RESP_BASE ||
                rx.can_id >= CAN_RESP_BASE + NUM_PICO) {
                continue;
            }

            const int pico = rx.can_id - CAN_RESP_BASE;  // 0..5
            const int pair_count = rx.can_dlc / 2;
            const int limit = (pair_count < ADC_PER_PICO) ? pair_count : ADC_PER_PICO;

            const bool should_print = std::chrono::steady_clock::now() < disp_until;
            if (should_print) {
                std::cout << "[POT] can " << std::hex << rx.can_id << std::dec << ":";
            }
            for (int i = 0; i < limit; ++i) {
                uint16_t adc = rx.data[i * 2] | (rx.data[i * 2 + 1] << 8);
                latest[pico][i] = adc;
                if (should_print) {
                    std::cout << " ch" << (pico * ADC_PER_PICO + i)
                              << "=" << adc;
                }
            }
            if (should_print) {
                std::cout << std::endl;
            }

            // グローバル変数にも保存しておく．
            g_pot_values.PushBack(latest);
        }

        if (!has_request) {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            continue;
        }

        // ===== build POTR =====
        std::vector<uint8_t> pkt;
        pkt.resize(7 + NUM_PICO * ADC_PER_PICO * 3);

        std::memcpy(&pkt[0], POTR_MAGIC, 4);
        pkt[4] = group_id;
        pkt[5] = req_id;
        pkt[6] = NUM_PICO * ADC_PER_PICO;

        size_t off = 7;
        for (int pico = 0; pico < NUM_PICO; ++pico) {
            for (int ch = 0; ch < ADC_PER_PICO; ++ch) {
                const uint8_t out_ch = pico * ADC_PER_PICO + ch;
                const uint16_t adc = latest[pico][ch];
                pkt[off++] = out_ch;
                pkt[off++] = adc & 0xFF;
                pkt[off++] = (adc >> 8) & 0xFF;
            }
        }

        sockaddr_in tx{};
        tx.sin_family = AF_INET;
        tx.sin_port   = htons(POT_TX_PORT);
        tx.sin_addr   = src.sin_addr;

        sendto(udp_sock, pkt.data(), pkt.size(), 0,
               (sockaddr*)&tx, sizeof(tx));

        std::cout << "[POT] reply " << (NUM_PICO * ADC_PER_PICO)
                  << " samples" << std::endl;
    }

    close(udp_sock);
    close(can_sock);
}

// ======================================================

void start_pot_thread()
{
    // ポテンショメータの読み取りスレッドを起動.
    pot_thread = std::thread(pot_loop);
}

void stop_pot_thread() {
    if (pot_thread.joinable()) {
        pot_thread.join();
    }
}
