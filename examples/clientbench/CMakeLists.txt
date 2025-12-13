set(CMAKE_AUTOMOC ON)

add_executable(clientbench clientbench.cpp)

find_package(Qt6 COMPONENTS Core Network Test Svg REQUIRED)
find_package(ZLIB REQUIRED)

target_link_libraries(clientbench
    pillow
    Qt6::Core
    Qt6::Network
    Qt6::Test
    Qt6::Svg
    ZLIB::ZLIB
)
