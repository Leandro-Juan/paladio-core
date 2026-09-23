# Python API Reference

The `paladio_core` module is a high-performance Python extension compiled via `pybind11`.

---

## 1. Module Overview

```python
import paladio_core
```

---

## 2. Enumerations

### `paladio_core.NodeType`
```python
class NodeType:
    ATTRACTION = ...
    HOTEL = ...
    RESTAURANT_BREAKFAST = ...
    RESTAURANT_LUNCH = ...
    RESTAURANT_DINNER = ...
    BAR = ...
```

---

## 3. Classes

### `paladio_core.POI`
```python
class POI:
    def __init__(
        self,
        type: NodeType,
        cost: float,
        score: float,
        earliest_time: int,
        latest_time: int,
        duration: int,
        is_mandatory: bool = False
    ) -> None: ...

    # Attributes
    type: NodeType
    cost: float
    score: float
    earliest_time: int
    latest_time: int
    duration: int
    is_breakfast_spot: bool
    is_lunch_spot: bool
    is_dinner_spot: bool
    is_mandatory: bool
```

---

### `paladio_core.TransitInfo`
```python
class TransitInfo:
    def __init__(self, duration: int, cost: float) -> None: ...

    duration: int
    cost: float
```

---

### `paladio_core.OptimizationConfig`
```python
class OptimizationConfig:
    def __init__(
        self,
        max_budget: float,
        start_node_index: Optional[int] = None,
        end_node_type: Optional[NodeType] = None,
        end_node_index: Optional[int] = None,
        end_time_limit: int = -1,
        breakfast_deadline: int = -1,
        lunch_deadline: int = -1,
        dinner_deadline: int = -1,
        max_idle_time: int = 60,
        idle_time_penalty_rate: float = 0.5,
        max_active_time_before_fatigue: int = 240,
        fatigue_penalty_multiplier: float = 0.6,
        min_meal_spacing: int = 180,
        monotony_threshold: int = 2,
        monotony_multiplier: float = 0.5,
        timeout_ms: int = 5000
    ) -> None: ...
```

---

### `paladio_core.OptimizationResult`
```python
class OptimizationResult:
    path: List[int]
    total_cost: float
    total_time: float
    total_score: float
```

---

## 4. Functions

### `paladio_core.optimize_itinerary`
```python
def optimize_itinerary(
    pois: List[POI],
    durations: np.ndarray,
    costs: np.ndarray,
    config: OptimizationConfig
) -> OptimizationResult: ...
```

### Arguments
- `pois`: List of `paladio_core.POI` objects ($N \le 64$).
- `durations`: 1D NumPy array of size $N \times N$ with dtype `np.int32` representing flattened transit durations in minutes.
- `costs`: 1D NumPy array of size $N \times N$ with dtype `np.float64` representing flattened transit costs.
- `config`: `paladio_core.OptimizationConfig` object.

### Returns
- `OptimizationResult`: Optimal route path and metrics.

### Raises
- `ValueError`: If $N > 64$, if transit arrays have mismatched lengths or are not 1D, or if start/end node indices are out of bounds.
