# 5-Minute Python Quickstart

This tutorial walks you through installing `paladio-core` and generating an optimal itinerary in Python.

---

Install via pip:
```bash
pip install paladio-core
```

Or install directly from the GitHub repository:
```bash
pip install git+https://github.com/Leandro-Juan/paladio-core.git
```

Or clone and build from local source:
```bash
git clone https://github.com/Leandro-Juan/paladio-core.git
cd paladio-core
pip install .
```

Verify the installation:
```python
import paladio_core
print(paladio_core.__doc__)
```

---

## 2. Defining Points of Interest (POIs)

Each candidate location in your routing network is defined as a `paladio_core.POI`:

```python
import paladio_core

# Parameters:
# 1. NodeType (ATTRACTION, HOTEL, RESTAURANT_BREAKFAST, RESTAURANT_LUNCH, RESTAURANT_DINNER, BAR)
# 2. cost (float): Entrance fee or ticket cost in currency
# 3. score (float): Objective reward value to maximize
# 4. earliest_time (int): Opening time in minutes from midnight (e.g. 540 = 09:00 AM)
# 5. latest_time (int): Closing time in minutes from midnight (e.g. 1080 = 18:00 PM)
# 6. duration (int): Dwell time required at the POI in minutes
# 7. is_mandatory (bool, optional): If True, itinerary is invalid unless this POI is visited

pois = [
    # Node 0: Hotel (Start/End location)
    paladio_core.POI(paladio_core.NodeType.HOTEL, 0.0, 0.0, 480, 1440, 0),
    
    # Node 1: Famous Museum
    paladio_core.POI(paladio_core.NodeType.ATTRACTION, 20.0, 100.0, 540, 1080, 90),
    
    # Node 2: Historic Cathedral
    paladio_core.POI(paladio_core.NodeType.ATTRACTION, 10.0, 75.0, 600, 1140, 45),
    
    # Node 3: Lunch Bistro
    paladio_core.POI(paladio_core.NodeType.RESTAURANT_LUNCH, 30.0, 60.0, 720, 900, 60),
    
    # Node 4: Scenic Park
    paladio_core.POI(paladio_core.NodeType.ATTRACTION, 0.0, 40.0, 480, 1200, 45),
    
    # Node 5: Hotel (Return destination)
    paladio_core.POI(paladio_core.NodeType.HOTEL, 0.0, 0.0, 480, 1440, 0)
]
```

---

## 3. Creating Transit Matrices

`paladio-core` expects flattened 1D NumPy arrays of size $N \times N$ for travel times (minutes) and travel costs:

```python
import numpy as np

n = len(pois)

# NxN transit duration in minutes
durations_2d = np.array([
    [ 0, 15, 20, 25, 30,  0],
    [15,  0, 10, 15, 20, 15],
    [20, 10,  0, 15, 25, 20],
    [25, 15, 15,  0, 15, 25],
    [30, 20, 25, 15,  0, 30],
    [ 0, 15, 20, 25, 30,  0]
], dtype=np.int32)

# NxN transit cost in currency
costs_2d = np.array([
    [0.0, 2.0, 3.0, 3.5, 4.0, 0.0],
    [2.0, 0.0, 1.5, 2.0, 3.0, 2.0],
    [3.0, 1.5, 0.0, 2.0, 3.5, 3.0],
    [3.5, 2.0, 2.0, 0.0, 2.5, 3.5],
    [4.0, 3.0, 3.5, 2.5, 0.0, 4.0],
    [0.0, 2.0, 3.0, 3.5, 4.0, 0.0]
], dtype=np.float64)

durations = durations_2d.flatten()
costs = costs_2d.flatten()
```

---

## 4. Configuring Global Constraints & Running the Solver

```python
# Configure optimization parameters
config = paladio_core.OptimizationConfig(
    max_budget=150.0,       # Maximum allowable total cost (transit + POI fees)
    start_node_index=0,     # Must begin at Node 0 (Hotel)
    end_node_index=5,       # Must end at Node 5 (Hotel)
    end_time_limit=1140,    # Tour must finish before 19:00 (minute 1140)
    lunch_deadline=840      # Lunch must be visited by 14:00 (minute 840)
)

# Run solver
result = paladio_core.optimize_itinerary(pois, durations, costs, config)

print("=== Optimal Itinerary Solution ===")
print(f"Path Indices:  {result.path}")
print(f"Total Score:   {result.total_score:.1f} points")
print(f"Total Cost:    {result.total_cost:.2f} EUR")
print(f"Total Elapsed: {result.total_time:.0f} minutes")
```

### Expected Output
```text
=== Optimal Itinerary Solution ===
Path Indices:  [0, 1, 3, 2, 4, 5]
Total Score:   275.0 points
Total Cost:    71.00 EUR
Total Elapsed: 395 minutes
```

Congratulations! You have solved your first multi-constraint combinatorial tour in sub-millisecond time.
