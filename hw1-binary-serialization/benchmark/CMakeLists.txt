cmake_minimum_required(VERSION 3.19)

project(hw1-benchmark
        VERSION 0.0.0
        LANGUAGES CXX
)

find_package(ntc-cmake REQUIRED)
include(ntc-dev-build)

# current homework libraries
find_package(hw1-wrappers REQUIRED)
find_package(hw1-message REQUIRED)

# boost program options
find_package(Boost COMPONENTS COMPONENTS program_options REQUIRED)

# message pack
find_package(msgpack REQUIRED)

# google benchmark
find_package(benchmark REQUIRED)

set(THREADS_PREFER_PTHERAD_FLAG ON)
find_package(Threads REQUIRED)

add_executable(${PROJECT_NAME}
    src/main.cpp
)

target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_20)

target_link_libraries(${PROJECT_NAME} PRIVATE hw1::wrappers)
target_link_libraries(${PROJECT_NAME} PRIVATE hw1::message)
target_link_libraries(${PROJECT_NAME} PRIVATE Boost::program_options)
target_link_libraries(${PROJECT_NAME} PRIVATE benchmark::benchmark)
target_link_libraries(${PROJECT_NAME} PRIVATE Threads::Threads)

ntc_target(${PROJECT_NAME})
