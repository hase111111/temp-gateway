#include "pot_handler.h"
#include <netinet/in.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <unistd.h>
#include <linux/can.h>
#include <linux/can/raw.h>
#include <net/if.h>

#include <chrono>
#include <cstring>
#include <iostream>
#include <thread>
#include <vector>

// ===== UDP =====
constexpr int POT_RX_PORT = 50010;
constexpr int POT_TX_PORT = 50011;

// ===== CAN =====
constexpr uint32_t CAN_REQ_ID      = 0x123;   // broadcast
constexpr uint32_t CAN_RESP_BASE   = 0x301;   // 0x301 - 0x306
constexpr int      NUM_PICO        = 6;
constexpr int      ADC_PER_PICO    = 3;

// ===== Packet =====
static constexpr char POTQ_MAGIC[4] = {'P','O','T','Q'};
static constexpr char POTR_MAGIC[4] = {'P','O','T','R'};

// ======================================================

static int open_can_socket(const char* ifname)
{
    int s = socket(PF_CAN, SOCK_RAW, CAN_RAW);

    ifreq ifr{};
    std::strcpy(ifr.ifr_name, ifname);
    ioctl(s, SIOCGIFINDEX, &ifr);

    sockaddr_can addr{};
    addr.can_family  = AF_CAN;
    addr.can_ifindex = ifr.ifr_ifindex;

    const auto _ = bind(s, (sockaddr*)&addr, sizeof(addr));
    return s;
}

// ======================================================

static void pot_loop()
{
    // ----- CAN socket -----
    int can_sock = open_can_socket("can0");

    // ----- UDP socket -----
    int udp_sock = socket(AF_INET, SOCK_DGRAM, 0);

    sockaddr_in rx_addr{};
    rx_addr.sin_family      = AF_INET;
    rx_addr.sin_port        = htons(POT_RX_PORT);
    rx_addr.sin_addr.s_addr = INADDR_ANY;
    const auto _ = bind(udp_sock, (sockaddr*)&rx_addr, sizeof(rx_addr));

    std::cout << "[POT] listening POTQ on " << POT_RX_PORT << std::endl;

    uint8_t buf[1500];

    while (true) {
        sockaddr_in src{};
        socklen_t slen = sizeof(src);

        ssize_t len = recvfrom(udp_sock, buf, sizeof(buf), 0,
                               (sockaddr*)&src, &slen);
        if (len < 6) continue;
        if (std::memcmp(buf, POTQ_MAGIC, 4) != 0) continue;

        uint8_t group_id = buf[4];
        uint8_t req_id   = buf[5];

        // ===== CAN request (broadcast) =====
        can_frame req{};
        req.can_id  = CAN_REQ_ID;
        req.can_dlc = 1;
        req.data[0] = req_id;
        write(can_sock, &req, sizeof(req));

        // ===== collect responses =====
        struct Sample {
            uint8_t ch;
            uint16_t adc;
        };
        std::vector<Sample> samples;

        auto deadline = std::chrono::steady_clock::now()
                      + std::chrono::milliseconds(50);

        while (std::chrono::steady_clock::now() < deadline) {
            can_frame rx{};
            ssize_t r = read(can_sock, &rx, sizeof(rx));
            if (r <= 0) continue;

            if (rx.can_id < CAN_RESP_BASE ||
                rx.can_id >= CAN_RESP_BASE + NUM_PICO)
                continue;

            int pico = rx.can_id - CAN_RESP_BASE; // 0..5

            for (int i = 0; i + 1 < rx.can_dlc; i += 2) {
                uint16_t adc =
                    rx.data[i] | (rx.data[i+1] << 8);

                uint8_t ch = pico * ADC_PER_PICO + (i / 2);
                samples.push_back({ch, adc});
            }
        }

        // ===== build POTR =====
        std::vector<uint8_t> pkt;
        pkt.resize(7 + samples.size() * 3);

        std::memcpy(&pkt[0], POTR_MAGIC, 4);
        pkt[4] = group_id;
        pkt[5] = req_id;
        pkt[6] = samples.size();

        size_t off = 7;
        for (auto& s : samples) {
            pkt[off++] = s.ch;
            pkt[off++] = s.adc & 0xFF;
            pkt[off++] = (s.adc >> 8) & 0xFF;
        }

        sockaddr_in tx{};
        tx.sin_family = AF_INET;
        tx.sin_port   = htons(POT_TX_PORT);
        tx.sin_addr   = src.sin_addr;

        sendto(udp_sock, pkt.data(), pkt.size(), 0,
               (sockaddr*)&tx, sizeof(tx));

        std::cout << "[POT] reply " << samples.size()
                  << " samples" << std::endl;
    }
}

// ======================================================

void start_pot_thread()
{
    std::thread(pot_loop).detach();
}
