cmake_minimum_required(VERSION 3.14)
project(deployer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

# yaml-cpp
FetchContent_Declare(
    yaml-cpp
    GIT_REPOSITORY https://github.com/jbeder/yaml-cpp.git
    GIT_TAG yaml-cpp-0.9.0
)
set(YAML_CPP_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(YAML_CPP_BUILD_TOOLS OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(yaml-cpp)

# Google Test
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.14.0
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

# Parser library (shared between deployer and tests)
add_library(parser_lib STATIC src/parser.cpp)
target_include_directories(parser_lib PUBLIC src)
target_link_libraries(parser_lib PUBLIC yaml-cpp::yaml-cpp)

# Main deployer binary
add_executable(deployer src/main.cpp)
target_link_libraries(deployer PRIVATE parser_lib)

# Tests
enable_testing()
add_executable(deployer_tests tests/parser_test.cpp)
target_link_libraries(deployer_tests PRIVATE parser_lib GTest::gtest_main)
include(GoogleTest)
gtest_discover_tests(deployer_tests)
