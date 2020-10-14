cmake_minimum_required(VERSION 3.16)

set(NAMESPACE hw1)
set(COMPONENT message)
project(${NAMESPACE}-${COMPONENT}
        VERSION 0.0.0
        LANGUAGES CXX
)

find_package(ntc-cmake REQUIRED)
include(ntc-dev-build)

# message pack - header-only, no find_package needed

# current homework libraries
find_package(hw1-wrappers REQUIRED)

add_library(${PROJECT_NAME}
    include/message.hpp
    src/message.cpp
)

target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_20)

target_link_libraries(${PROJECT_NAME} PUBLIC hw1::wrappers)

ntc_target(${PROJECT_NAME} message/)
