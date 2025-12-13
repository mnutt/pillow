set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

add_executable(simplessl
    simplessl.cpp
    simplessl.qrc
)

find_package(Qt6 COMPONENTS Core Network Test REQUIRED)

target_link_libraries(simplessl
    pillow
    Qt6::Core
    Qt6::Network
    Qt6::Test
)
