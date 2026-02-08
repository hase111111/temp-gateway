#pragma once

#include "system_state.h"

// CTRL UDPコマンドを受信して、システム状態を遷移させる．
// 受信コマンドに応じて、ODriveへキャリブレーション/クローズドループ指令を送る．
// SystemStateは外部から参照できる．

// ↑というかんじのプログラムを変更して，グローバル変数のg_thread_safe_storeを使うようにした．
// key "system_state" に SystemState が保存されているので，それを参照/更新する形にする．
// また，key "cmd" に最新のコマンドが int 型で保存されているので，それに応じて動作する．

// CTRL受信用スレッドを起動/停止する．
void start_ctrl_thread();
void stop_ctrl_thread();

