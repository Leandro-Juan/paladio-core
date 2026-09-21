cmake_minimum_required(VERSION 3.20)
project(PaladioEngine VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Fetch pybind11 for Python integration
include(FetchContent)
FetchContent_Declare(
    pybind11
    GIT_REPOSITORY https://github.com/pybind/pybind11.git
    GIT_TAG        v2.11.1
)
FetchContent_MakeAvailable(pybind11)

# Fetch GoogleTest
set(GTEST_VERSION v1.14.0)
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/refs/tags/${GTEST_VERSION}.zip
)
FetchContent_MakeAvailable(googletest)

option(PALADIO_ENABLE_NATIVE "Enable -march=native optimization" OFF)

# Main Library
pybind11_add_module(paladio_core src/engine.cpp src/bindings.cpp)
target_include_directories(paladio_core PRIVATE include)

set_target_properties(paladio_core PROPERTIES
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN ON
)

# Compiler optimizations and sanitizer flags (configurable via presets or CLI)
target_compile_options(paladio_core PRIVATE
    $<$<CONFIG:Debug>:-g -Og -fsanitize=address>
    $<$<AND:$<CONFIG:Release>,$<BOOL:${PALADIO_ENABLE_NATIVE}>>:-O3 -march=native -DNDEBUG>
    $<$<AND:$<CONFIG:Release>,$<NOT:$<BOOL:${PALADIO_ENABLE_NATIVE}>>>:-O3 -DNDEBUG>
    $<$<CXX_COMPILER_ID:GNU>:-Wall -Wextra -Werror>
    $<$<CXX_COMPILER_ID:Clang>:-Weverything -Wno-c++98-compat>
)

target_link_options(paladio_core PRIVATE
    $<$<CONFIG:Debug>:-fsanitize=address>
)

# Testing
add_executable(paladio_tests
  tests/engine_test.cpp
  tests/engine_stress_test.cpp
  tests/engine_real_data_test.cpp
  src/engine.cpp
)

target_include_directories(paladio_tests PRIVATE include)
# We have to exclude the pybind11 module macro from the tests to avoid duplicate main/module
target_compile_definitions(paladio_tests PRIVATE -DPALADIO_TESTING=1)

target_compile_options(paladio_tests PRIVATE
    $<$<CONFIG:Debug>:-g -Og -fsanitize=address>
    $<$<CXX_COMPILER_ID:GNU>:-Wall -Wextra -Werror>
)
target_link_options(paladio_tests PRIVATE $<$<CONFIG:Debug>:-fsanitize=address>)

target_link_libraries(paladio_tests GTest::gtest GTest::gmock GTest::gtest_main)

enable_testing()
include(GoogleTest)
gtest_discover_tests(paladio_tests)

