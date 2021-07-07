cmake_minimum_required(VERSION 3.19)

project(hw2-wrappers
        VERSION 0.0.0
        LANGUAGES CXX
)

find_package(ntc-cmake REQUIRED)
include(ntc-dev-build)

# liburing
find_package(PkgConfig REQUIRED)
pkg_check_modules(uring REQUIRED IMPORTED_TARGET liburing)

add_library(${PROJECT_NAME}
    include/io_uring.hpp
    include/socket.hpp
    include/syscall.hpp
    src/io_uring.cpp
    src/socket.cpp
    src/syscall.cpp
)

target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_20)

target_link_libraries(${PROJECT_NAME} PUBLIC PkgConfig::uring)

ntc_target(${PROJECT_NAME}
    ALIAS_NAME hw2::wrappers
)
