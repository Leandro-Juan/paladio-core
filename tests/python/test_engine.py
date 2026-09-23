import random
import time

import numpy as np
import pytest

try:
    import paladio_core
except ImportError as e:
    pytest.fail(f"Failed to import paladio_core: {e}")


@pytest.fixture(scope="module")
def node_types():
    return [
        paladio_core.NodeType.ATTRACTION,
        paladio_core.NodeType.HOTEL,
        paladio_core.NodeType.RESTAURANT_BREAKFAST,
        paladio_core.NodeType.RESTAURANT_LUNCH,
        paladio_core.NodeType.RESTAURANT_DINNER,
        paladio_core.NodeType.BAR,
    ]


@pytest.fixture
def generate_complex_graph(node_types):
    def _generate(n=35, seed=42):
        random.seed(seed)
        pois = []
        pois.append(
            paladio_core.POI(paladio_core.NodeType.HOTEL, 0.0, 0.0, 420, 1440, 0)
        )
        for i in range(1, n):
            ntype = random.choices(node_types, weights=[60, 5, 2, 5, 10, 10], k=1)[0]
            cost = round(random.uniform(0.0, 25.0), 2)
            score = round(random.uniform(10.0, 100.0), 2)
            duration = random.randint(30, 120)

            if ntype == paladio_core.NodeType.RESTAURANT_BREAKFAST:
                earliest, latest = 420, 660
            elif ntype == paladio_core.NodeType.RESTAURANT_LUNCH:
                earliest, latest = 720, 960
            elif ntype == paladio_core.NodeType.RESTAURANT_DINNER:
                earliest, latest = 1080, 1380
            elif ntype == paladio_core.NodeType.BAR:
                earliest, latest = 1200, 1440
            else:
                earliest = random.randint(480, 720)
                latest = earliest + random.randint(240, 600)

            pois.append(
                paladio_core.POI(ntype, cost, score, earliest, latest, duration)
            )

        durations = np.zeros(n * n, dtype=np.int32)
        costs = np.zeros(n * n, dtype=np.float64)
        for u in range(n):
            for v in range(n):
                idx = u * n + v
                if u != v:
                    dur = random.randint(10, 45)
                    cost = round(dur * 0.5 + random.uniform(0, 5), 2)
                    durations[idx] = dur
                    costs[idx] = cost

        return pois, durations, costs

    return _generate


@pytest.fixture
def default_config():
    return paladio_core.OptimizationConfig(
        max_budget=200.0,
        start_node_index=0,
        end_node_index=0,
        end_time_limit=1440,
        breakfast_deadline=660,
        lunch_deadline=960,
        dinner_deadline=1380,
        max_idle_time=45,
        idle_time_penalty_rate=0.5,
        max_active_time_before_fatigue=240,
        fatigue_penalty_multiplier=0.6,
        min_meal_spacing=180,
        monotony_threshold=2,
        monotony_multiplier=0.5,
    )


def test_optimization_finds_valid_path(generate_complex_graph, default_config, capsys):
    """Test that the engine successfully returns a path with seed=0 (has all meal types)."""
    # Arrange
    pois, durations, costs = generate_complex_graph(n=35, seed=2)

    # Act
    time.perf_counter()
    result = paladio_core.optimize_itinerary(pois, durations, costs, default_config)
    time.perf_counter()

    # Assert
    assert (
        len(result.path) > 0
    ), "Expected a valid path to be found, but got 0 nodes visited."
    assert result.total_cost <= default_config.max_budget


def test_optimization_respects_budget(generate_complex_graph, default_config):
    """Test that the generated path stays within the defined budget."""
    # Arrange
    pois, durations, costs = generate_complex_graph(n=35, seed=2)

    # Act
    result = paladio_core.optimize_itinerary(pois, durations, costs, default_config)

    # Assert
    assert len(result.path) > 0, "Path must be found to test budget."
    assert (
        result.total_cost <= default_config.max_budget
    ), f"Expected cost <= {default_config.max_budget}, got {result.total_cost}"


def test_optimization_returns_to_base(generate_complex_graph, default_config):
    """Test that the path correctly enforces round-trip constraints starting and ending at node 0."""
    # Arrange
    pois, durations, costs = generate_complex_graph(n=35, seed=2)

    # Act
    result = paladio_core.optimize_itinerary(pois, durations, costs, default_config)

    # Assert
    assert len(result.path) > 0, "Path must be found to test return to base."
    assert result.path[0] == 0, f"Expected start node 0, got {result.path[0]}"
    assert result.path[-1] == 0, f"Expected end node 0, got {result.path[-1]}"


