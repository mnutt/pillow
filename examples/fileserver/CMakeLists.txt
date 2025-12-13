set(CMAKE_AUTOMOC ON)

add_executable(fileserver fileserver.cpp)

find_package(Qt6 COMPONENTS Core Network Test REQUIRED)

target_link_libraries(fileserver
    pillow
    Qt6::Core
    Qt6::Network
    Qt6::Test
)
