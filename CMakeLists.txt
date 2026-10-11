
cmake_minimum_required(VERSION 3.20)

project(InternetRoutingSimulator LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

enable_testing()

add_library(routing_core
    src/network.cpp
)

target_include_directories(routing_core
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
)

add_executable(routing_simulator
    src/main.cpp
)

target_link_libraries(routing_simulator PRIVATE routing_core)

add_executable(routing_tests
    tests/routing_tests.cpp
)

target_link_libraries(routing_tests PRIVATE routing_core)

add_test(
    NAME RoutingTests
    COMMAND routing_tests
)
add_executable(tcp_demo
    src/tcp_demo.cpp
)

target_link_libraries(tcp_demo PRIVATE ws2_32)
add_executable(tcp_server
    src/tcp_server.cpp
)

target_link_libraries(tcp_server PRIVATE ws2_32)
add_executable(tcp_client
    src/tcp_client.cpp
)

target_link_libraries(tcp_client PRIVATE ws2_32)
add_executable(routed_message_demo
    src/routed_message_demo.cpp
)

target_link_libraries(routed_message_demo PRIVATE routing_core)
add_executable(tcp_relay
    src/tcp_relay.cpp
)
add_executable(tcp_destination
    src/tcp_destination.cpp
)

target_link_libraries(tcp_destination PRIVATE ws2_32)
add_executable(tcp_source
    src/tcp_source.cpp
)

target_link_libraries(tcp_source PRIVATE ws2_32)
add_executable(route_aware_demo
    src/route_aware_demo.cpp
)

target_link_libraries(route_aware_demo PRIVATE routing_core)
add_executable(route_tcp_node
    src/route_tcp_node.cpp
)

target_link_libraries(route_tcp_node PRIVATE routing_core ws2_32)
