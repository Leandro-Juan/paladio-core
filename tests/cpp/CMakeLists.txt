# GoogleTest Suite for Paladio Core

set(GTEST_VERSION v1.14.0)
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/refs/tags/${GTEST_VERSION}.zip
)
FetchContent_MakeAvailable(googletest)

add_executable(paladio_tests
  engine_test.cpp
  engine_stress_test.cpp
  engine_real_data_test.cpp
)

target_link_libraries(paladio_tests PRIVATE
  paladio_engine_static
  GTest::gtest
  GTest::gmock
  GTest::gtest_main
)

target_compile_definitions(paladio_tests PRIVATE PALADIO_TESTING=1)
target_include_directories(paladio_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

target_compile_options(paladio_tests PRIVATE
  $<$<CONFIG:Debug>:-g -Og -fsanitize=address,undefined>
  $<$<CXX_COMPILER_ID:GNU>:-Wall -Wextra -Werror>
  $<$<CXX_COMPILER_ID:Clang>:-Wall -Wextra -Werror>
)
target_link_options(paladio_tests PRIVATE
  $<$<CONFIG:Debug>:-fsanitize=address,undefined>
)

include(GoogleTest)
gtest_discover_tests(paladio_tests)
