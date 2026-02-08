#include <iostream>
#include <thread>

#include "can_utils.h"
#include "ctrl_manager.h"
#include "logger.h"
#include "pot_handler.h"
#include "udj1_handler.h"

int main() {
    std::cout << "[GW] start" << std::endl;

    can_init("can0");
    start_pot_thread();
    start_ctrl_thread();

    std::cout << "[GW] CTRL ready, waiting commands" << std::endl;
	logger_start();
    udj1_loop();
	logger_stop();
    can_close();
    return 0;
}
