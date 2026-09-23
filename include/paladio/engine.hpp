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
 * @brief Represents the semantic category of a Point of Interest (POI).
 *
 * Used for hard constraint checking (e.g., ensuring a dinner spot is visited
 * if the user has a dinner deadline).
 */
enum class NodeType : uint8_t {
  ATTRACTION,
  HOTEL,
  RESTAURANT_BREAKFAST,
  RESTAURANT_LUNCH,
  RESTAURANT_DINNER,
  BAR
};

/**
 * @brief A Point of Interest in the travel itinerary network.
 *
 * POIs act as nodes in the branch and bound routing algorithm. They contain
 * both temporal constraints (time windows) and the objective value (score) to
 * be maximized.
 */
struct POI {
  double cost;             ///< Financial cost of visiting the POI.
  double score;            ///< Objective value to maximize for this POI.
  int earliest_time;       ///< Earliest allowable arrival time (in minutes from a
                           ///< baseline).
  int latest_time;         ///< Latest allowable departure time.
  int duration;            ///< Required duration to spend at the POI in minutes.
  NodeType type;           ///< The categorical type of the POI.
  bool is_breakfast_spot;  ///< Pre-computed flag for breakfast suitability.
  bool is_lunch_spot;      ///< Pre-computed flag for lunch suitability.
  bool is_dinner_spot;     ///< Pre-computed flag for dinner suitability.
  bool is_mandatory;       ///< True if this POI MUST be visited.

  /**
   * @brief Default constructor for STL container compatibility.
   */
  POI() = default;

  /**
   * @brief Constructs a POI and automatically pre-computes meal flags.
   *
   * @param t Categorical type of the POI.
   * @param c Financial cost.
   * @param s Objective score.
   * @param e Earliest allowable arrival time.
   * @param l Latest allowable departure time.
   * @param d Duration of the visit.
   * @param m Is this POI mandatory.
   */
  POI(NodeType t, double c, double s, int e, int l, int d, bool m = false)
      : cost(c), score(s), earliest_time(e), latest_time(l), duration(d), type(t), is_mandatory(m) {
    is_breakfast_spot = (t == NodeType::RESTAURANT_BREAKFAST);
    is_lunch_spot = (t == NodeType::RESTAURANT_LUNCH);
    is_dinner_spot = (t == NodeType::RESTAURANT_DINNER);
  }
};

/**
 * @brief Represents a directed edge weight between two POIs.
 */
struct TransitInfo {
  int duration;  ///< Travel time in minutes between the two POIs.
  double cost;   ///< Financial cost of this transit edge.
};

/**
 * @brief Configuration and global constraints for the optimization run.
 *
 * Defines the user's budget, temporal limits, and mandatory hard deadlines
 * for meals. A deadline of -1 indicates the constraint is disabled.
 */
struct OptimizationConfig {
  double max_budget;                      ///< Maximum total allowable cost (transit + POIs).
  std::optional<int> start_node_index;    ///< Optional fixed starting node (by index).
  std::optional<NodeType> end_node_type;  ///< Optional required type for the final node.
  std::optional<int> end_node_index;      ///< Optional required specific final node
                                          ///< index (Round-trip).
  int end_time_limit = -1;                ///< Global deadline for the entire itinerary.
  int breakfast_deadline = -1;          ///< Latest time by which a breakfast spot must be visited.
  int lunch_deadline = -1;              ///< Latest time by which a lunch spot must be visited.
  int dinner_deadline = -1;             ///< Latest time by which a dinner spot must be visited.
  int max_idle_time = 60;               ///< Maximum allowed waiting time before a POI opens.
  double idle_time_penalty_rate = 0.5;  ///< Penalty deducted per 15 mins of idle time.
  int max_active_time_before_fatigue = 240;  ///< Max active minutes before fatigue sets in.
  double fatigue_penalty_multiplier = 0.6;   ///< Score multiplier for POIs visited under fatigue.
  int min_meal_spacing = 180;                ///< Minimum minutes required between meals.
  int monotony_threshold = 2;                ///< Threshold for diminishing returns per category.
  double monotony_multiplier =
      0.5;                ///< Diminishing returns multiplier per subsequent category visit.
  int timeout_ms = 5000;  ///< Maximum allowed execution time in milliseconds.
};

/**
 * @brief The resulting best itinerary found by the solver.
 */
struct OptimizationResult {
  std::vector<int> path;  ///< Sequence of POI indices representing the optimal route.
  double total_cost;      ///< Total financial cost of the path.
  double total_time;      ///< Total elapsed time upon completion of the path.
  double total_score;     ///< Maximum objective score achieved.
};

/**
 * @brief Computes the optimal travel itinerary constrained by budget and time.
 *
 * Uses a Depth First Search (DFS) with a Branch & Bound continuous fractional
 * knapsack heuristic to prune suboptimal paths. It solves a variation of the
 * Time-Constrained Orienteering Problem (TCOP).
 *
 * @param pois Vector of all available POIs (max 64 due to bitmask tracking).
 * @param transit_times Flattened 2D matrix (N x N) of transit durations and
 * costs.
 * @param config User constraints, including budget and deadlines.
 * @return OptimizationResult The highest-scoring valid itinerary.
 * @throws std::invalid_argument if the number of POIs exceeds 64.
 */
[[nodiscard]] OptimizationResult optimize_itinerary(const std::vector<POI> &pois,
                                                    const int *transit_durations,
                                                    const double *transit_costs,
                                                    const OptimizationConfig &config);

#ifdef PALADIO_TESTING
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
