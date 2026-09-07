cmake_minimum_required(VERSION 3.25)

set(CMAKE_C_STANDARD   17)
set(CMAKE_CXX_STANDARD 20)

add_subdirectory(../ FIO)

project(demo)
add_executable(demo main.cpp)
target_link_libraries(demo FIO)
