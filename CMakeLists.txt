cmake_minimum_required(VERSION 3.30)
project(computer_networks C)

set(CMAKE_C_STANDARD 11)

add_executable(computer_networks lab1/client_1.c)

if(WIN32)
    target_link_libraries(computer_networks Ws2_32)
endif()
