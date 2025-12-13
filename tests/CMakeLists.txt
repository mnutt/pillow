set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)
enable_testing()

# Common settings for all tests
set(TEST_INCLUDE_DIRS . ../pillowcore)
find_package(Qt6 COMPONENTS Core Network Test REQUIRED)

# Helper function to create a test executable
function(add_pillow_test test_name test_source)
    add_executable(${test_name} ${test_source} ${ARGN})
    target_include_directories(${test_name} PRIVATE ${TEST_INCLUDE_DIRS})
    target_link_libraries(${test_name} 
        pillow
        Qt6::Core 
        Qt6::Network 
        Qt6::Test
    )
    if(UNIX)
        target_link_libraries(${test_name} z)
    endif()
    add_test(NAME ${test_name} COMMAND ${test_name})
endfunction()

# Create separate test executables
add_pillow_test(test_httpclient tst_httpclient.cpp tests.qrc)
add_pillow_test(test_httprequestwriter tst_httprequestwriter.cpp)
add_pillow_test(test_httpresponseparser tst_httpresponseparser.cpp)
add_pillow_test(test_networkaccessmanager tst_networkaccessmanager.cpp)
add_pillow_test(test_bytearrayhelpers tst_bytearrayhelpers.cpp)
add_pillow_test(test_httpheader tst_httpheader.cpp)
add_pillow_test(test_httphandler tst_httphandler.cpp httphandlerbase.cpp httphandlerbase.h)
add_pillow_test(test_httphandlerfile tst_httphandlerfile.cpp httphandlerbase.cpp httphandlerbase.h)
add_pillow_test(test_httphandlersimplerouter tst_httphandlersimplerouter.cpp httphandlerbase.cpp httphandlerbase.h)
add_pillow_test(test_httphandlerproxy tst_httphandlerproxy.cpp httphandlerbase.cpp httphandlerbase.h)
add_pillow_test(test_httpconnection_tcp tst_httpconnection_tcp.cpp httpconnectionbase.cpp httpconnectionbase.h)
add_pillow_test(test_httpconnection_ssl tst_httpconnection_ssl.cpp httpconnectionbase.cpp httpconnectionbase.h tests.qrc)
add_pillow_test(test_httpconnection_local tst_httpconnection_local.cpp httpconnectionbase.cpp httpconnectionbase.h)
add_pillow_test(test_httpconnection_buffer tst_httpconnection_buffer.cpp httpconnectionbase.cpp httpconnectionbase.h)
add_pillow_test(test_httpserver tst_httpserver.cpp httpserverbase.cpp httpserverbase.h)
add_pillow_test(test_httplocalserver tst_httplocalserver.cpp httpserverbase.cpp httpserverbase.h)
add_pillow_test(test_httpsserver tst_httpsserver.cpp httpserverbase.cpp httpserverbase.h tests.qrc)
add_pillow_test(test_httphelpers tst_httphelpers.cpp)

# Optional: Create a custom target to run all tests
add_custom_target(test_all
    COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure
    DEPENDS test_httpclient test_httprequestwriter test_httpresponseparser test_networkaccessmanager test_bytearrayhelpers test_httpheader test_httphandler test_httphandlerfile test_httphandlersimplerouter test_httphandlerproxy test_httpconnection_tcp test_httpconnection_ssl test_httpconnection_local test_httpconnection_buffer test_httpserver test_httplocalserver test_httpsserver test_httphelpers
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)

# Generate a test runner script
if(UNIX)
    file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/run_all_tests.sh
"#!/bin/bash
# Auto-generated test runner script
set -e

echo \"Running Pillow Test Suite\"
echo \"========================\"

# Run all tests using ctest
cd \"${CMAKE_CURRENT_BINARY_DIR}\"
ctest --output-on-failure

echo \"\"
echo \"Test suite completed!\"
")
    file(CHMOD ${CMAKE_CURRENT_BINARY_DIR}/run_all_tests.sh PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)
else()
    file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/run_all_tests.bat
"@echo off
REM Auto-generated test runner script
echo Running Pillow Test Suite
echo ========================

REM Run all tests using ctest
cd /d \"${CMAKE_CURRENT_BINARY_DIR}\"
ctest --output-on-failure

echo.
echo Test suite completed!
")
endif()

# Create a single test runner executable that runs all tests
add_executable(tests tests_main.cpp)
target_link_libraries(tests Qt6::Core)
add_dependencies(tests
    test_httpclient test_httprequestwriter test_httpresponseparser
    test_networkaccessmanager test_bytearrayhelpers test_httpheader
    test_httphandler test_httphandlerfile test_httphandlersimplerouter
    test_httphandlerproxy test_httpconnection_tcp test_httpconnection_ssl
    test_httpconnection_local test_httpconnection_buffer test_httpserver
    test_httplocalserver test_httpsserver test_httphelpers
)