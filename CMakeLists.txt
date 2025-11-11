cmake_minimum_required(VERSION 3.30)
project(computer_networks C)

set(CMAKE_C_STANDARD 11)

add_executable(computer_networks lab2/3_server.c)

add_executable(server lab3/4_server.c)

add_executable(peer lab3/4_peer.c)

add_executable(peer_win lab3/4_peer_win.c)
if(WIN32)
    target_link_libraries(client_windows PRIVATE Ws2_32)
endif()
