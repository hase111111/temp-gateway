cmake_minimum_required(VERSION 3.10)
project(gateway)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_FLAGS "-O2 -Wall -Wno-unused-result -Wno-unused-function -Wno-unused-variable -Wno-psabi")

file(GLOB_RECURSE PROJECT_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.h"
)

list(FILTER PROJECT_SOURCES EXCLUDE REGEX "/build/")

add_executable(gateway ${PROJECT_SOURCES})

target_link_libraries(gateway pthread)
