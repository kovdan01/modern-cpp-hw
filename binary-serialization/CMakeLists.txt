cmake_minimum_required(VERSION 3.16)

project(binary-serialization
        VERSION 0.0.0
        LANGUAGES CXX
)

find_package(ntc-cmake REQUIRED)
include(ntc-dev-build)

set(HW1_SHARED_INCLUDE_DIR ${CMAKE_CURRENT_LIST_DIR}/include)

add_subdirectory(wrappers)
add_subdirectory(message)
add_subdirectory(benchmark)
add_subdirectory(generate-data)
