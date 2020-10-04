cmake_minimum_required(VERSION 3.16)

set(NAMESPACE hw)
set(COMPONENT binary-serialization)
project(${NAMESPACE}-${COMPONENT}
        VERSION 0.0.0
        LANGUAGES CXX
)

find_package(ntc-cmake REQUIRED)
include(ntc-dev-build)

add_executable(${PROJECT_NAME}
    src/main.cpp
    src/message.hpp
    src/message.cpp
)

target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_20)

set(THIRD_PARTY_DIR ${CMAKE_CURRENT_LIST_DIR}/3rd_party)

ntc_target(${PROJECT_NAME})

target_include_directories(${PROJECT_NAME} PRIVATE ${THIRD_PARTY_DIR}/msgpack/include)
set(MSGPACK_CXX17 on)
