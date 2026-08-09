cmake_minimum_required(VERSION 3.25)

set(CMAKE_C_STANDARD   17)
set(CMAKE_CXX_STANDARD 20)

include(FIO.cmake)

project(demo)
add_executable(demo demo.cpp)
target_link_libraries(demo FIO)
