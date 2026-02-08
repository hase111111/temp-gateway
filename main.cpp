#include <iostream>

#include "can_utils.h"
#include "ctrl_manager.h"
#include "logger.h"
#include "pot_handler.h"
#include "udj1_handler.h"

int main() {
    std::cout << "[GW] Gateway Start. / ゲートウエイマイコンを起動します." << std::endl;
    std::cout << "[GW] Start threads. / 通信スレッドを起動します." << std::endl;

    // まず，CAN通信を初期化.
    can_init("can0");

    // その後, 各種スレッドを起動.
    start_pot_thread();
    start_ctrl_thread();

    std::cout << "[GW] CTRL ready, waiting commands" << std::endl;
	logger_start();
    udj1_loop();
    
	logger_stop();

    // 終了処理.
    std::cout << "[GW] Stopping CAN communication. / CAN通信を終了します." << std::endl;
    can_close();

    std::cout << "[GW] Gateway stopped. / ゲートウエイマイコンを終了しました." << std::endl;
    return 0;
}
