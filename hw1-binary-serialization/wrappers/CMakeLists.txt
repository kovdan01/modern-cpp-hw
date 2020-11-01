cmake_minimum_required(VERSION 3.19)

project(hw1-wrappers
        VERSION 0.0.0
        LANGUAGES CXX
)

find_package(ntc-cmake REQUIRED)
include(ntc-dev-build)

# cbor
find_package(PkgConfig REQUIRED)
pkg_check_modules(cbor REQUIRED IMPORTED_TARGET libcbor)

# bson
find_package(bson-1.0 REQUIRED)

add_library(${PROJECT_NAME} INTERFACE
    include/bson_wrapper.hpp
    include/bson_wrapper.ipp
    include/cbor_wrapper.hpp
    include/cbor_wrapper.ipp
)

target_compile_features(${PROJECT_NAME} INTERFACE cxx_std_20)

target_link_libraries(${PROJECT_NAME} INTERFACE mongo::bson_shared)
target_link_libraries(${PROJECT_NAME} INTERFACE PkgConfig::cbor)

ntc_target(${PROJECT_NAME}
    ALIAS_NAME hw1::wrappers
)
