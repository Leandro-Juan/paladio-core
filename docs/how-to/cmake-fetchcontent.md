# How-To: Integrate via CMake FetchContent

This guide details how to integrate `paladio-core` into downstream C++ projects using CMake `FetchContent`, configure optimization flags, and link targets.

---

## 1. Minimal Integration

In your project's `CMakeLists.txt`:

```cmake
cmake_minimum_required(VERSION 3.20)
project(MyApp LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(
    paladio_core
    GIT_REPOSITORY https://github.com/Leandro-Juan/paladio-core.git
    GIT_TAG        v1.0.0
)

# Optional: Disable Python bindings and GoogleTests to keep your build ultra-lean
set(PALADIO_BUILD_PYTHON OFF CACHE BOOL "" FORCE)
set(PALADIO_BUILD_TESTS OFF CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(paladio_core)

add_executable(my_app src/main.cpp)
target_link_libraries(my_app PRIVATE paladio::engine)
```

In your source code:
```cpp
#include <paladio/engine.hpp>
```

---

## 2. Enabling Hardware-Specific Optimizations (`-march=native`)

If you are compiling for dedicated host servers or high-performance bare-metal environments, activate native vectorization and CPU instruction extensions (AVX2, BMI2):

```cmake
set(PALADIO_ENABLE_NATIVE ON CACHE BOOL "" FORCE)
```

This compiles the hot-path Branch & Bound search loops with `-O3 -march=native -DNDEBUG`, enabling faster bit manipulations via hardware intrinsics (`__builtin_ctzll` / `std::countr_zero`).

---

## 3. Static vs. Shared Target Properties

- **`paladio_engine_static` (alias `paladio::engine`)**: Static C++20 library compiled with position-independent code (`POSITION_INDEPENDENT_CODE ON`), ready to link directly into executables or dynamic plugins without runtime dependencies.
- **Includes**: Includes are exported via `$<BUILD_INTERFACE:...>` and `$<INSTALL_INTERFACE:...>` under the prefix `paladio/`.
