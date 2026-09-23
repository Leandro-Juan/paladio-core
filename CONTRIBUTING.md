# Contributing to Paladio Core

**Paladio Core** (`paladio-core`) was created, architected, and engineered by **Leandro Juan** as a core personal flagship project in high-performance C++20 combinatorial optimization and operations research.

As the sole author and principal architect, **Leandro Juan** directs the project's roadmap, architectural invariants, and design principles.

---

## 1. Project Invariants & Architectural Mandates

Any proposed improvements or pull requests must strictly adhere to the project's architectural invariants:

1. **Deterministic Rigor**: Core routing algorithms must yield identical, deterministic outputs across executions for identical input topologies and constraints.
2. **Zero Dynamic Heap Allocations in Hot Loops**: The inner Branch & Bound recursion must remain completely free of dynamic allocations (`malloc`, `new`, `std::vector::push_back`). Recursion state must reside on contiguous stack memory (`std::array`, fixed-size buffers, bitmasks).
3. **Memory Safety & Clean Sanitizers**: All C++ code must compile warning-free under `-Wall -Wextra -Werror` and pass AddressSanitizer (ASan), LeakSanitizer (LSan), and UndefinedBehaviorSanitizer (UBSan) with zero leaks or errors.
4. **Operations Research Soundness**: Any new pruning heuristic must be mathematically justified and documented (whether it is an admissible relaxation or a guided heuristic).
5. **No AI-Generated Slop**: Contributions must demonstrate deep understanding of modern C++20 systems programming and operations research principles.

---

## 2. Development & Local Testing Workflow

### Prerequisites
- Modern C++ compiler supporting **C++20** (GCC 12+, Clang 16+, MSVC 2022+)
- CMake 3.20+
- Python 3.10+
- Ninja (recommended)

### Building Native C++ with AddressSanitizer
```bash
# Configure with test suite and AddressSanitizer enabled
cmake -B build -S . -DCMAKE_BUILD_TYPE=Debug -DPALADIO_BUILD_TESTS=ON -DPALADIO_ENABLE_SANITIZERS=ON

# Compile GoogleTest suite and core library
cmake --build build -j$(nproc)

# Run GoogleTest test suite with full failure diagnostics
ctest --test-dir build --output-on-failure
```

### Building Python Bindings in Release Mode
```bash
# Build the Python extension module in Release mode
cmake -B build_release -S . -DCMAKE_BUILD_TYPE=Release -DPALADIO_BUILD_PYTHON=ON
cmake --build build_release -j$(nproc)

# Run Python test suite
PYTHONPATH=build_release pytest tests/python/ -v
```

---

## 3. Formatting & Standards

We enforce strict formatting rules:
- C++ formatting via `.clang-format`:
  ```bash
  clang-format -i include/paladio/*.hpp src/*.cpp tests/cpp/*.cpp tests/cpp/*.hpp
  ```
- Python formatting:
  ```bash
  ruff format tests/python/
  ruff check tests/python/
  ```

---

## 4. Submitting Contributions

All contributions are subject to final review and approval by **Leandro Juan**:
1. Open an issue first to discuss substantial architectural or algorithmic modifications.
2. Fork the repository and create your feature branch (`git checkout -b feat/my-improvement`).
3. Ensure both C++ tests (`ctest`) and Python tests (`pytest`) pass with zero errors.
4. Open a Pull Request with a clear, technical description of your change and benchmark impact.
