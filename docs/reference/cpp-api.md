# C++20 API Reference

All C++ symbols reside within the `paladio::core` namespace in header `<paladio/engine.hpp>`.

---

## 1. Enumerations

### `NodeType`
```cpp
enum class NodeType : uint8_t {
  ATTRACTION,
  HOTEL,
  RESTAURANT_BREAKFAST,
  RESTAURANT_LUNCH,
  RESTAURANT_DINNER,
  BAR
};
```
Categorical classification used for mandatory meal scheduling and fatigue resetting (visiting `HOTEL` or `BAR` resets active walking time).

---

## 2. Core Structs

### `POI`
Represents a candidate node in the routing graph.

```cpp
struct POI {
  double cost;              // Financial cost of visiting
  double score;             // Objective reward to maximize
  int earliest_time;        // Opening window (minutes from baseline)
  int latest_time;          // Closing window (minutes from baseline)
  int duration;             // Required dwell time in minutes
  NodeType type;            // Categorical type
  bool is_breakfast_spot;   // True if type == RESTAURANT_BREAKFAST
  bool is_lunch_spot;       // True if type == RESTAURANT_LUNCH
  bool is_dinner_spot;      // True if type == RESTAURANT_DINNER
  bool is_mandatory;        // True if node must be present in final path

  POI() = default;
  POI(NodeType t, double c, double s, int e, int l, int d, bool m = false);
};
```

---

### `TransitInfo`
Represents edge properties between POIs.

```cpp
struct TransitInfo {
  int duration; // Travel time in minutes
  double cost;  // Financial cost of transit
};
```

---

### `OptimizationConfig`
Global parameters, constraints, and physiological realism configuration.

```cpp
struct OptimizationConfig {
  double max_budget;                       // Max total financial cost
  std::optional<int> start_node_index;     // Fixed start node index
  std::optional<NodeType> end_node_type;   // Required final node category
  std::optional<int> end_node_index;       // Required final node index
  int end_time_limit = -1;                 // Global tour deadline in minutes (-1 disabled)
  int breakfast_deadline = -1;             // Deadline to reach breakfast
  int lunch_deadline = -1;                 // Deadline to reach lunch
  int dinner_deadline = -1;                // Deadline to reach dinner
  int max_idle_time = 60;                  // Max allowable waiting time (minutes)
  double idle_time_penalty_rate = 0.5;     // Penalty per 15 min idle
  int max_active_time_before_fatigue = 240;// Minutes before fatigue kicks in
  double fatigue_penalty_multiplier = 0.6; // Score multiplier under fatigue
  int min_meal_spacing = 180;              // Min minutes between meals
  int monotony_threshold = 2;              // Visits before category diminishing utility
  double monotony_multiplier = 0.5;        // Diminishing return multiplier
  int timeout_ms = 5000;                   // Execution timeout guard in ms
};
```

---

### `OptimizationResult`
Optimal itinerary found by the branch-and-bound solver.

```cpp
struct OptimizationResult {
  std::vector<int> path;  // Sequence of POI node indices
  double total_cost;      // Cumulative financial cost
  double total_time;      // Total elapsed duration in minutes
  double total_score;     // Total net objective score achieved
};
```

---

## 3. Solver Function

```cpp
[[nodiscard]] OptimizationResult optimize_itinerary(
    const std::vector<POI> &pois,
    const int *transit_durations,
    const double *transit_costs,
    const OptimizationConfig &config
);
```

### Parameters
- `pois`: Vector of candidate POIs ($N \le 64$).
- `transit_durations`: Pointer to contiguous flattened $N \times N$ matrix of integer travel minutes.
- `transit_costs`: Pointer to contiguous flattened $N \times N$ matrix of double transit costs.
- `config`: Constraint and parameter configuration.

### Exceptions
- `std::invalid_argument`: Thrown if $N > 64$, if matrix pointers are null, or if start/end node indices are out of range $[0, N-1]$.
