cmake_minimum_required(VERSION 3.20)
project(PaladioEngine VERSION 1.0.0 LANGUAGES CXX)

if(POLICY CMP0135)
  cmake_policy(SET CMP0135 NEW)
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Build Options
option(PALADIO_BUILD_TESTS "Build GoogleTest test suite" OFF)
option(PALADIO_ENABLE_NATIVE "Enable -march=native optimization" OFF)
option(PALADIO_ENABLE_SANITIZERS "Enable AddressSanitizer and UndefinedBehaviorSanitizer" OFF)
option(PALADIO_BUILD_PYTHON "Build Python bindings via pybind11" ON)

include(FetchContent)

# Core Static Library (Consumed by native C++ applications and test suites)
add_library(paladio_engine_static STATIC
    src/engine.cpp
)
add_library(paladio::engine ALIAS paladio_engine_static)

target_include_directories(paladio_engine_static PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_compile_options(paladio_engine_static PRIVATE
    $<$<BOOL:${PALADIO_ENABLE_SANITIZERS}>:-g -Og -fsanitize=address,undefined>
    $<$<AND:$<CONFIG:Release>,$<BOOL:${PALADIO_ENABLE_NATIVE}>>:-O3 -march=native -DNDEBUG>
    $<$<AND:$<CONFIG:Release>,$<NOT:$<BOOL:${PALADIO_ENABLE_NATIVE}>>>:-O3 -DNDEBUG>
    $<$<CXX_COMPILER_ID:GNU>:-Wall -Wextra -Werror>
    $<$<CXX_COMPILER_ID:Clang>:-Wall -Wextra -Werror>
)

target_link_options(paladio_engine_static PUBLIC
    $<$<BOOL:${PALADIO_ENABLE_SANITIZERS}>:-fsanitize=address,undefined>
)

# Python Module (paladio_core)
if(PALADIO_BUILD_PYTHON)
    FetchContent_Declare(
        pybind11
        GIT_REPOSITORY https://github.com/pybind/pybind11.git
        GIT_TAG        v2.11.1
    )
    FetchContent_MakeAvailable(pybind11)

    pybind11_add_module(paladio_core src/bindings.cpp)
    target_link_libraries(paladio_core PRIVATE paladio_engine_static)

    set_target_properties(paladio_core PROPERTIES
        CXX_VISIBILITY_PRESET hidden
        VISIBILITY_INLINES_HIDDEN ON
    )
endif()

# C++ Testing with GoogleTest
if(PALADIO_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests/cpp)
endif()
