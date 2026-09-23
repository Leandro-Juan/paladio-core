# 5-Minute C++ Quickstart

This tutorial walks you through integrating `paladio-core` into a modern C++20 application using CMake and executing an itinerary optimization run.

---

## 1. Setting Up Your `CMakeLists.txt`

The cleanest way to consume `paladio-core` is via CMake's `FetchContent` module:

```cmake
cmake_minimum_required(VERSION 3.20)
project(MyRoutingApp LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(
    paladio_core
    GIT_REPOSITORY https://github.com/Leandro-Juan/paladio-core.git
    GIT_TAG        v1.0.0
)
FetchContent_MakeAvailable(paladio_core)

add_executable(my_routing_app main.cpp)
target_link_libraries(my_routing_app PRIVATE paladio::engine)
```

> [!TIP]
> You can point `GIT_TAG` to a specific release tag such as `v1.0.0` or track the latest development branch using `GIT_TAG main`.

---

## 2. Writing `main.cpp`

Create `main.cpp` using the namespaced public API `#include <paladio/engine.hpp>`:

```cpp
#include <iostream>
#include <vector>
#include <paladio/engine.hpp>

int main() {
    using namespace paladio::core;

    // 1. Define POI candidates
    std::vector<POI> pois = {
        // Type, Cost, Score, Earliest Time, Latest Time, Duration, is_mandatory
        POI(NodeType::HOTEL, 0.0, 0.0, 480, 1440, 0),
        POI(NodeType::ATTRACTION, 18.0, 95.0, 540, 1080, 90),
        POI(NodeType::RESTAURANT_LUNCH, 28.0, 60.0, 720, 900, 60),
        POI(NodeType::ATTRACTION, 12.0, 70.0, 600, 1140, 45),
        POI(NodeType::HOTEL, 0.0, 0.0, 480, 1440, 0)
    };

    const int N = static_cast<int>(pois.size());

    // 2. Define flattened 5x5 transit duration and cost matrices
    std::vector<int> durations(N * N, 15);
    std::vector<double> costs(N * N, 2.5);

    // Diagonal elements (self-transit) are zero
    for (int i = 0; i < N; ++i) {
        durations[i * N + i] = 0;
        costs[i * N + i] = 0.0;
    }

    // 3. Configure optimization constraints
    OptimizationConfig config;
    config.max_budget = 120.0;
    config.start_node_index = 0;
    config.end_node_index = N - 1;
    config.end_time_limit = 1140; // Tour must complete before 19:00
    config.lunch_deadline = 840;  // Lunch must be reached by 14:00

    // 4. Execute optimization
    OptimizationResult result = optimize_itinerary(
        pois,
        durations.data(),
        costs.data(),
        config
    );

    // 5. Inspect results
    std::cout << "Optimization Complete!\n";
    std::cout << "Optimal Score: " << result.total_score << "\n";
    std::cout << "Total Cost:    " << result.total_cost << " EUR\n";
    std::cout << "Total Time:    " << result.total_time << " minutes\n";
    std::cout << "Route Sequence: ";
    for (size_t i = 0; i < result.path.size(); ++i) {
        std::cout << result.path[i] << (i + 1 < result.path.size() ? " -> " : "\n");
    }

    return 0;
}
```

---

## 3. Building and Running

```bash
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
./build/my_routing_app
```

Output:
```text
Optimization Complete!
Optimal Score: 225
Total Cost:    68 EUR
Total Time:    250 minutes
Route Sequence: 0 -> 1 -> 2 -> 3 -> 4
```
