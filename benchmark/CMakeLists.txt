cmake_minimum_required(VERSION 3.16)
project(pillow_benchmark LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Network)
find_package(ZLIB REQUIRED)

add_executable(bench_server bench_server.cpp)

target_include_directories(bench_server PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../pillowcore
)

target_link_libraries(bench_server
    Qt6::Core
    Qt6::Network
    pillow
    ZLIB::ZLIB
)
