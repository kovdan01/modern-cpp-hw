cmake_minimum_required(VERSION 3.19)

project(hw1-binary-serialization
        VERSION 0.0.0
        LANGUAGES CXX
)

find_package(ntc-cmake REQUIRED)
include(ntc-dev-build)

add_subdirectory(wrappers)
add_subdirectory(message)
add_subdirectory(benchmark)
add_subdirectory(generate-data)
add_subdirectory(tests)
