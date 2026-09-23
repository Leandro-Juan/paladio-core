# How-To: Configure Human Realism & Physiological Constraints

Academic orienteering algorithms typically produce dense, robotic routes: tourists are scheduled for back-to-back museums without lunch, forced to wait idle outside closed venues, or subjected to 8 continuous hours of walking.

`paladio-core` incorporates physiological and pacing constraints directly into the branch-and-bound evaluation. This guide demonstrates how to configure them.

---

## 1. Active-Time Fatigue Modeling

Tourists experience cumulative physical and cognitive fatigue during extended active travel. When active travel (walking + visiting) exceeds a user's comfort threshold without a resting break, subsequent POI scores undergo multiplicative decay:

```python
config = paladio_core.OptimizationConfig(
    max_budget=200.0,
    # After 240 minutes (4 hours) of continuous activity without rest:
    max_active_time_before_fatigue=240,
    # Subsequent POI scores are penalized by 40% (multiplier 0.60):
    fatigue_penalty_multiplier=0.60
)
```

### Resting Nodes That Reset Fatigue
In `paladio-core`, reaching any POI with categorical type `NodeType.HOTEL` or `NodeType.BAR` resets `continuous_active_time` back to 0.

---

## 2. Circadian Meal Windows & Minimum Spacing

To guarantee a realistic day, travelers need scheduled meals at appropriate hours, while avoiding back-to-back dining:

```python
config = paladio_core.OptimizationConfig(
    max_budget=200.0,
    # Circadian deadlines (in minutes from midnight):
    breakfast_deadline=660,   # Must reach breakfast by 11:00 AM (minute 660)
    lunch_deadline=870,       # Must reach lunch by 14:30 PM (minute 870)
    dinner_deadline=1290,     # Must reach dinner by 21:30 PM (minute 1290)
    
    # Minimum required spacing between meals (in minutes):
    min_meal_spacing=180      # At least 3 hours between meals
)
```

If a candidate branch schedules a second meal within 180 minutes of the previous one, the solver rejects the candidate branch during feasibility pruning.

---

## 3. Idle-Time Pacing Penalties

When arriving at a POI before its opening time window $e_i$, travelers must wait idle. While short rests can be pleasant, long delays ruin itineraries:

```python
config = paladio_core.OptimizationConfig(
    max_budget=200.0,
    # Maximum allowed waiting time before an attraction opens (minutes):
    max_idle_time=45,
    # Score penalty deducted per 15 minutes of idle waiting:
    idle_time_penalty_rate=0.75
)
```

---

## 4. Category Monotony & Diminishing Utility

Visiting five historic churches or four art galleries on the same day causes diminishing marginal enjoyment. The engine penalizes repetitive visits per category:

```python
config = paladio_core.OptimizationConfig(
    max_budget=200.0,
    # Visits beyond the 2nd attraction of the same category suffer diminishing returns:
    monotony_threshold=2,
    # Score multiplier applied to subsequent visits:
    monotony_multiplier=0.50
)
```
