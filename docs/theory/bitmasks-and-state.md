# 64-Bit Bitmask State Encoding & Dominance Pruning

This document details the low-level systems engineering and memory layout techniques that enable `paladio-core` to execute branch transitions in single-digit clock cycles.

---

## 1. 64-Bit Integer Bitmask Representation

Traditional graph routing algorithms track visited subsets using dynamic containers (`std::vector<bool>`, `std::unordered_set<int>`), which trigger heap allocations, pointer indirection, and CPU cache misses.

In `paladio-core`, subsets of nodes are encoded directly as bit fields inside standard 64-bit unsigned integers (`uint64_t visited_mask`):

- **Bit $i$ is set to 1:** POI at density rank $i$ has been visited along the current trajectory.
- **Bit $i$ is set to 0:** POI at density rank $i$ remains available.

### Hardware-Accelerated Bit Operations
Testing and updating visitation states are atomic, single-cycle CPU operations:

```cpp
// Check if node at rank 'r' is visited
bool is_visited = (visited_mask & (1ULL << r)) != 0;

// Mark node as visited
visited_mask |= (1ULL << r);

// Find the lowest unvisited candidate rank in 1 CPU cycle
int next_rank = std::countr_zero(~visited_mask);
```

On modern x86_64 architectures, `std::countr_zero` compiles directly to the `TZCNT` or `BSF` machine instruction.

---

## 2. Multi-Criteria Pareto Dominance Pruning

When traversing graph permutations, different sequences can reach the exact same node $u$ with the exact same subset of visited locations ($S$).

For example, path $0 \to 1 \to 2 \to 3$ and path $0 \to 2 \to 1 \to 3$ both arrive at node 3 with visited set $\{0, 1, 2, 3\}$.

### Dominance Relation
Let State $A$ and State $B$ represent two partial paths arriving at node $u$ with identical visited bitmask $S$.

State $A$ **Pareto-dominates** State $B$ if and only if:

$$\begin{aligned}
\text{time}(A) &\le \text{time}(B) \\
\text{cost}(A) &\le \text{cost}(B) \\
\text{score}(A) &\ge \text{score}(B) \\
\text{active\_time}(A) &\le \text{active\_time}(B)
\end{aligned}$$

with at least one strict inequality.

If State $B$ is dominated by an earlier explored State $A$, continuing to explore descendants of State $B$ is mathematically guaranteed to produce inferior results to the descendants of State $A$. State $B$ is discarded immediately.

---

## 3. Contiguous Memory Layout (`SearchState`)

The entire state representation during DFS recursion is encapsulated in `SearchState`:

```cpp
struct SearchState {
  uint64_t visited_mask = 0;
  double current_cost = 0.0;
  int current_time = 0;
  double current_score = 0.0;
  bool had_breakfast = false;
  bool had_lunch = false;
  bool had_dinner = false;
  int last_meal_time = -9999;
  int continuous_active_time = 0;
  std::array<uint8_t, 8> category_visits = {0};
  std::array<int, 64> current_path;
  int current_path_size = 0;
};
```

Because `current_path` uses fixed-size `std::array<int, 64>` instead of `std::vector<int>`, each recursion step modifies only stack memory, keeping CPU pipeline branch prediction hot and memory access bound to L1 cache lines.
