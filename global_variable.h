#pragma once

// グローバル変数をまとめておくヘッダファイル．
// 他のヘッダファイルからインクルードされることを想定している．
// あまり褒められた設計ではないが，手早く実装するために便宜上こうしている．

#include "thread_safe_store.h"
#include "thread_safe_vector.h"

inline ThreadSafeStore g_thread_safe_store;
inline ThreadSafeVector<float> g_logger_buffer(16);  // ロガー用バッファ（16関節分）