"""Type stubs for the paladio_core C++ extension module."""

from typing import Any, List, Optional
import enum
import numpy as np
import numpy.typing as npt

class NodeType(enum.Enum):
    ATTRACTION: int
    HOTEL: int
    RESTAURANT_BREAKFAST: int
    RESTAURANT_LUNCH: int
    RESTAURANT_DINNER: int
    BAR: int

class TransitInfo:
    duration: int
    cost: float
    def __init__(self, duration: int, cost: float) -> None: ...

class POI:
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

    def __init__(
        self,
        type: NodeType,
        cost: float,
        score: float,
        earliest_time: int,
        latest_time: int,
        duration: int,
        is_mandatory: bool = False,
    ) -> None: ...

class OptimizationConfig:
    max_budget: float
    start_node_index: Optional[int]
    end_node_type: Optional[NodeType]
    end_node_index: Optional[int]
    end_time_limit: int
    breakfast_deadline: int
    lunch_deadline: int
    dinner_deadline: int
    max_idle_time: int
    idle_time_penalty_rate: float
    max_active_time_before_fatigue: int
    fatigue_penalty_multiplier: float
    min_meal_spacing: int
    monotony_threshold: int
    monotony_multiplier: float
    timeout_ms: int

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
        timeout_ms: int = 5000,
    ) -> None: ...

class OptimizationResult:
    path: List[int]
    total_cost: float
    total_time: float
    total_score: float

def optimize_itinerary(
    pois: List[POI],
    durations: npt.NDArray[np.int32] | Any,
    costs: npt.NDArray[np.float64] | Any,
    config: OptimizationConfig,
) -> OptimizationResult:
    """Optimize travel constraints (TSPTW + Knapsack). Releases GIL during computation."""
    ...
