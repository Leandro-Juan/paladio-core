"""Type stubs for the paladio_core C++ extension module.

Provides high-performance deterministic combinatorial routing solving the
Time-Constrained Orienteering Problem with Time Windows (TCOPTW).
"""

from typing import Any, List, Optional
import enum
import numpy as np
import numpy.typing as npt

class NodeType(enum.Enum):
    """Semantic category classification for a Point of Interest (POI)."""

    ATTRACTION: int
    """Cultural, historic, or recreational attraction. Accrues objective score."""

    HOTEL: int
    """Accommodation depot. Typically start/end terminal node. Functions as a rest node."""

    RESTAURANT_BREAKFAST: int
    """Morning dining venue. Resets fatigue and satisfies breakfast deadlines."""

    RESTAURANT_LUNCH: int
    """Midday dining venue. Resets fatigue and satisfies lunch deadlines."""

    RESTAURANT_DINNER: int
    """Evening dining venue. Resets fatigue and satisfies dinner deadlines."""

    BAR: int
    """Nightlife or social venue. Functions as a rest node."""

class TransitInfo:
    """Represents a directed weighted edge connecting two POIs in the metric graph."""

    duration: int
    """Direct travel duration in minutes between origin and destination nodes (>= 0)."""

    cost: float
    """Financial transit expenditure (fare, fuel, toll) in currency units (>= 0.0)."""

    def __init__(self, duration: int, cost: float) -> None: ...

class POI:
    """A Point of Interest (POI) vertex in the routing network.

    Encapsulates temporal operating windows, dwell duration, direct monetary cost,
    categorical classification, and the objective utility score to be maximized.
    """

    type: NodeType
    """Categorical classification driving physiological constraints and monotony tracking."""

    cost: float
    """Direct monetary visit cost in currency units (>= 0.0)."""

    score: float
    """Intrinsic utility/reward value to maximize (>= 0.0)."""

    earliest_time: int
    """Earliest allowable visit start time in minutes from midnight (e.g. 540 = 09:00)."""

    latest_time: int
    """Latest allowable departure time in minutes from midnight (e.g. 1080 = 18:00)."""

    duration: int
    """Mandatory dwell/stay duration in minutes at this POI (>= 0)."""

    is_breakfast_spot: bool
    """When true, visiting this POI satisfies circadian breakfast requirements."""

    is_lunch_spot: bool
    """When true, visiting this POI satisfies circadian lunch requirements."""

    is_dinner_spot: bool
    """When true, visiting this POI satisfies circadian dinner requirements."""

    is_mandatory: bool
    """When true, candidate itineraries omitting this POI are considered infeasible."""

    def __init__(
        self,
        type: NodeType,
        cost: float,
        score: float,
        earliest_time: int,
        latest_time: int,
        duration: int,
        is_mandatory: bool = False,
    ) -> None:
        """Constructs a POI and automatically pre-computes meal classification flags."""
        ...

class OptimizationConfig:
    """Global constraints, bounding budgets, and human-realism parameters for the solver.

    Configures both classical hard combinatorial bounds (budget, global timeline, mandatory
    terminal nodes) and physiological human-realism mechanics (circadian meal deadlines,
    fatigue degradation, idle time penalties, and category monotony damping).
    """

    max_budget: float
    """Upper bound on total expenditure (sum of visit costs + transit fares) in currency units."""

    start_node_index: Optional[int]
    """Index of mandatory starting node. If None, solver searches all feasible root candidates."""

    end_node_type: Optional[NodeType]
    """Optional required categorical type for the final node in the tour."""

    end_node_index: Optional[int]
    """Optional required specific terminal node index (e.g., returning to hotel for round-trip)."""

    end_time_limit: int
    """Global deadline in minutes from midnight for the entire itinerary. -1 disables limit."""

    breakfast_deadline: int
    """Latest allowable arrival minute for breakfast. Sentinel -1 disables constraint."""

    lunch_deadline: int
    """Latest allowable arrival minute for lunch. Sentinel -1 disables constraint."""

    dinner_deadline: int
    """Latest allowable arrival minute for dinner. Sentinel -1 disables constraint."""

    max_idle_time: int
    """Maximum allowable waiting time in minutes before a POI opens (default: 60)."""

    idle_time_penalty_rate: float
    """Objective score deduction rate per 15 minutes of idle waiting (default: 0.5)."""

    max_active_time_before_fatigue: int
    """Maximum cumulative active minutes before fatigue sets in (default: 240)."""

    fatigue_penalty_multiplier: float
    """Score attenuation multiplier applied to POIs visited under active fatigue (default: 0.6)."""

    min_meal_spacing: int
    """Minimum spacing in minutes required between consecutive meals (default: 180)."""

    monotony_threshold: int
    """Maximum visits to the same category before diminishing returns apply (default: 2)."""

    monotony_multiplier: float
    """Diminishing returns multiplier applied exponentially per subsequent visit (default: 0.5)."""

    timeout_ms: int
    """Maximum wall-clock execution time in milliseconds before search terminates (default: 5000)."""

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
    """Optimal solution summary returned by the combinatorial solver."""

    path: List[int]
    """Ordered sequence of visited POI node indices from start to terminal."""

    total_cost: float
    """Total financial expenditure accumulated (visits + transits)."""

    total_time: float
    """Total elapsed itinerary duration in minutes."""

    total_score: float
    """Maximized cumulative objective score achieved under all physiological penalties."""

def optimize_itinerary(
    pois: List[POI],
    durations: npt.NDArray[np.int32] | Any,
    costs: npt.NDArray[np.float64] | Any,
    config: OptimizationConfig,
) -> OptimizationResult:
    """Computes the optimal travel itinerary solving the Time-Constrained Orienteering Problem
    with Time Windows (TCOPTW).

    Releases the Python GIL during computation to allow multi-core concurrent execution.

    Args:
        pois: List of candidate POI objects (N <= 64).
        durations: Flattened 1D NumPy array of size N*N containing travel durations in minutes.
        costs: Flattened 1D NumPy array of size N*N containing transit costs.
        config: OptimizationConfig specifying budget, deadlines, and human-realism settings.

    Returns:
        OptimizationResult containing the optimal path and aggregate metrics.

    Raises:
        ValueError: If N > 64, if transit arrays have mismatched lengths or are not 1D,
                    or if start/end node indices are out of bounds.
    """
    ...
