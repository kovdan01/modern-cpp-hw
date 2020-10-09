cmake_minimum_required(VERSION 3.16)

set(NAMESPACE hw)
set(COMPONENT binary-serialization)
project(${NAMESPACE}-${COMPONENT}
        VERSION 0.0.0
        LANGUAGES CXX
)

find_package(ntc-cmake REQUIRED)
include(ntc-dev-build)

# message pack - header-only, no find_package needed

# cbor
find_package(PkgConfig REQUIRED)
pkg_check_modules(cbor REQUIRED IMPORTED_TARGET libcbor)

# bson
find_package(mongoc-1.0 REQUIRED)

# google benchmark
find_package(benchmark REQUIRED)

set(THREADS_PREFER_PTHERAD_FLAG ON)
find_package(Threads REQUIRED)

add_executable(${PROJECT_NAME}
    src/cbor_wrapper.hpp
    src/cbor_wrapper.cpp
    src/main.cpp
    src/message.hpp
    src/message.cpp
    src/test_input.inc
    src/types.hpp
)

target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_20)

ntc_target(${PROJECT_NAME})

target_link_libraries(${PROJECT_NAME} PRIVATE PkgConfig::cbor)
target_link_libraries(${PROJECT_NAME} PRIVATE mongo::bson_shared)
target_link_libraries(${PROJECT_NAME} PRIVATE benchmark::benchmark)
target_link_libraries(${PROJECT_NAME} PRIVATE Threads::Threads)
