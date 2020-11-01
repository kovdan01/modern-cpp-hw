cmake_minimum_required(VERSION 3.19)

project(hw1-tests
        VERSION 0.0.0
        LANGUAGES CXX
)

find_package(ntc-cmake REQUIRED)
include(ntc-dev-build)

# current homework libraries
find_package(hw1-wrappers REQUIRED)
find_package(hw1-message REQUIRED)

# message pack - header-only, no find_package needed

# Catch2
find_package(Catch2 REQUIRED)

# compile main only once to speed up compilation
set(TESTS_MAIN_NAME tests-main)

add_library(${TESTS_MAIN_NAME} OBJECT
    src/main.cpp)

target_compile_features(${TESTS_MAIN_NAME} PRIVATE cxx_std_20)

target_link_libraries(${TESTS_MAIN_NAME} PRIVATE Catch2::Catch2)
target_link_libraries(${TESTS_MAIN_NAME} PRIVATE hw1::message)

# test cases
add_executable(${PROJECT_NAME}
    src/tests.cpp
)

target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_20)

target_link_libraries(${PROJECT_NAME} PRIVATE Catch2::Catch2)
target_link_libraries(${PROJECT_NAME} PRIVATE hw1::message)
target_link_libraries(${PROJECT_NAME} PRIVATE hw1::wrappers)
target_link_libraries(${PROJECT_NAME} PRIVATE ${TESTS_MAIN_NAME})

ntc_target(${PROJECT_NAME})

file(CREATE_LINK
    ${CMAKE_CURRENT_LIST_DIR}/test.data
    ${CMAKE_CURRENT_BINARY_DIR}/test.data
)

include(CTest)
include(Catch)
catch_discover_tests(${PROJECT_NAME}
    EXTRA_ARGS -I test.data
)
