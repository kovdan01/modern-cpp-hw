cmake_minimum_required(VERSION 3.16)

set(NAMESPACE hw1)
set(COMPONENT generate-data)
project(${NAMESPACE}-${COMPONENT}
        VERSION 0.0.0
        LANGUAGES CXX
)

find_package(ntc-cmake REQUIRED)
include(ntc-dev-build)

# current homework libraries
find_package(hw1-message REQUIRED)

# boost program options
find_package(Boost COMPONENTS COMPONENTS program_options REQUIRED)

# message pack - header-only, no find_package needed

add_executable(${PROJECT_NAME}
    src/generate_data.cpp
)

target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_20)

target_link_libraries(${PROJECT_NAME} PRIVATE hw1::message)
target_link_libraries(${PROJECT_NAME} PRIVATE Boost::program_options)

ntc_target(${PROJECT_NAME})
