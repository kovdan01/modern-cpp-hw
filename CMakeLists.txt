cmake_minimum_required(VERSION 3.16)

project(modern-cpp-hw
        VERSION 0.0.0
        LANGUAGES CXX
)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/ntc-cmake")

# Include ntc-dev-build to disable find_package(ntc-cmake)
# in subprojects and establish default for BUILD_SHARED_LIBS.
include(ntc-dev-build)

enable_testing()

add_subdirectory(hw1-binary-serialization)

include(FeatureSummary)
feature_summary(WHAT ALL)
