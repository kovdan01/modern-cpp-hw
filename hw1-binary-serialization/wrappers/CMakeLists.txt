cmake_minimum_required(VERSION 3.16)

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

add_library(${PROJECT_NAME}
    include/bson_wrapper.hpp
    include/cbor_wrapper.hpp
    src/bson_wrapper.cpp
    src/cbor_wrapper.cpp)

target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_20)

target_link_libraries(${PROJECT_NAME} PUBLIC mongo::bson_shared)
target_link_libraries(${PROJECT_NAME} PUBLIC PkgConfig::cbor)

ntc_target(${PROJECT_NAME}
    ALIAS_NAME hw1::wrappers
    HEADER_PREFIX hw1/wrappers/
)
