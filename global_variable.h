#pragma once

// グローバル変数をまとめておくヘッダファイル．
// 他のヘッダファイルからインクルードされることを想定している．
// あまり褒められた設計ではないが，手早く実装するために便宜上こうしている．

#include "thread_safe_store.h"
#include "thread_safe_vector.h"

constexpr int NUM_PICO = 6;
constexpr int ADC_PER_PICO = 3;

inline ThreadSafeStore g_thread_safe_store;

inline ThreadSafeVector<std::array<std::array<uint16_t, ADC_PER_PICO>, NUM_PICO>> g_pot_values(1);  // ロガー用バッファ（16関節分）
