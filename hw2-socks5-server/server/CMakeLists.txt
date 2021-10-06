cmake_minimum_required(VERSION 3.19)

project(hw2-socks5-server
        VERSION 0.0.0
        LANGUAGES CXX
)

find_package(ntc-cmake REQUIRED)
include(ntc-dev-build)

find_package(hw2-wrappers)

# thread support
set(THREADS_PREFER_PTHERAD_FLAG ON)
find_package(Threads REQUIRED)

# tclap
find_package(PkgConfig REQUIRED)
pkg_check_modules(tclap REQUIRED IMPORTED_TARGET tclap)

add_executable(${PROJECT_NAME}
    include/event_loop.hpp
    include/utils.hpp
    src/event_loop.cpp
    src/main.cpp
)

target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_20)

target_link_libraries(${PROJECT_NAME} PRIVATE Threads::Threads)
target_link_libraries(${PROJECT_NAME} PRIVATE hw2::wrappers)
target_link_libraries(${PROJECT_NAME} INTERFACE PkgConfig::tclap)

ntc_target(${PROJECT_NAME})
