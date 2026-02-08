#include "can_utils.h"
#include <cstring>
#include <unistd.h>
#include <linux/can.h>
#include <linux/can/raw.h>
#include <net/if.h>
#include <sys/ioctl.h>

static int can_sock = -1;

constexpr uint16_t CMD_SET_INPUT_POS = 0x00C;
constexpr uint16_t CMD_SET_AXIS_REQUESTED_STATE = 0x007;

void can_init(const char* ifname) {
    can_sock = socket(PF_CAN, SOCK_RAW, CAN_RAW);

    ifreq ifr{};
    std::strcpy(ifr.ifr_name, ifname);
    ioctl(can_sock, SIOCGIFINDEX, &ifr);

    sockaddr_can addr{};
    addr.can_family  = AF_CAN;
    addr.can_ifindex = ifr.ifr_ifindex;

    bind(can_sock, (sockaddr*)&addr, sizeof(addr));
}

void can_close() {
    if (can_sock >= 0) close(can_sock);
}

void send_axis_state(int node_id, uint32_t state) {
    can_frame f{};
    f.can_id  = (node_id << 5) | CMD_SET_AXIS_REQUESTED_STATE;
    f.can_dlc = 4;
    std::memcpy(f.data, &state, 4);
    write(can_sock, &f, sizeof(f));
}

void send_position(int node_id, float pos) {
    can_frame f{};
    f.can_id  = (node_id << 5) | CMD_SET_INPUT_POS;
    f.can_dlc = 4;
    std::memcpy(f.data, &pos, 4);
    write(can_sock, &f, sizeof(f));
}

void send_can_raw(uint32_t can_id, const uint8_t* data, uint8_t dlc) {
    struct can_frame f{};
    f.can_id  = can_id;
    f.can_dlc = dlc;
    std::memcpy(f.data, data, dlc);
    write(can_sock, &f, sizeof(f));
}
