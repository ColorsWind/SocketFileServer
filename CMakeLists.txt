cmake_minimum_required(VERSION 3.19)
project(SocketFileServer)

set(CMAKE_CXX_STANDARD 11)

add_executable(SocketServer server.cpp common.cpp common.h)
add_executable(SocketClient client.cpp common.cpp common.h)
target_link_libraries(SocketServer ws2_32)
target_link_libraries(SocketClient ws2_32)