cmake_minimum_required(VERSION 3.12)
project(GuSh)

set(CMAKE_CXX_STANDARD 17)

add_executable(GuSh main.cpp Commands.h Commands.cpp Lexer.cpp)

# Try to find the readline library using CMake's FindReadline module
find_package(Readline)

if(Readline_FOUND)
    # If the readline library is found, link it to the target (Linux)
    target_include_directories(GuSh PUBLIC ${Readline_INCLUDE_DIR})
    target_link_libraries(GuSh ${Readline_LIBRARY})
else()
    # If the readline library is not found, then use pkg-config (Mainly for BSD)
    find_package(PkgConfig)
    pkg_check_modules(RL REQUIRED readline)
    target_compile_options(GuSh PUBLIC ${RL_CFLAGS_OTHER})
    target_include_directories(GuSh PUBLIC ${RL_INCLUDE_DIRS})
    target_link_directories(GuSh PUBLIC ${RL_LIBRARY_DIRS})
    target_link_libraries(GuSh ${RL_LIBRARIES})
endif()
