cmake_minimum_required(VERSION 3.10)
project(gateway)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_FLAGS "-O2 -Wall")

add_executable(gateway
    main.cpp
    ctrl_manager.cpp
    udj1_handler.cpp
    can_utils.cpp
    logger.cpp
    pot_handler.cpp
)

target_link_libraries(gateway pthread)
