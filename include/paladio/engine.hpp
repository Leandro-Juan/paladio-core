#pragma once

#include <concepts>
#include <cstdint>
#include <limits>
#include <optional>
#include <stdexcept>
#include <utility>
#include <vector>

namespace paladio::core {

/**
 * @brief Semantic category classification for a Point of Interest (POI).
 *
 * Drives physiological constraints (circadian meal windows, active fatigue reset,
 * and diminishing marginal utility under repeated category visits).
 */
enum class NodeType : uint8_t {
  ATTRACTION,  ///< Cultural, historic, or recreational attraction. Accrues objective score,
               ///< subject to time windows, category monotony decay, and fatigue penalties.
  HOTEL,       ///< Accommodation depot. Typically starting and/or ending terminal node. Functions
               ///< as a rest node (resets active fatigue) and incurs 0 cost when used as start/end
               ///< depot.
  RESTAURANT_BREAKFAST,  ///< Morning dining venue. Functions as a rest node, resets active fatigue,
                         ///< satisfies @ref OptimizationConfig::breakfast_deadline, and respects
                         ///< @ref OptimizationConfig::min_meal_spacing.
  RESTAURANT_LUNCH,      ///< Midday dining venue. Functions as a rest node, resets active fatigue,
                         ///< satisfies @ref OptimizationConfig::lunch_deadline, and respects
                         ///< @ref OptimizationConfig::min_meal_spacing.
  RESTAURANT_DINNER,     ///< Evening dining venue. Functions as a rest node, resets active fatigue,
                         ///< satisfies @ref OptimizationConfig::dinner_deadline, and respects
                         ///< @ref OptimizationConfig::min_meal_spacing.
  BAR  ///< Nightlife or social venue. Functions as a rest node (resets active fatigue).
};

/**
 * @brief A Point of Interest (POI) vertex in the routing network.
 *
 * Encapsulates temporal operating windows, dwell duration, direct monetary cost,
 * categorical classification, and the objective utility score to be maximized.
 */
struct POI {
  double cost;   ///< Direct monetary visit cost (e.g. admission fee, meal price) in currency units.
                 ///< Invariant: Non-negative (@f$ \ge 0.0 @f$).
  double score;  ///< Intrinsic utility/reward value to maximize. Invariant: Non-negative (@f$ \ge
                 ///< 0.0 @f$).
  int earliest_time;  ///< Earliest allowable visit start time in minutes from midnight (e.g.,
                      ///< @c 540 for 09:00). Arrival prior to this incurs idle waiting time.
  int latest_time;  ///< Latest allowable departure time in minutes from midnight (e.g., @c 1080 for
                    ///< 18:00). Visit must conclude before or at this time (@f$ t_{\text{arrival}}
                    ///< + \text{duration} \le \text{latest\_time} @f$).
  int duration;   ///< Mandatory dwell/stay duration in minutes at this POI. Invariant: Non-negative
                  ///< (@f$ \ge 0 @f$).
  NodeType type;  ///< Categorical classification driving physiological constraints and monotony
                  ///< tracking.
  bool is_breakfast_spot;  ///< When true, visiting this POI satisfies circadian breakfast
                           ///< requirements.
  bool is_lunch_spot;      ///< When true, visiting this POI satisfies circadian lunch requirements.
  bool is_dinner_spot;  ///< When true, visiting this POI satisfies circadian dinner requirements.
  bool is_mandatory;    ///< When true, any candidate itinerary omitting this POI is considered
                        ///< infeasible and pruned.

  /**
   * @brief Default constructor for STL container compatibility and zero-initialization.
   */
  POI() = default;

