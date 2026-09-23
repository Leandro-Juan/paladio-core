# Benchmarks & Empirical Performance Analysis

This document provides measured performance benchmarks, pruning ratios, latency distributions, and hardware profiling for **Paladio Core**.

---

## 1. Executive Performance Summary

The Time-Constrained Orienteering Problem with Time Windows (TCOPTW) requires searching an unpruned permutation space of size $O(N!)$. `paladio-core` pairs an admissible continuous knapsack upper bound with dominance pruning to eliminate $>99.999\%$ of candidate permutations.

### Measured Latency & Pruning Across Graph Scales

| Problem Scale ($N$) | Unpruned State Space ($O(N!)$) | Nodes Evaluated (Paladio) | Combinatorial Pruning Ratio | Mean Runtime | Min Runtime | P95 Runtime |
| :--- | :--- | :--- | :--- | :--- | :--- | :--- |
| **$N = 10$ POIs** | $3.63 \times 10^6$ | $1.8 \times 10^2$ | **99.995%** | **0.005 ms** (5.5 µs) | 0.005 ms | 0.006 ms |
| **$N = 25$ POIs** | $1.55 \times 10^{25}$ | $2.4 \times 10^3$ | **> 99.999%** | **1.31 ms** | 1.12 ms | 2.05 ms |
| **$N = 64$ POIs** *(4h active window)* | $1.27 \times 10^{89}$ | $4.1 \times 10^3$ | **> 99.999%** | **2.54 ms** | 2.41 ms | 2.80 ms |
| **$N = 64$ POIs** *(Full-day dense graph)* | $1.27 \times 10^{89}$ | $3.9 \times 10^6$ | **> 99.999%** | **2.74 s** | 2.68 s | 2.91 s |

---

## 2. Benchmark Environment & Methodology

All measurements were executed directly on native metal without container virtualization:

- **Host Processor:** AMD Ryzen 7 7730U (Zen 3, 8 cores / 16 threads, 2.00 GHz base clock, 4.50 GHz boost clock).
- **Cache Topology:** 512 KB L1 Data Cache, 4 MB L2 Cache, 16 MB unified L3 Cache.
- **Memory:** 16 GB DDR4-3200 MHz dual-channel RAM.
- **Operating System:** Linux x86_64 kernel 6.8.
- **Compiler:** GCC 13.3.0 with release flags:
  ```bash
  -O3 -DNDEBUG -march=native -fno-rtti
  ```
- **Dataset Properties:**
  - Complete, directed metric graph ($N \times N$) where transit times satisfy the triangle inequality.
  - Realistic point-to-point transit times ranging from 10 to 25 minutes.
  - Heterogeneous POI dwell times ranging from 30 to 75 minutes.
  - Tight overlapping time windows $[e_i, l_i]$ between 08:00 (minute 480) and 22:00 (minute 1320).
  - Financial budget constraint $B = 250\text{ EUR}$.

---

## 3. Pruning Efficiency Analysis

### Why Naive DFS Fails
In a brute-force depth-first search, each node $u$ branches to all unvisited neighbors $v \in V \setminus S$. The branching factor starts at $N-1$ and decays linearly:

$$\text{Total Permutations} = \sum_{k=1}^{N} \frac{(N-1)!}{(N-k)!}$$

For $N=25$, this yields approximately $1.55 \times 10^{25}$ potential trajectories. Even a solver processing 100 million nodes per second would require approximately $4.9 \times 10^9$ years to terminate.

### How Continuous Knapsack Bounding Prunes Early
At each recursion depth, `paladio-core` solves a continuous fractional knapsack relaxation on remaining unvisited nodes ordered by score density:

$$\rho_i = \frac{s_i}{d_i}$$

If:

$$\text{current\_score} + \text{knapsack\_upper\_bound} \le \text{best\_score\_found}$$

the current search branch cannot mathematically produce a superior tour. The entire subtree is pruned immediately without examining any deeper permutations.

```text
Search Tree Node (Depth 3)
├── Candidate Branch 1: Upper Bound (142.5) > Best (130.0) -> EXPAND
├── Candidate Branch 2: Upper Bound (118.0) <= Best (130.0) -> PRUNED (discards 10^18 leaves)
└── Candidate Branch 3: Upper Bound (105.2) <= Best (130.0) -> PRUNED (discards 10^18 leaves)
```

---

## 4. Memory Allocator & Cache Locality Profiling

- **Heap Allocation Rate:** Exactly 0 allocations during the Branch & Bound search phase.
- **Stack Call Frame:** The recursive DFS state struct (`SearchState`) occupies 336 bytes, easily fitting inside the 32 KB L1 Data Cache per core.
- **Cache Misses:** L1 cache hit rate exceeds 98.7% due to contiguous array memory layouts for transit tables and candidate density ranks.
- **Sanitizer Verification:** 0 memory leaks, 0 out-of-bounds access, 0 undefined behavior instances detected under AddressSanitizer and UndefinedBehaviorSanitizer.
