#include <iostream>

#include "can_utils.h"
#include "ctrl_manager.h"
#include "logger.h"
#include "pot_handler.h"
#include "udj1_handler.h"
#include "thread_safe_store.h"
#include "stdin_writer.h"

int main() {
    std::cout << "[GW] Gateway Start. / ゲートウエイマイコンを起動します." << std::endl;
    std::cout << "[GW] Start threads. / 通信スレッドを起動します." << std::endl;

    // まず，CAN通信を初期化.
    can_init("can0");

    g_thread_safe_store.Set<bool>("fin", false);
    g_thread_safe_store.Set<bool>("cal_end", false);

    // その後, 各種スレッドを起動.
    start_pot_thread();
    start_ctrl_thread();
	start_logger_thread();
    start_udj1_thread();

    std::cout << "[GW] All threads started. / 全ての通信スレッドを起動しました." << std::endl;
    // udj1_loop();
    StdinWriter{}.Run();
    
    // スレッドの終了を待つ.
    std::cout << "[GW] Stopping threads. / 通信スレッドを終了します." << std::endl;
    stop_udj1_thread();
    stop_ctrl_thread();
	stop_logger_thread();
    stop_pot_thread();

    // 終了処理.
    std::cout << "[GW] Stopping CAN communication. / CAN通信を終了します." << std::endl;
    can_close();

    std::cout << "[GW] Gateway stopped. / ゲートウエイマイコンを終了しました." << std::endl;
    return 0;
}
