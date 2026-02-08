#pragma once

#include <cstdint>

// Raspberry Pi Pico (×6) の値を CAN 経由で取得する．
// Raspberry Pi Pico 1台あたり 3ch の ADC 値を持つ．
// つまり，合計 18ch の ポテンショメータ値を取得する．
// ポテンショメータの値は 12bit ADC 値 (0..4095) である．

void start_pot_thread();
