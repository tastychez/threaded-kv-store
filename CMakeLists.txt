cmake_minimum_required(VERSION 3.10)
project(threaded_kv_store)

set(CMAKE_C_STANDARD 99)

# Server executable
add_executable(server server.c)
target_link_libraries(server pthread)

# Client executable
add_executable(client client.c)

