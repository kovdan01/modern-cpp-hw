cmake_minimum_required(VERSION 3.19)

project(hw2-socks5-server
        VERSION 0.0.0
        LANGUAGES CXX
)

find_package(ntc-cmake REQUIRED)
include(ntc-dev-build)

add_subdirectory(wrappers)
add_subdirectory(server)
