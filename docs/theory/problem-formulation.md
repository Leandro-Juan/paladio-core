# Formal Mathematical Problem Formulation (TCOPTW)

This document establishes the formal mathematical formulation of the **Time-Constrained Orienteering Problem with Time Windows (TCOPTW)** as solved by `paladio-core`.

---

## 1. Graph & Parameter Definitions

Let $G = (V, E)$ be a directed, complete metric graph where:
- $V = \{0, 1, \dots, N-1\}$ represents the set of Points of Interest (POIs).
- Node $0$ represents the origin/hotel, and node $N-1$ represents the terminal destination.
- Each POI $i \in V$ possesses:
  - An objective score/reward $s_i \ge 0$.
  - A dwell or visit duration $d_i \ge 0$.
  - A financial visit cost $c_i \ge 0$.
  - A time window $[e_i, l_i]$, where $e_i$ is the earliest allowable arrival time, and $l_i$ is the latest allowable departure time.
  - A categorical type $T_i \in \{\text{ATTRACTION}, \text{HOTEL}, \text{RESTAURANT\_BREAKFAST}, \dots\}$.
- Each directed edge $(i, j) \in E$ has:
  - A travel duration $t_{ij} \ge 0$ in minutes.
  - A transit cost $k_{ij} \ge 0$.

---

## 2. Decision Variables

Define binary decision variables:
- $x_{ij} \in \{0, 1\}$: Equals 1 if directed edge $(i, j)$ is traversed in the route, and 0 otherwise.
- $y_i \in \{0, 1\}$: Equals 1 if POI $i$ is visited, and 0 otherwise.
- $a_i \ge 0$: Continuous variable representing the arrival time at node $i$.
- $w_i \ge 0$: Continuous variable representing idle waiting time at node $i$ if $a_i < e_i$.

---

## 3. Objective Function

The primary goal is to maximize the cumulative score collected along the path, subject to optional penalty deductions for human realism:

$$\max \sum_{i \in V} s_i y_i - \sum_{i \in V} P_{\text{idle}}(w_i) - \sum_{i \in V} P_{\text{fatigue}}(i)$$

---

## 4. Operational Constraints

### 4.1 Flow Conservation
$$\sum_{j \in V \setminus \{0\}} x_{0j} = 1, \quad \sum_{i \in V \setminus \{N-1\}} x_{i, N-1} = 1$$

$$\sum_{j \in V \setminus \{k\}} x_{jk} = \sum_{j \in V \setminus \{k\}} x_{kj} = y_k \quad \forall k \in V \setminus \{0, N-1\}$$

### 4.2 Time Window & Timeline Continuity
For any traversed edge $(i, j)$ with $x_{ij} = 1$:

$$a_j \ge \max(a_i, e_i) + d_i + t_{ij}$$

$$a_i + d_i \le l_i \quad \forall i \in V \text{ such that } y_i = 1$$

$$a_{N-1} \le T_{\max}$$

where $T_{\max}$ is the global itinerary time limit (`end_time_limit`).

### 4.3 Budget Constraint
$$\sum_{i \in V} c_i y_i + \sum_{(i, j) \in E} k_{ij} x_{ij} \le B$$

where $B$ is the maximum allowable budget (`max_budget`).

### 4.4 Mandatory POI Enforcement
For any node $m \in V$ designated as mandatory ($m \in V_{\text{mandatory}}$):

$$y_m = 1$$
