#include "paladio/engine.hpp"

#include <iostream>

#include <pybind11/numpy.h>
#include <pybind11/operators.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;
using namespace paladio::core;

/**
 * @brief PyBind11 Module Definition
 *
 * Exposes the C++ optimization engine as a Python module (`paladio_core`).
 * Implements structured NumPy arrays for zero-copy transit matrix binding.
 */
PYBIND11_MODULE(paladio_core, m) {
  m.doc() =
      "Paladio Continuous Travel Optimization C++ Core. Provides "
      "deterministic branch-and-bound routing.";

  py::enum_<NodeType>(m, "NodeType", "Semantic categories for Points of Interest.")
      .value("ATTRACTION", NodeType::ATTRACTION)
      .value("HOTEL", NodeType::HOTEL)
      .value("RESTAURANT_BREAKFAST", NodeType::RESTAURANT_BREAKFAST)
      .value("RESTAURANT_LUNCH", NodeType::RESTAURANT_LUNCH)
      .value("RESTAURANT_DINNER", NodeType::RESTAURANT_DINNER)
      .value("BAR", NodeType::BAR)
      .export_values();

  py::class_<POI>(m, "POI", "A Point of Interest node in the itinerary network.")
      .def(py::init<NodeType, double, double, int, int, int, bool, uint8_t>(), py::arg("type"),
           py::arg("cost"), py::arg("score"), py::arg("earliest_time"), py::arg("latest_time"),
           py::arg("duration"), py::arg("is_mandatory") = false, py::arg("category_id") = 255)
      .def_readwrite("type", &POI::type)
      .def_readwrite("cost", &POI::cost)
      .def_readwrite("score", &POI::score)
      .def_readwrite("earliest_time", &POI::earliest_time)
      .def_readwrite("latest_time", &POI::latest_time)
      .def_readwrite("duration", &POI::duration)
      .def_readwrite("is_breakfast_spot", &POI::is_breakfast_spot)
      .def_readwrite("is_lunch_spot", &POI::is_lunch_spot)
      .def_readwrite("is_dinner_spot", &POI::is_dinner_spot)
      .def_readwrite("is_mandatory", &POI::is_mandatory)
      .def_readwrite("category_id", &POI::category_id);

  py::class_<OptimizationConfig>(m, "OptimizationConfig",
                                 "Global constraints for the routing problem.")
      .def(py::init<double, std::optional<int>, std::optional<NodeType>, std::optional<int>, int,
                    int, int, int, int, double, int, double, int, int, double, int, uint64_t>(),
           py::arg("max_budget"), py::arg("start_node_index") = std::nullopt,
           py::arg("end_node_type") = std::nullopt, py::arg("end_node_index") = std::nullopt,
           py::arg("end_time_limit") = -1, py::arg("breakfast_deadline") = -1,
           py::arg("lunch_deadline") = -1, py::arg("dinner_deadline") = -1,
           py::arg("max_idle_time") = 60, py::arg("idle_time_penalty_rate") = 0.5,
           py::arg("max_active_time_before_fatigue") = 240,
           py::arg("fatigue_penalty_multiplier") = 0.6, py::arg("min_meal_spacing") = 180,
           py::arg("monotony_threshold") = 2, py::arg("monotony_multiplier") = 0.5,
           py::arg("timeout_ms") = 5000, py::arg("max_nodes_expanded") = 0)
      .def_readwrite("max_budget", &OptimizationConfig::max_budget)
      .def_readwrite("start_node_index", &OptimizationConfig::start_node_index)
      .def_readwrite("end_node_type", &OptimizationConfig::end_node_type)
      .def_readwrite("end_node_index", &OptimizationConfig::end_node_index)
      .def_readwrite("end_time_limit", &OptimizationConfig::end_time_limit)
      .def_readwrite("breakfast_deadline", &OptimizationConfig::breakfast_deadline)
      .def_readwrite("lunch_deadline", &OptimizationConfig::lunch_deadline)
      .def_readwrite("dinner_deadline", &OptimizationConfig::dinner_deadline)
      .def_readwrite("max_idle_time", &OptimizationConfig::max_idle_time)
      .def_readwrite("idle_time_penalty_rate", &OptimizationConfig::idle_time_penalty_rate)
      .def_readwrite("max_active_time_before_fatigue",
                     &OptimizationConfig::max_active_time_before_fatigue)
      .def_readwrite("fatigue_penalty_multiplier", &OptimizationConfig::fatigue_penalty_multiplier)
      .def_readwrite("min_meal_spacing", &OptimizationConfig::min_meal_spacing)
      .def_readwrite("monotony_threshold", &OptimizationConfig::monotony_threshold)
      .def_readwrite("monotony_multiplier", &OptimizationConfig::monotony_multiplier)
      .def_readwrite("timeout_ms", &OptimizationConfig::timeout_ms)
      .def_readwrite("max_nodes_expanded", &OptimizationConfig::max_nodes_expanded);

  py::class_<OptimizationResult>(m, "OptimizationResult", "Optimal path returned by the solver.")
      .def_readwrite("path", &OptimizationResult::path, "Sequence of visited POI indices.")
      .def_readwrite("arrival_times", &OptimizationResult::arrival_times,
                     "Sequence of arrival times in minutes from midnight.")
      .def_readwrite("total_cost", &OptimizationResult::total_cost, "Accumulated financial cost.")
      .def_readwrite("total_time", &OptimizationResult::total_time,
                     "Total elapsed time in minutes.")
      .def_readwrite("total_score", &OptimizationResult::total_score,
                     "Maximally accumulated score.")
      .def_readwrite("nodes_expanded", &OptimizationResult::nodes_expanded,
                     "Count of branch-and-bound nodes explored.")
      .def_readwrite("timed_out", &OptimizationResult::timed_out,
                     "True if search timed out or hit node expansion limit.");

  // The core takes std::vector, but we bind it via numpy array for zero-copy
  // FFI speed
  m.def(
      "optimize_itinerary",
      [](const std::vector<POI> &pois,
         py::array_t<int, py::array::c_style | py::array::forcecast> durations,
         py::array_t<double, py::array::c_style | py::array::forcecast> costs,
         const OptimizationConfig &config) {
        py::buffer_info dur_buf = durations.request();
        py::buffer_info cost_buf = costs.request();

        if (dur_buf.ndim != 1 || cost_buf.ndim != 1 || dur_buf.shape[0] != cost_buf.shape[0]) {
          throw py::value_error(
              "Transit matrices must be flattened 1D "
              "arrays of the same length.");
        }

        size_t expected_size = pois.size() * pois.size();
        if (dur_buf.shape[0] != static_cast<py::ssize_t>(expected_size)) {
          throw py::value_error(
              "Transit matrices must have size exactly equal to N*N where N is "
              "the number of POIs.");
        }

        int *d_ptr = static_cast<int *>(dur_buf.ptr);
        double *c_ptr = static_cast<double *>(cost_buf.ptr);

        // Release GIL for the core C++ loop to allow Python concurrent execution
        OptimizationResult result;
        {
          py::gil_scoped_release release;
          result = optimize_itinerary(pois, d_ptr, c_ptr, config);
        }
        return result;
      },
      "Optimize travel constraints (TSPTW + Knapsack). Releases GIL during "
      "computation.");

  m.def(
      "solve_tsp_tour",
      [](py::array_t<int, py::array::c_style | py::array::forcecast> durations, int n,
         int start_node, int end_node, bool fixed_end) -> std::vector<int> {
        py::buffer_info dur_buf = durations.request();
        if (dur_buf.ndim != 1) {
          throw py::value_error("Transit duration matrix must be a flattened 1D array.");
        }
        size_t expected_size = static_cast<size_t>(n) * static_cast<size_t>(n);
        if (dur_buf.shape[0] != static_cast<py::ssize_t>(expected_size)) {
          throw py::value_error("Transit matrix must have size exactly equal to N*N.");
        }
        const int *d_ptr = static_cast<const int *>(dur_buf.ptr);

        std::vector<int> result;
        {
          py::gil_scoped_release release;
          result = solve_tsp_tour(d_ptr, n, start_node, end_node, fixed_end);
        }
        return result;
      },
      py::arg("durations"), py::arg("n"), py::arg("start_node") = 0, py::arg("end_node") = -1,
      py::arg("fixed_end") = false,
      "Computes a non-crossing, minimal-duration TSP tour over n nodes using C++20 local search. "
      "Releases GIL during computation.");

  m.def(
      "solve_tsptw",
      [](py::array_t<int, py::array::c_style | py::array::forcecast> durations, int n,
         py::array_t<int, py::array::c_style | py::array::forcecast> dwell_times,
         py::array_t<int, py::array::c_style | py::array::forcecast> window_starts,
         py::array_t<int, py::array::c_style | py::array::forcecast> window_ends, int start_node,
         int end_node, bool fixed_end, double penalty_weight) -> std::vector<int> {
        py::buffer_info dur_buf = durations.request();
        py::buffer_info dwell_buf = dwell_times.request();
        py::buffer_info win_s_buf = window_starts.request();
        py::buffer_info win_e_buf = window_ends.request();

        if (dur_buf.ndim != 1) {
          throw py::value_error("Transit duration matrix must be a flattened 1D array.");
        }
        size_t expected_size = static_cast<size_t>(n) * static_cast<size_t>(n);
        if (dur_buf.shape[0] != static_cast<py::ssize_t>(expected_size)) {
          throw py::value_error("Transit matrix must have size exactly equal to N*N.");
        }
        if (dwell_buf.shape[0] != static_cast<py::ssize_t>(n) ||
            win_s_buf.shape[0] != static_cast<py::ssize_t>(n) ||
            win_e_buf.shape[0] != static_cast<py::ssize_t>(n)) {
          throw py::value_error(
              "dwell_times, window_starts, and window_ends must have size equal to N.");
        }

        const int *d_ptr = static_cast<const int *>(dur_buf.ptr);
        const int *dwell_ptr = static_cast<const int *>(dwell_buf.ptr);
        const int *win_s_ptr = static_cast<const int *>(win_s_buf.ptr);
        const int *win_e_ptr = static_cast<const int *>(win_e_buf.ptr);

        std::vector<int> result;
        {
          py::gil_scoped_release release;
          result = solve_tsptw(d_ptr, n, dwell_ptr, win_s_ptr, win_e_ptr, start_node, end_node,
                               fixed_end, penalty_weight);
        }
        return result;
      },
      py::arg("durations"), py::arg("n"), py::arg("dwell_times"), py::arg("window_starts"),
      py::arg("window_ends"), py::arg("start_node") = 0, py::arg("end_node") = -1,
      py::arg("fixed_end") = false, py::arg("penalty_weight") = 100.0,
      "Computes a global TSPTW tour over n nodes with soft time-window penalties using C++20 local "
      "search. "
      "Releases GIL during computation.");
}
