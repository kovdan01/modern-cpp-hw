cmake_minimum_required(VERSION 3.19)

project(hw1-message
        VERSION 0.0.0
        LANGUAGES CXX
)

find_package(ntc-cmake REQUIRED)
include(ntc-dev-build)

# current homework libraries
find_package(hw1-wrappers REQUIRED)

# message pack
find_package(msgpack REQUIRED)

add_library(${PROJECT_NAME}
    include/message.hpp
    include/message.ipp
    src/message.cpp
)

target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_20)

target_link_libraries(${PROJECT_NAME} PUBLIC hw1::wrappers)
target_link_libraries(${PROJECT_NAME} PUBLIC msgpackc-cxx)

ntc_target(${PROJECT_NAME}
    ALIAS_NAME hw1::message
    HEADER_PREFIX hw1/message/
)
