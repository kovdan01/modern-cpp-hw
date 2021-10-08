cmake_minimum_required(VERSION 3.19)

project(hw2-socks5-server
        VERSION 0.0.0
        LANGUAGES CXX
)

find_package(ntc-cmake REQUIRED)
include(ntc-dev-build)

# thread support
set(THREADS_PREFER_PTHERAD_FLAG ON)
find_package(Threads REQUIRED)

# liburing
find_package(PkgConfig REQUIRED)
pkg_check_modules(liburing REQUIRED IMPORTED_TARGET liburing)

# tclap
find_package(PkgConfig REQUIRED)
pkg_check_modules(tclap REQUIRED IMPORTED_TARGET tclap)

# spdlog
find_package(spdlog REQUIRED)

add_executable(${PROJECT_NAME}
    include/server.hpp
    include/socket.hpp
    include/syscall.hpp
    include/utils.hpp
    src/main.cpp
    src/server.cpp
    src/socket.cpp
    src/syscall.cpp
    src/utils.cpp
)

target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_20)

target_link_libraries(${PROJECT_NAME} PRIVATE Threads::Threads)
target_link_libraries(${PROJECT_NAME} PUBLIC PkgConfig::liburing)
target_link_libraries(${PROJECT_NAME} INTERFACE PkgConfig::tclap)
target_link_libraries(${PROJECT_NAME} PRIVATE spdlog::spdlog_header_only)

ntc_target(${PROJECT_NAME})
