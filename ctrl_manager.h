#pragma once

#include <atomic>

// CTRL UDPコマンドを受信して、システム状態を遷移させる．
// 受信コマンドに応じて、ODriveへキャリブレーション/クローズドループ指令を送る．
// SystemStateは外部から参照できる．

enum class SystemState {
    INIT,
    CALIBRATED,
    READY,
    RUN
};

// CTRL受信用スレッドを起動/停止する．
void start_ctrl_thread();
void stop_ctrl_thread();

// 現在のシステム状態を取得する．
SystemState get_system_state();
