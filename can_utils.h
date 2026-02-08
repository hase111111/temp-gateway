#pragma once

#include <cstdint>

void can_init(const char* ifname);

// CAN 通信を終了する．
// これを呼んだのち，直ちにプログラムを終了すること．
void can_close();

void send_axis_state(int node_id, uint32_t state);
void send_position(int node_id, float pos);
void send_can_raw(uint32_t can_id, const uint8_t* data, uint8_t dlc);
void send_set_absolute_position(int node_id, float pos);
bool get_position_only(int& node_id, float& pos);
void stop_odrive(int node_id);