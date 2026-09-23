# How-To: Build with Sanitizers & Run Stress Tests

This guide outlines how to compile `paladio-core` with memory sanitizers (AddressSanitizer and UndefinedBehaviorSanitizer) and execute stress tests.

---

## 1. Building with Sanitizers

AddressSanitizer (ASan) instruments memory loads and stores to detect buffer overflows, use-after-free, and stack corruption. UndefinedBehaviorSanitizer (UBSan) detects integer overflows and invalid bit-shifts.

### Enabling in CMake
```bash
cmake -B build_asan -S . \
    -DCMAKE_BUILD_TYPE=Debug \
    -DPALADIO_BUILD_TESTS=ON \
    -DPALADIO_ENABLE_SANITIZERS=ON

cmake --build build_asan -j$(nproc)
```

### Running C++ Test Suite under ASan/UBSan
```bash
ctest --test-dir build_asan --output-on-failure
```

All 20 test cases—including dense 64-node combinatorial stress graphs and real-world geographic datasets—must pass with zero leaks and zero errors reported.

---

## 2. Running Combinatorial Stress Tests

The GoogleTest suite includes dedicated stress tests:

1. **`EngineStressTest.SixtyFourPOIDenseGraph`**: Constructs a 64-node complete graph with 4,096 transit edges to stress-test the 64-bit integer bitmask boundaries and knapsack bounding under deep branching.
2. **`EngineStressTest.MaximumBitmaskIndex`**: Targets node index 63 specifically to verify bit-shift validity at `1ULL << 63` and prevent undefined overflow behavior.
3. **`EngineStressTest.RandomTimeConstraintFuzzing`**: Generates randomly overlapping time windows to stress feasibility pruning and temporal edge cases.

To run only the stress tests:
```bash
./build_asan/tests/cpp/paladio_tests --gtest_filter="EngineStressTest.*"
```

---

## 3. ASan Runtime Preloading with Python (If Needed)

When testing Python bindings that link against an ASan-instrumented C++ library, the AddressSanitizer runtime library must be preloaded:

```bash
LD_PRELOAD=$(gcc -print-file-name=libasan.so) pytest tests/python/ -v
```

> **Note:** In production, Python extension modules should always be compiled with standard Release flags (`-O3 -DNDEBUG`) without ASan to avoid conflicting with Python's internal memory pools.
