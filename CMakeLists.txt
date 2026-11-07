cmake_minimum_required(VERSION 3.16)
project(port_scanner_cpp LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(port_scanner src/main.cpp)

if(WIN32)
    target_link_libraries(port_scanner ws2_32)
endif()

find_package(Threads REQUIRED)
target_link_libraries(port_scanner Threads::Threads)
