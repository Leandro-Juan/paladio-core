# Branch & Bound with Fractional Knapsack Upper Bounding

This document explains the core pruning mechanism inside `paladio-core`: the **Continuous Fractional Knapsack upper bound**, and proves its mathematical admissibility.

---

## 1. The Need for Upper Bounding

In combinatorial branch-and-bound algorithms, search tree expansion must be pruned as early as possible. If an algorithm knows an upper bound $\bar{U}(S)$ on the maximum score achievable from a partial state $S$, and:

$$\text{Score}_{\text{current}}(S) + \bar{U}(S) \le \text{Score}_{\text{incumbent}}$$

then no descendant of $S$ can surpass the incumbent solution. The entire subtree rooted at $S$ can be safely discarded.

---

## 2. The Continuous Fractional Knapsack Relaxation

Given:
- Remaining available tour time $T_{\text{rem}} = T_{\max} - t_{\text{current}}$.
- Set of unvisited candidate nodes $U \subseteq V$.
- Global minimum transit duration between any two nodes $t_{\min}$.

For every unvisited node $i \in U$, visiting $i$ requires at least:

$$\Delta t_i = t_{\min} + d_i$$

where $d_i$ is the dwell duration at POI $i$. We sort candidates in descending order of **score density**:

$$\rho_i = \frac{s_i}{\Delta t_i}$$

The Continuous Knapsack upper bound is computed greedily by filling $T_{\text{rem}}$ with items of highest density:

$$\bar{U} = \sum_{k=1}^{m} s_k + \left( \frac{T_{\text{rem}} - \sum_{k=1}^{m} \Delta t_k}{\Delta t_{m+1}} \right) s_{m+1}$$

where item $m+1$ is the fractional critical item.

---

## 3. Mathematical Proof of Admissibility

> **Theorem (Admissibility of Fractional Knapsack Bound):**  
> On any linear TCOPTW instance, the Continuous Fractional Knapsack upper bound $\bar{U}(S)$ is admissible; that is, $\bar{U}(S) \ge U^*(S)$, where $U^*(S)$ is the exact optimal score achievable from state $S$.

### Proof:
1. **Time Cost Relaxation:** In the true metric graph, traveling from the current node $u$ to node $i$ incurs transit duration $t_{ui} \ge t_{\min}$. Hence, the true time consumed by visiting $i$ satisfies $\Delta t_i^{\text{true}} = t_{ui} + d_i \ge t_{\min} + d_i = \Delta t_i$. Relaxing all transit durations to $t_{\min}$ expands the feasible time budget.
2. **Time Window Relaxation:** The continuous knapsack problem ignores arrival window boundaries $[e_i, l_i]$, allowing candidates to be visited at any moment without idle waiting or tardiness penalties.
3. **Integrality Relaxation:** Relaxing the binary decision variables $y_i \in \{0, 1\}$ to continuous fractions $y_i \in [0, 1]$ expands the feasible region from a discrete subset of hypercube vertices to the entire convex hull.
4. **Greedy Optimality of LP:** By the Dantzig fractional knapsack theorem, the greedy density-ordered allocation is provably optimal for the continuous LP relaxation.

Because every relaxation expands the feasible solution space and the LP solver finds the exact optimum over that expanded space, the bound is guaranteed never to underestimate the true discrete integer optimum:

$$\bar{U}(S) \ge U^*(S)$$

Therefore, pruning when $\text{score} + \bar{U} \le \text{best}$ guarantees that the global optimal tour is never erroneously discarded on linear instances.
