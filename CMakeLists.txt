cmake_minimum_required(VERSION 4.1.0)
add_executable(revShell main.cpp)
project(REVERSESHELL)
target_link_libraries(revShell PRIVATE ws2_32)