  /**
   * @brief Constructs a POI and automatically pre-computes meal classification flags.
   *
   * @param t Categorical classification (@ref NodeType).
   * @param c Direct financial visit cost (@f$ \ge 0.0 @f$).
   * @param s Objective utility score (@f$ \ge 0.0 @f$).
   * @param e Earliest opening time in minutes from midnight (@f$ 0 \le e \le l @f$).
   * @param l Latest closing time in minutes from midnight (@f$ e \le l \le 1440 @f$).
   * @param d Mandatory visit dwell duration in minutes (@f$ d \ge 0 @f$).
   * @param m Flag indicating if this POI is mandatory (@c true) or optional (@c false).
   */
  POI(NodeType t, double c, double s, int e, int l, int d, bool m = false)
      : cost(c), score(s), earliest_time(e), latest_time(l), duration(d), type(t), is_mandatory(m) {
    is_breakfast_spot = (t == NodeType::RESTAURANT_BREAKFAST);
    is_lunch_spot = (t == NodeType::RESTAURANT_LUNCH);
    is_dinner_spot = (t == NodeType::RESTAURANT_DINNER);
  }
};

/**
 * @brief Represents a directed weighted edge connecting two POIs in the metric graph.
 */
struct TransitInfo {
  int duration;  ///< Direct travel duration in minutes between origin and destination nodes (@f$
                 ///< \ge 0 @f$).
  double cost;  ///< Financial transit expenditure (fare, fuel, toll) in currency units (@f$ \ge 0.0
                ///< @f$).
};

/**
 * @brief Global constraints, bounding budgets, and human-realism parameters for the solver.
 *
 * Configures both classical hard combinatorial bounds (budget, global timeline, mandatory terminal
 * nodes) and physiological human-realism mechanics (circadian meal deadlines, fatigue degradation,
 * idle time penalties, and category monotony damping).
 */
struct OptimizationConfig {
  double max_budget;  ///< Upper bound on total expenditure (sum of visit costs + transit fares) in
                      ///< currency units.
  std::optional<int> start_node_index;    ///< Index of mandatory starting node. If @c std::nullopt,
                                          ///< solver searches all feasible root candidates.
  std::optional<NodeType> end_node_type;  ///< Optional required categorical type for the final node
                                          ///< in the tour.
  std::optional<int> end_node_index;      ///< Optional required specific terminal node index (e.g.,
                                          ///< returning to hotel depot for round-trip tour).
  int end_time_limit = -1;  ///< Global deadline in minutes from midnight for the entire itinerary.
                            ///< Sentinel value @c -1 disables the global time limit.
  int breakfast_deadline =
      -1;  ///< Latest allowable arrival minute for breakfast. Sentinel @c -1 disables.
  int lunch_deadline = -1;  ///< Latest allowable arrival minute for lunch. Sentinel @c -1 disables.
  int dinner_deadline =
      -1;                  ///< Latest allowable arrival minute for dinner. Sentinel @c -1 disables.
  int max_idle_time = 60;  ///< Maximum allowable waiting time in minutes before a POI opens.
                           ///< Trajectories requiring longer idle waiting are pruned.
  double idle_time_penalty_rate =
      0.5;  ///< Objective score deduction rate per 15 minutes of idle waiting: @f$ \Delta s =
            ///< \frac{t_{\text{idle}}}{15.0} \times \text{rate} @f$.
  int max_active_time_before_fatigue =
      240;  ///< Maximum cumulative active minutes (walking + touring) before physiological fatigue
            ///< sets in. Reset to 0 upon visiting rest nodes (@ref NodeType::HOTEL, @ref
            ///< NodeType::BAR, or meals).
  double fatigue_penalty_multiplier =
      0.6;  ///< Score attenuation multiplier applied to POIs visited under active fatigue: @f$ s'
            ///< = s \times \mu_{\text{fatigue}} @f$ where @f$ 0.0 < \mu \le 1.0 @f$.
  int min_meal_spacing =
      180;  ///< Minimum spacing in minutes required between consecutive meals (@f$
            ///< t_{\text{arrival}} - t_{\text{last\_meal}} \ge \Delta_{\text{meal}} @f$).
  int monotony_threshold =
      2;  ///< Maximum visits to the same POI category before diminishing returns apply.
  double monotony_multiplier =
      0.5;  ///< Diminishing returns multiplier applied exponentially per subsequent category visit:
            ///< @f$ s' = s \times \mu_{\text{monotony}}^{k - \tau + 1} @f$.
  int timeout_ms = 5000;  ///< Maximum wall-clock execution time in milliseconds before search
                          ///< terminates and returns incumbent best.
};

/**
 * @brief Optimal solution summary returned by the combinatorial solver.
 */
struct OptimizationResult {
  std::vector<int> path;  ///< Ordered sequence of visited POI node indices from start to terminal.
                          ///< Empty if no feasible path satisfies constraints.
  double total_cost;      ///< Total financial expenditure accumulated (visits + transits).
  double total_time;   ///< Total elapsed itinerary duration in minutes (arrival at final node minus
                       ///< start departure).
  double total_score;  ///< Maximized cumulative objective score achieved under all physiological
                       ///< penalties.
};

/**
 * @brief Computes the optimal travel itinerary solving the Time-Constrained Orienteering Problem
 * with Time Windows (TCOPTW).
 *
 * @details Solves the NP-hard TCOPTW on an arbitrary directed metric graph using a deterministic
 * Depth-First Branch & Bound search. The solver achieves sub-millisecond execution on typical
 * graphs through three cooperative systems-level techniques:
 * 1. **Continuous Fractional Knapsack Bounding:** Pre-sorts candidate POIs by score-to-duration
 *    density (@f$ s_i / d_i @f$) and computes an admissible optimistic upper bound on achievable
 *    future score. Branches whose optimistic bound cannot exceed the incumbent best solution are
 * pruned.
 * 2. **Multi-Criteria Pareto Dominance:** Maintains state memoization per @f$
 * (\text{visited\_mask}, u) @f$ bucket. Prunes states that are strictly dominated across arrival
 * time, budget, score, meal completion flags, and fatigue levels.
 * 3. **Hardware Bitmask State Transitions:** Visited nodes, mandatory milestones, and category sets
 *    are tracked via 64-bit bitmasks (`uint64_t`) with single-cycle CPU hardware intrinsics
 *    (`std::countr_zero`).
 *
 * @param pois Vector of available candidate POIs. Must satisfy @f$ N \le 64 @f$ due to 64-bit
 * integer bitmask representation.
 * @param transit_durations Flattened row-major @f$ N \times N @f$ transit duration matrix in
 * minutes. Entry @c transit_durations[u * N + v] specifies transit time from node @p u to node @p
 * v.
 * @param transit_costs Flattened row-major @f$ N \times N @f$ transit cost matrix in currency
 * units.
 * @param config Global optimization parameters, budget limits, and physiological constraints.
 * @return OptimizationResult The globally optimal (or best incumbent within timeout) valid
 * itinerary. If no valid itinerary exists, returns an empty path with zeroed metrics.
 *
 * @pre @p pois.size() must be @f$ \le 64 @f$.
 * @pre @p transit_durations and @p transit_costs must point to valid contiguous memory buffers of
 * at least @f$ N \times N @f$ elements.
 * @pre If @p config.start_node_index is set, it must be in the range @f$ [0, N-1] @f$.
 * @pre If @p config.end_node_index is set, it must be in the range @f$ [0, N-1] @f$.
 *
 * @post All returned node indices in @ref OptimizationResult::path are valid indices in @f$ [0,
 * N-1] @f$.
 * @post @ref OptimizationResult::total_cost @f$ \le @f$ @p config.max_budget.
 *
 * @throws std::invalid_argument If @p pois.size() exceeds 64, or if configured start/end indices
 * are out of range.
 *
 * @note **Reentrancy and Thread-Safety:** This function is pure and stateless; it performs no
 * dynamic heap allocations inside hot-path recursive exploration, making it safe for concurrent
 * invocation across threads.
 *
 * @complexity Worst-case @f$ O(N!) @f$; average pruned complexity is orders of magnitude smaller
 * (@f$ > 99.999\% @f$ state space reduction via knapsack bounding and dominance pruning).
 */
[[nodiscard]] OptimizationResult optimize_itinerary(const std::vector<POI> &pois,
                                                    const int *transit_durations,
                                                    const double *transit_costs,
                                                    const OptimizationConfig &config);

#ifdef PALADIO_TESTING
/**
 * @brief Testing convenience overload accepting vector of TransitInfo structs.
 *
 * @param pois Vector of candidate POIs.
 * @param transit_times Flattened @f$ N \times N @f$ vector of @ref TransitInfo edge weights.
 * @param config Solver constraints.
 * @return OptimizationResult Optimal itinerary result.
 */
inline OptimizationResult optimize_itinerary(const std::vector<POI> &pois,
                                             const std::vector<TransitInfo> &transit_times,
                                             const OptimizationConfig &config) {
  std::vector<int> durations(transit_times.size());
  std::vector<double> costs(transit_times.size());
  for (size_t i = 0; i < transit_times.size(); ++i) {
    durations[i] = transit_times[i].duration;
    costs[i] = transit_times[i].cost;
  }
  return optimize_itinerary(pois, durations.data(), costs.data(), config);
}
#endif
}  // namespace paladio::core
