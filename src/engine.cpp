#include "paladio/engine.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <chrono>
#include <cmath>
#include <iostream>
#include <limits>
#include <unordered_map>
#include <vector>

namespace paladio::core {

namespace {

/**
 * @brief Internal exception thrown when search duration exceeds @ref
 * OptimizationConfig::timeout_ms.
 *
 * Polled cooperatively every 1024 node evaluations (@c (node_eval_count & 1023) == 0) to avoid
 * high clock-query overhead. Caught by the root solver loop to cleanly return the incumbent best.
 */
struct TimeoutException : public std::exception {};

/**
 * @brief Fixed-size contiguous stack frame tracking state during DFS recursion.
 *
 * @details Implements strict zero-heap allocation guarantees. Memory footprint is exactly
 * 336 bytes, allowing millions of recursive states to traverse L1 cache lines without
 * triggering dynamic memory allocations (@c malloc or @c new).
 */
struct SearchState {
  uint64_t visited_mask = 0;   ///< Bitmask of visited nodes indexed by density rank.
  double current_cost = 0.0;   ///< Cumulative monetary cost incurred (visits + transits).
  int current_time = 0;        ///< Current elapsed timeline in minutes from midnight.
  double current_score = 0.0;  ///< Cumulative objective reward after physiological attenuations.
  bool had_breakfast = false;  ///< True if a valid breakfast milestone has been fulfilled.
  bool had_lunch = false;      ///< True if a valid lunch milestone has been fulfilled.
  bool had_dinner = false;     ///< True if a valid dinner milestone has been fulfilled.
  int last_meal_time = -9999;  ///< Departure minute of most recent meal (for spacing enforcement).
  int continuous_active_time = 0;  ///< Consecutive active minutes accumulated without a rest stop.
  std::array<uint8_t, 8> category_visits = {0};  ///< Frequency histogram per @ref NodeType.
  std::array<int, 64> current_path;  ///< Stack-allocated sequence of visited POI indices.
  int current_path_size = 0;         ///< Current depth/length of the trajectory.
};

/**
 * @brief Lookup key for state memoization in multi-criteria Pareto dominance pruning.
 */
struct MemoKey {
  uint64_t mask;  ///< Bitmask of visited nodes.
  int node;       ///< Current head vertex index.
  bool operator==(const MemoKey &o) const { return mask == o.mask && node == o.node; }
};

/**
 * @brief Hash function for @ref MemoKey using Golden Ratio bitwise mixing.
 */
struct MemoKeyHash {
  size_t operator()(const MemoKey &k) const {
    size_t seed = 0;
    seed ^= std::hash<uint64_t>()(k.mask) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
    seed ^= std::hash<int>()(k.node) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
    return seed;
  }
};

/**
 * @brief Multi-criteria state record stored in memoization buckets to prune dominated paths.
 */
struct MemoEntry {
  int current_time = std::numeric_limits<int>::max();               ///< Timeline arrival minute.
  double current_cost = std::numeric_limits<double>::infinity();    ///< Cumulative financial cost.
  double current_score = -std::numeric_limits<double>::infinity();  ///< Accumulated score.
  bool had_breakfast = false;                                       ///< Breakfast status flag.
  bool had_lunch = false;                                           ///< Lunch status flag.
  bool had_dinner = false;                                          ///< Dinner status flag.
  int continuous_active_time = 0;  ///< Fatigue counter at state entry.
  int last_meal_time = -9999;      ///< Departure minute of last meal.
};

/**
 * @brief Mathematical infinity representation for floating-point bounds.
 */
constexpr double INF = std::numeric_limits<double>::infinity();

/**
 * @brief Computes an admissible optimistic upper bound on achievable future score.
 *
 * @details Solves the Continuous Fractional Knapsack Problem (CFKP) relaxation over the set of
 * unvisited candidate POIs. Since candidates are pre-sorted in descending order of
 * score-to-duration density (@f$ s_i / d_i @f$), the greedy assignment is proven to yield the
 * mathematically optimal fractional upper bound.
 *
 * In addition, transit duration from the current node is optimistically lower-bounded by the global
 * minimum transit duration @p min_transit_global. Consequently, the heuristic value @f$ h(s) @f$
 * satisfies the admissibility condition (@f$ h(s) \ge h^*(s) @f$) for linear instances, ensuring
 * that pruning branches where @f$ \text{score} + h(s) \le \text{best\_score} @f$ preserves global
 * optimality.
 *
 * Iteration over unvisited nodes leverages single-cycle hardware intrinsics:
 * - @c std::countr_zero: extracts the lowest set bit index (representing the highest remaining
 * density rank).
 * - @c unvisited &= unvisited - 1: clears the lowest set bit in a single CPU instruction.
 *
 * @param visited_mask 64-bit integer bitmask of already visited nodes indexed by density rank.
 * @param current_time Current elapsed itinerary timeline in minutes from midnight.
 * @param end_time_limit Global timeline boundary in minutes from midnight.
 * @param pois Pointer to contiguous array of candidate POI definitions.
 * @param sorted_pois_by_density Array mapping density rank to POI index in @p pois.
 * @param min_transit_global Minimum transit duration between any distinct pair in the graph.
 * @param n Total number of POIs in the graph (@f$ N \le 64 @f$).
 * @return double Admissible upper bound on the maximum score achievable within remaining time.
 *
 * @complexity @f$ O(U) @f$ where @f$ U \le N @f$ is the number of remaining unvisited nodes.
 */
inline double calculate_optimistic_bound(uint64_t visited_mask, int current_time,
                                         int end_time_limit, const POI *pois,
                                         const int *sorted_pois_by_density, int min_transit_global,
                                         int n) {
  int remaining_time = end_time_limit - current_time;
  if (remaining_time <= 0)
    return 0.0;

  double optimistic_future_score = 0.0;

  uint64_t unvisited = (~visited_mask);
  if (n < 64) {
    unvisited &= ((1ULL << n) - 1);
  }

  while (unvisited) {
    int rank = std::countr_zero(unvisited);
    unvisited &= unvisited - 1;  // Clear lowest set bit

    int i = sorted_pois_by_density[rank];

    int arrival = std::max(current_time + min_transit_global, pois[i].earliest_time);
    int time_after_visit = arrival + pois[i].duration;

    if (time_after_visit > pois[i].latest_time) {
      continue;
    }

    int time_cost = time_after_visit - current_time;

    if (time_cost <= remaining_time) {
      optimistic_future_score += pois[i].score;
      remaining_time -= time_cost;
    } else {
      if (time_cost > 0) {
        optimistic_future_score +=
            pois[i].score * (static_cast<double>(remaining_time) / time_cost);
      }
      break;
    }
  }
  return optimistic_future_score;
}

/**
 * @brief Recursive Depth-First Branch & Bound exploration engine.
 *
 * @details Explores candidate permutations on the metric POI graph. Evaluates several layers
 * of pruning before expanding recursive child nodes:
 * 1. **Multi-Criteria Pareto Dominance:** Compares the state against prior visits to the same
 *    vertex under the same visited set. If an existing state achieved equal or lower time, cost,
 *    fatigue, and equal or higher score and meal coverage, the current branch is pruned.
 * 2. **Continuous Knapsack Upper Bounding:** Calls @ref calculate_optimistic_bound. If optimistic
 *    upper bound + current score cannot beat the incumbent best, the entire subtree is pruned.
 * 3. **Mandatory POI Reachability:** Calculates the minimum duration required to complete all
 *    remaining unvisited mandatory nodes; prunes if arrival exceeds @p config.end_time_limit.
 * 4. **Transition Feasibility:** Enforces budget, operating time windows, idle time tolerances,
 *    circadian meal spacing, and arrival deadlines.
 * 5. **Incumbent Solution Update:** When a valid terminal node or end condition is reached,
 *    evaluates incumbent improvement using a lexicographical tie-breaker:
 *    @f$ \text{score} \uparrow \ \succ \ \text{time} \downarrow \ \succ \ \text{cost} \downarrow
 * @f$.
 *
 * @param u Current head POI index in the trajectory.
 * @param state Contiguous stack-allocated search state (336 bytes).
 * @param pois Pointer to contiguous array of POI definitions.
 * @param transit_durations Flattened row-major @f$ N \times N @f$ transit duration matrix.
 * @param transit_costs Flattened row-major @f$ N \times N @f$ transit cost matrix.
 * @param config Global optimization parameters and physiological constraints.
 * @param sorted_pois_by_density Array mapping density rank to POI index in @p pois.
 * @param density_rank Array mapping POI index to its density rank.
 * @param min_transit_global Minimum transit duration between any distinct pair in the graph.
 * @param n Total number of POIs in the graph (@f$ N \le 64 @f$).
 * @param memo Hash map storing Pareto dominance entries bucketed by @ref MemoKey.
 * @param global_mandatory_mask Bitmask of all POIs flagged as mandatory.
 * @param best_result In-out reference storing the best incumbent solution found.
 * @param start_time Wall-clock timestamp recorded at search start.
 * @param node_eval_count Counter tracking total node visits; triggers timeout checks every 1024
 * calls.
 *
 * @throws TimeoutException When wall-clock execution time exceeds @p config.timeout_ms.
 */
void dfs(int u, SearchState &state, const POI *pois, const int *transit_durations,
         const double *transit_costs, const OptimizationConfig &config,
         const int *sorted_pois_by_density, const int *density_rank, int min_transit_global, int n,
         std::unordered_map<MemoKey, std::vector<MemoEntry>, MemoKeyHash> &memo,
         uint64_t global_mandatory_mask, OptimizationResult &best_result,
         const std::chrono::steady_clock::time_point &start_time, int &node_eval_count) {
  node_eval_count++;
  if ((node_eval_count & 1023) == 0) {
    auto now = std::chrono::steady_clock::now();
    if (std::chrono::duration_cast<std::chrono::milliseconds>(now - start_time).count() >
        config.timeout_ms) {
      throw TimeoutException();
    }
  }

  MemoKey key{state.visited_mask, u};
  auto &bucket = memo[key];

  for (auto it = bucket.begin(); it != bucket.end();) {
    auto &m = *it;

    bool old_dominates =
        (state.current_time >= m.current_time && state.current_cost >= m.current_cost &&
         state.current_score <= m.current_score + 1e-5 &&
         (m.had_breakfast || !state.had_breakfast) && (m.had_lunch || !state.had_lunch) &&
         (m.had_dinner || !state.had_dinner) &&
         state.continuous_active_time >= m.continuous_active_time &&
         m.last_meal_time == state.last_meal_time);

    if (old_dominates) {
      return;
    }

    bool new_dominates =
        (state.current_time <= m.current_time && state.current_cost <= m.current_cost &&
         state.current_score >= m.current_score - 1e-5 &&
         (state.had_breakfast || !m.had_breakfast) && (state.had_lunch || !m.had_lunch) &&
         (state.had_dinner || !m.had_dinner) &&
         state.continuous_active_time <= m.continuous_active_time &&
         state.last_meal_time == m.last_meal_time);

    if (new_dominates) {
      it = bucket.erase(it);
      continue;
    }
    ++it;
  }

  MemoEntry new_entry;
  new_entry.current_time = state.current_time;
  new_entry.current_cost = state.current_cost;
  new_entry.current_score = state.current_score;
  new_entry.had_breakfast = state.had_breakfast;
  new_entry.had_lunch = state.had_lunch;
  new_entry.had_dinner = state.had_dinner;
  new_entry.continuous_active_time = state.continuous_active_time;
  new_entry.last_meal_time = state.last_meal_time;
  bucket.push_back(new_entry);

  int effective_end_time =
      config.end_time_limit != -1 ? config.end_time_limit : std::numeric_limits<int>::max();
  double max_possible_score =
      state.current_score +
      calculate_optimistic_bound(state.visited_mask, state.current_time, effective_end_time, pois,
                                 sorted_pois_by_density, min_transit_global, n);
  if (max_possible_score < best_result.total_score - 1e-5) {
    return;
  }

  uint64_t unvisited_mandatory = global_mandatory_mask & ~state.visited_mask;
  if (unvisited_mandatory != 0 && config.end_time_limit != -1) {
    int required_time_for_mandatory = 0;
    uint64_t temp_mask = unvisited_mandatory;
    while (temp_mask) {
      int rank = std::countr_zero(temp_mask);
      temp_mask &= temp_mask - 1;
      int i = sorted_pois_by_density[rank];
      int node_dur = pois[i].duration;
      if (config.end_node_index.has_value() && i == config.end_node_index.value() &&
          pois[i].type == NodeType::HOTEL) {
        node_dur = 0;
      }
      required_time_for_mandatory += node_dur + min_transit_global;
    }
    if (state.current_time + required_time_for_mandatory > config.end_time_limit) {
      return;
    }
  }

  for (int i = 0; i < n; ++i) {
    if ((state.visited_mask & (1ULL << i)) == 0) {
      int v = sorted_pois_by_density[i];

      if (config.end_node_index.has_value() && v == config.end_node_index.value()) {
        continue;
      }

      int arrival_time_before_wait = state.current_time + transit_durations[u * n + v];
      double transit_cost = transit_costs[u * n + v];
      int arrival_time = arrival_time_before_wait;

      if (arrival_time < pois[v].earliest_time) {
        arrival_time = pois[v].earliest_time;
      }

      int idle_time = arrival_time - arrival_time_before_wait;
      if (idle_time > config.max_idle_time)
        continue;

      if (arrival_time + pois[v].duration > pois[v].latest_time) {
        continue;
      }

      double next_cost = state.current_cost + transit_cost + pois[v].cost;
      if (config.end_node_index.has_value() && config.end_node_index.value() != v) {
        int target_end = config.end_node_index.value();
        double return_cost = transit_costs[v * n + target_end];
        int return_dur = transit_durations[v * n + target_end];
        double end_node_cost =
            (pois[target_end].type == NodeType::HOTEL) ? 0.0 : pois[target_end].cost;
        if (next_cost + return_cost + end_node_cost > config.max_budget)
          continue;

        int time_after_visit = arrival_time + pois[v].duration;
        if (time_after_visit + return_dur > pois[target_end].latest_time)
          continue;
        if (config.end_time_limit != -1 && time_after_visit + return_dur > config.end_time_limit)
          continue;
      } else {
        if (next_cost > config.max_budget)
          continue;
      }

      bool is_strict_meal = (pois[v].type == NodeType::RESTAURANT_BREAKFAST ||
                             pois[v].type == NodeType::RESTAURANT_LUNCH ||
                             pois[v].type == NodeType::RESTAURANT_DINNER);
      if (is_strict_meal) {
        if (arrival_time - state.last_meal_time < config.min_meal_spacing)
          continue;
      }

      double node_score = pois[v].score;

      int next_continuous_active_time = state.continuous_active_time;
      bool is_rest_node =
          (pois[v].type == NodeType::BAR || pois[v].type == NodeType::HOTEL || is_strict_meal);
      if (is_rest_node) {
        next_continuous_active_time = 0;
      } else {
        if (state.continuous_active_time + transit_durations[u * n + v] >
            config.max_active_time_before_fatigue) {
          node_score *= config.fatigue_penalty_multiplier;
        }
        next_continuous_active_time += transit_durations[u * n + v] + pois[v].duration;
      }

      uint8_t category_count = state.category_visits[static_cast<size_t>(pois[v].type)];
      if (category_count >= config.monotony_threshold) {
        node_score *=
            std::pow(config.monotony_multiplier, category_count - config.monotony_threshold + 1);
      }

      double penalty = (idle_time / 15.0) * config.idle_time_penalty_rate;
      double next_score = state.current_score + node_score - penalty;
      int next_time = arrival_time + pois[v].duration;

      bool is_breakfast = pois[v].is_breakfast_spot;
      bool is_lunch = pois[v].is_lunch_spot;
      bool is_dinner = pois[v].is_dinner_spot;

      bool next_had_breakfast = state.had_breakfast || is_breakfast;
      bool next_had_lunch = state.had_lunch || is_lunch;
      bool next_had_dinner = state.had_dinner || is_dinner;

      bool acts_as_meal = is_strict_meal || is_breakfast || is_lunch || is_dinner;
      int next_last_meal_time =
          acts_as_meal ? arrival_time + pois[v].duration : state.last_meal_time;

      if (config.breakfast_deadline != -1 && !state.had_breakfast) {
        if (is_breakfast) {
          if (arrival_time > config.breakfast_deadline)
            continue;
        } else if (next_time > config.breakfast_deadline) {
          continue;
        }
      }
      if (config.lunch_deadline != -1 && !state.had_lunch) {
        if (is_lunch) {
          if (arrival_time > config.lunch_deadline)
            continue;
        } else if (next_time > config.lunch_deadline) {
          continue;
        }
      }
      if (config.dinner_deadline != -1 && !state.had_dinner) {
        if (is_dinner) {
          if (arrival_time > config.dinner_deadline)
            continue;
        } else if (next_time > config.dinner_deadline) {
          continue;
        }
      }

      uint64_t prev_mask = state.visited_mask;
      double prev_cost = state.current_cost;
      int prev_time = state.current_time;
      double prev_score = state.current_score;
      bool prev_breakfast = state.had_breakfast;
      bool prev_lunch = state.had_lunch;
      bool prev_dinner = state.had_dinner;
      int prev_last_meal_time = state.last_meal_time;
      int prev_continuous_active_time = state.continuous_active_time;

      state.visited_mask |= (1ULL << i);
      state.current_cost = next_cost;
      state.current_time = next_time;
      state.current_score = next_score;
      state.had_breakfast = next_had_breakfast;
      state.had_lunch = next_had_lunch;
      state.had_dinner = next_had_dinner;
      state.last_meal_time = next_last_meal_time;
      state.continuous_active_time = next_continuous_active_time;
      state.category_visits[static_cast<size_t>(pois[v].type)]++;
      state.current_path[state.current_path_size++] = v;

      dfs(v, state, pois, transit_durations, transit_costs, config, sorted_pois_by_density,
          density_rank, min_transit_global, n, memo, global_mandatory_mask, best_result, start_time,
          node_eval_count);

      state.current_path_size--;
      state.category_visits[static_cast<size_t>(pois[v].type)]--;
      state.continuous_active_time = prev_continuous_active_time;
      state.last_meal_time = prev_last_meal_time;
      state.had_dinner = prev_dinner;
      state.had_lunch = prev_lunch;
      state.had_breakfast = prev_breakfast;
      state.current_score = prev_score;
      state.current_time = prev_time;
      state.current_cost = prev_cost;
      state.visited_mask = prev_mask;
    }
  }

  bool valid_end_node = true;
  double final_cost = state.current_cost;
  int final_time = state.current_time;
  double final_score = state.current_score;
  size_t final_path_size = state.current_path_size;
  int arrival_at_end = final_time;

  if (config.end_node_index.has_value() && state.current_path_size > 0) {
    int target_end = config.end_node_index.value();
    if (state.current_path[state.current_path_size - 1] != target_end) {
      int return_dur = transit_durations[u * n + target_end];
      double return_cost = transit_costs[u * n + target_end];
      arrival_at_end = state.current_time + return_dur;
      final_time = arrival_at_end;
      final_cost += return_cost;

      int idle_time = 0;
      if (final_time < pois[target_end].earliest_time) {
        idle_time = pois[target_end].earliest_time - final_time;
        final_time = pois[target_end].earliest_time;
        arrival_at_end = pois[target_end].earliest_time;
      }

      if (idle_time > config.max_idle_time)
        valid_end_node = false;

      double idle_penalty = (idle_time / 15.0) * config.idle_time_penalty_rate;
      if (pois[target_end].type != NodeType::HOTEL) {
        final_score += pois[target_end].score - idle_penalty;
        final_cost += pois[target_end].cost;
        final_time += pois[target_end].duration;
      } else {
        final_score -= idle_penalty;
      }

      if (final_cost > config.max_budget || final_time > pois[target_end].latest_time ||
          (config.end_time_limit != -1 && final_time > config.end_time_limit)) {
        valid_end_node = false;
      }

      final_path_size++;
    } else {
      if (config.end_time_limit != -1 && final_time > config.end_time_limit) {
        valid_end_node = false;
      }
    }
  } else if (config.end_time_limit != -1 && final_time > config.end_time_limit) {
    valid_end_node = false;
  }

  if (config.end_node_type.has_value() && state.current_path_size > 0) {
    int tail_node = (final_path_size > static_cast<size_t>(state.current_path_size))
                        ? config.end_node_index.value()
                        : state.current_path[state.current_path_size - 1];
    valid_end_node = valid_end_node && (pois[tail_node].type == config.end_node_type.value());
  }

  bool had_b = state.had_breakfast;
  bool had_l = state.had_lunch;
  bool had_d = state.had_dinner;
  uint64_t full_visited_mask = state.visited_mask;

  if (config.end_node_index.has_value() &&
      final_path_size > static_cast<size_t>(state.current_path_size)) {
    int target_end = config.end_node_index.value();
    full_visited_mask |= (1ULL << density_rank[target_end]);

    if (pois[target_end].is_breakfast_spot &&
        (config.breakfast_deadline == -1 || arrival_at_end <= config.breakfast_deadline)) {
      had_b = true;
    }
    if (pois[target_end].is_lunch_spot &&
        (config.lunch_deadline == -1 || arrival_at_end <= config.lunch_deadline)) {
      had_l = true;
    }
    if (pois[target_end].is_dinner_spot &&
        (config.dinner_deadline == -1 || arrival_at_end <= config.dinner_deadline)) {
      had_d = true;
    }
  }

  if (config.breakfast_deadline != -1 && !had_b)
    valid_end_node = false;
  if (config.lunch_deadline != -1 && !had_l)
    valid_end_node = false;
  if (config.dinner_deadline != -1 && !had_d)
    valid_end_node = false;

  if ((full_visited_mask & global_mandatory_mask) != global_mandatory_mask) {
    valid_end_node = false;
  }

  if (state.current_path_size > 0 && valid_end_node) {
    bool is_better = false;
    double eps = 1e-5;

    if (final_score > best_result.total_score + eps) {
      is_better = true;
    } else if (std::abs(final_score - best_result.total_score) <= eps) {
      if (final_time < best_result.total_time) {
        is_better = true;
      } else if (final_time == best_result.total_time) {
        if (final_cost < best_result.total_cost) {
          is_better = true;
        }
      }
    }

    if (is_better) {
      best_result.path.assign(state.current_path.begin(),
                              state.current_path.begin() + state.current_path_size);
      if (final_path_size > static_cast<size_t>(state.current_path_size)) {
        best_result.path.push_back(config.end_node_index.value());
      }
      best_result.total_cost = final_cost;
      best_result.total_time = final_time;
      best_result.total_score = final_score;
    }
  }
}

}  // namespace

/**
 * @brief Public solver entry point implementing the TCOPTW Branch & Bound solver.
 *
 * @details Executes the pre-processing and root iteration pipeline:
 * 1. Validates preconditions (non-empty POIs, @f$ N \le 64 @f$, non-null matrix pointers, valid
 * indices).
 * 2. Pre-sorts candidate POIs by score-to-duration density (@f$ s_i / d_i @f$) to optimize CFKP
 * upper bounding.
 * 3. Builds density rank mappings and a global mandatory POI bitmask.
 * 4. Precomputes the global minimum transit duration @c min_transit_global to support admissible
 * bounding.
 * 5. Iterates across all feasible starting nodes (or the single designated start node),
 * initializing the zero-heap @ref SearchState and invoking @ref dfs.
 * 6. Gracefully catches @ref TimeoutException to return the incumbent best solution when timeout is
 * hit.
 */
OptimizationResult optimize_itinerary(const std::vector<POI> &pois, const int *transit_durations,
                                      const double *transit_costs,
                                      const OptimizationConfig &config) {
  if (pois.empty()) {
    OptimizationResult empty_result;
    empty_result.total_cost = 0.0;
    empty_result.total_score = 0.0;
    empty_result.total_time = 0.0;
    return empty_result;
  }

  if (pois.size() > 64) {
    throw std::invalid_argument(
        "DFS engine does not support more than 64 POIs due to bitmask limits.");
  }

  if (transit_durations == nullptr || transit_costs == nullptr) {
    throw std::invalid_argument("Transit matrices pointers must not be null.");
  }

  int n = static_cast<int>(pois.size());

  if (config.start_node_index.has_value() &&
      (config.start_node_index.value() < 0 || config.start_node_index.value() >= n)) {
    throw std::invalid_argument("start_node_index is out of range.");
  }

  if (config.end_node_index.has_value() &&
      (config.end_node_index.value() < 0 || config.end_node_index.value() >= n)) {
    throw std::invalid_argument("end_node_index is out of range.");
  }

  OptimizationResult best_result;
  best_result.total_score = -1.0;
  best_result.total_time = INF;
  best_result.total_cost = INF;

  std::vector<int> sorted_pois_by_density(n);
  for (int i = 0; i < n; ++i)
    sorted_pois_by_density[i] = i;
  std::sort(sorted_pois_by_density.begin(), sorted_pois_by_density.end(), [&pois](int a, int b) {
    double density_a = pois[a].duration > 0 ? pois[a].score / static_cast<double>(pois[a].duration)
                                            : (pois[a].score > 0 ? INF : 0.0);
    double density_b = pois[b].duration > 0 ? pois[b].score / static_cast<double>(pois[b].duration)
                                            : (pois[b].score > 0 ? INF : 0.0);
    if (density_a != density_b)
      return density_a > density_b;
    if (pois[a].is_mandatory != pois[b].is_mandatory)
      return pois[a].is_mandatory > pois[b].is_mandatory;
    return a < b;
  });

  std::vector<int> density_rank(n);
  for (int i = 0; i < n; ++i) {
    density_rank[sorted_pois_by_density[i]] = i;
  }

  uint64_t global_mandatory_mask = 0;
  for (int i = 0; i < n; ++i) {
    if (pois[i].is_mandatory) {
      global_mandatory_mask |= (1ULL << density_rank[i]);
    }
  }

  int min_transit_global = 0;
  if (n > 1) {
    min_transit_global = std::numeric_limits<int>::max();
    for (int u = 0; u < n; ++u) {
      for (int v = 0; v < n; ++v) {
        if (u != v) {
          min_transit_global = std::min(min_transit_global, transit_durations[u * n + v]);
        }
      }
    }
  }

  std::unordered_map<MemoKey, std::vector<MemoEntry>, MemoKeyHash> memo;

  auto start_time = std::chrono::steady_clock::now();
  int node_eval_count = 0;

  const POI *pois_ptr = pois.data();
  const int *sorted_pois_ptr = sorted_pois_by_density.data();
  const int *density_rank_ptr = density_rank.data();

  int start_idx = config.start_node_index.value_or(-1);

  for (int start_node = 0; start_node < n; ++start_node) {
    if (start_idx != -1 && start_node != start_idx)
      continue;

    int arrival_start = std::max(0, pois[start_node].earliest_time);
    bool is_hotel_start = (pois[start_node].type == NodeType::HOTEL);

    if (!is_hotel_start &&
        arrival_start + pois[start_node].duration > pois[start_node].latest_time) {
      continue;
    }

    double start_cost = is_hotel_start ? 0.0 : pois[start_node].cost;
    if (start_cost > config.max_budget)
      continue;

    SearchState state;
    state.visited_mask = (1ULL << density_rank[start_node]);
    state.current_cost = start_cost;

    state.current_time =
        is_hotel_start ? arrival_start : (arrival_start + pois[start_node].duration);
    state.current_score = is_hotel_start ? 0.0 : pois[start_node].score;

    state.had_breakfast = pois[start_node].is_breakfast_spot;
    state.had_lunch = false;
    state.had_dinner = false;
    state.current_path[state.current_path_size++] = start_node;

    bool is_strict_meal = (pois[start_node].type == NodeType::RESTAURANT_BREAKFAST ||
                           pois[start_node].type == NodeType::RESTAURANT_LUNCH ||
                           pois[start_node].type == NodeType::RESTAURANT_DINNER);
    if (is_strict_meal) {
      state.last_meal_time = state.current_time;
      state.continuous_active_time = 0;
    } else if (pois[start_node].type == NodeType::BAR || pois[start_node].type == NodeType::HOTEL) {
      state.continuous_active_time = 0;
    } else {
      state.continuous_active_time = pois[start_node].duration;
    }
    state.category_visits[static_cast<size_t>(pois[start_node].type)] = 1;

    if (config.breakfast_deadline != -1 && state.current_time > config.breakfast_deadline &&
        !state.had_breakfast)
      continue;
    if (config.lunch_deadline != -1 && state.current_time > config.lunch_deadline &&
        !state.had_lunch)
      continue;
    if (config.dinner_deadline != -1 && state.current_time > config.dinner_deadline &&
        !state.had_dinner)
      continue;

    try {
      dfs(start_node, state, pois_ptr, transit_durations, transit_costs, config, sorted_pois_ptr,
          density_rank_ptr, min_transit_global, n, memo, global_mandatory_mask, best_result,
          start_time, node_eval_count);
    } catch (const TimeoutException &) {
      break;
    }
  }

  if (best_result.total_score == -1.0) {
    best_result.total_cost = 0.0;
    best_result.total_score = 0.0;
    best_result.total_time = 0.0;
  }

  return best_result;
}

}  // namespace paladio::core
