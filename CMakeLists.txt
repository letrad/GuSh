cmake_minimum_required(VERSION 3.12)
project(GuSh)

set(CMAKE_CXX_STANDARD 17)

add_executable(GuSh main.cpp Commands.h Commands.cpp)

target_link_libraries(GuSh readline)
