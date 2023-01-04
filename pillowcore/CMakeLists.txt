set(CMAKE_AUTOMOC ON)

add_library(pillow
    STATIC
	parser/parser.c
	parser/http_parser.c
	HttpServer.cpp
	HttpHandler.cpp
	HttpHelpers.cpp
	HttpsServer.cpp
	HttpHandlerSimpleRouter.cpp
	HttpConnection.cpp
	HttpHandlerProxy.cpp
	HttpClient.cpp
	HttpHeader.cpp
)

target_include_directories(pillow PUBLIC .)

find_package(Qt6 COMPONENTS Network REQUIRED)
target_link_libraries(pillow Qt6::Network)