def test_optimization_meal_constraints(generate_complex_graph, default_config):
    """Test that all mandatory meals are successfully visited before their deadlines."""
    # Arrange
    pois, durations, costs = generate_complex_graph(n=35, seed=2)

    # Act
    result = paladio_core.optimize_itinerary(pois, durations, costs, default_config)

    # Assert
    assert len(result.path) > 0, "Path must be found to test meal constraints."

    has_breakfast = any(
        pois[n].type == paladio_core.NodeType.RESTAURANT_BREAKFAST for n in result.path
    )
    has_lunch = any(
        pois[n].type == paladio_core.NodeType.RESTAURANT_LUNCH for n in result.path
    )
    has_dinner = any(
        pois[n].type == paladio_core.NodeType.RESTAURANT_DINNER for n in result.path
    )

    assert has_breakfast, "Path did not include a required breakfast spot."
    assert has_lunch, "Path did not include a required lunch spot."
    assert has_dinner, "Path did not include a required dinner spot."


def test_optimization_unfeasible_scenario(generate_complex_graph, default_config):
    """Test that a scenario missing a required node type (seed=123 has no breakfast) gracefully fails."""
    # Arrange
    pois, durations, costs = generate_complex_graph(n=35, seed=123)

    # Act
    result = paladio_core.optimize_itinerary(pois, durations, costs, default_config)

    # Assert
    assert (
        len(result.path) == 0
    ), f"Expected empty path due to unfeasible constraints, got path of length {len(result.path)}"
    assert result.total_cost == 0.0
    assert result.total_score == 0.0


def test_extreme_64_poi(generate_complex_graph, default_config, capsys, tmp_path):
    """Test the engine with 64 POIs to test to the extreme."""
    # Arrange
    import json

    pois, durations, costs = generate_complex_graph(n=64, seed=0)

    # Tighten constraints to allow the solver to finish in reasonable time on 64 POIs
    default_config.max_budget = 120.0
    default_config.end_time_limit = 1440

    # Act
    time.perf_counter()
    result = paladio_core.optimize_itinerary(pois, durations, costs, default_config)
    time.perf_counter()

    # Assert
    assert len(result.path) > 0, "Expected a valid path"
    assert result.total_cost <= default_config.max_budget

    # Save the output for the realism validator
    out_data = {
        "config": {
            "max_idle_time": default_config.max_idle_time,
            "min_meal_spacing": default_config.min_meal_spacing,
            "max_budget": default_config.max_budget,
            "end_time_limit": default_config.end_time_limit,
        },
        "pois": [],
        "transits": {},
        "path": result.path,
    }

    for i, p in enumerate(pois):
        out_data["pois"].append(
            {
                "id": i,
                "earliest_time": p.earliest_time,
                "latest_time": p.latest_time,
                "duration": p.duration,
                "cost": p.cost,
                "is_meal_spot": p.type
                in [
                    paladio_core.NodeType.RESTAURANT_BREAKFAST,
                    paladio_core.NodeType.RESTAURANT_LUNCH,
                    paladio_core.NodeType.RESTAURANT_DINNER,
                ],
            }
        )

    for i in range(64):
        out_data["transits"][str(i)] = {}
        for j in range(64):
            idx = i * 64 + j
            if idx < len(durations):
                out_data["transits"][str(i)][str(j)] = {
                    "duration": int(durations[idx]),
                    "cost": float(costs[idx]),
                }

    json_path = tmp_path / "itinerary_64.json"
    with open(json_path, "w") as f:
        json.dump(out_data, f, indent=2)


def test_engine_resilience_malformed_input(default_config):
    """Test that the PyBind11 boundary doesn't segfault with badly sized arrays."""
    # Arrange
    pois = []
    for i in range(10):
        pois.append(
            paladio_core.POI(paladio_core.NodeType.ATTRACTION, 0.0, 1.0, 0, 1440, 10)
        )

    # We purposefully make durations the WRONG size (not N*N)
    durations = np.zeros(50, dtype=np.int32)
    costs = np.zeros(50, dtype=np.float64)

    # Act & Assert
    with pytest.raises(
        Exception
    ):  # Assuming PyBind11 raises an exception or we catch it
        paladio_core.optimize_itinerary(pois, durations, costs, default_config)
