cmake_minimum_required(VERSION 3.16)

set(NAMESPACE hw1)
set(COMPONENT wrappers)
project(${NAMESPACE}-${COMPONENT}
        VERSION 0.0.0
        LANGUAGES CXX
)

find_package(ntc-cmake REQUIRED)
include(ntc-dev-build)

# cbor
find_package(PkgConfig REQUIRED)
pkg_check_modules(cbor REQUIRED IMPORTED_TARGET libcbor)

# bson
find_package(mongoc-1.0 REQUIRED)

add_library(${PROJECT_NAME}
    include/cbor_wrapper.hpp
    src/cbor_wrapper.cpp)

target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_20)

target_link_libraries(${PROJECT_NAME} PRIVATE PkgConfig::cbor)
target_link_libraries(${PROJECT_NAME} PRIVATE mongo::bson_shared)

ntc_target(${PROJECT_NAME})
