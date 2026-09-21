#include "engine.hpp"
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

  m.doc() = "Paladio Continuous Travel Optimization C++ Core. Provides "
            "deterministic branch-and-bound routing.";

  py::enum_<NodeType>(m, "NodeType",
                      "Semantic categories for Points of Interest.")
      .value("ATTRACTION", NodeType::ATTRACTION)
      .value("HOTEL", NodeType::HOTEL)
      .value("RESTAURANT_BREAKFAST", NodeType::RESTAURANT_BREAKFAST)
      .value("RESTAURANT_LUNCH", NodeType::RESTAURANT_LUNCH)
      .value("RESTAURANT_DINNER", NodeType::RESTAURANT_DINNER)
      .value("BAR", NodeType::BAR)
      .export_values();

  py::class_<TransitInfo>(m, "TransitInfo", "Travel edge connecting two POIs.")
      .def(py::init<int, double>())
      .def_readwrite("duration", &TransitInfo::duration,
                     "Travel duration in minutes.")
      .def_readwrite("cost", &TransitInfo::cost, "Financial cost of travel.");

  py::class_<POI>(m, "POI",
                  "A Point of Interest node in the itinerary network.")
      .def(py::init<NodeType, double, double, int, int, int, bool>(),
           py::arg("type"), py::arg("cost"), py::arg("score"),
           py::arg("earliest_time"), py::arg("latest_time"),
           py::arg("duration"), py::arg("is_mandatory") = false)
      .def_readwrite("type", &POI::type)
      .def_readwrite("cost", &POI::cost)
      .def_readwrite("score", &POI::score)
      .def_readwrite("earliest_time", &POI::earliest_time)
      .def_readwrite("latest_time", &POI::latest_time)
      .def_readwrite("duration", &POI::duration)
      .def_readwrite("is_breakfast_spot", &POI::is_breakfast_spot)
      .def_readwrite("is_lunch_spot", &POI::is_lunch_spot)
      .def_readwrite("is_dinner_spot", &POI::is_dinner_spot)
      .def_readwrite("is_mandatory", &POI::is_mandatory);

  py::class_<OptimizationConfig>(m, "OptimizationConfig",
                                 "Global constraints for the routing problem.")
      .def(py::init<double, std::optional<int>, std::optional<NodeType>,
                    std::optional<int>, int, int, int, int, int, double, int,
                    double, int, int, double, int>(),
           py::arg("max_budget"), py::arg("start_node_index") = std::nullopt,
           py::arg("end_node_type") = std::nullopt,
           py::arg("end_node_index") = std::nullopt,
           py::arg("end_time_limit") = -1, py::arg("breakfast_deadline") = -1,
           py::arg("lunch_deadline") = -1, py::arg("dinner_deadline") = -1,
           py::arg("max_idle_time") = 60,
           py::arg("idle_time_penalty_rate") = 0.5,
           py::arg("max_active_time_before_fatigue") = 240,
           py::arg("fatigue_penalty_multiplier") = 0.6,
           py::arg("min_meal_spacing") = 180, py::arg("monotony_threshold") = 2,
           py::arg("monotony_multiplier") = 0.5, py::arg("timeout_ms") = 5000)
      .def_readwrite("max_budget", &OptimizationConfig::max_budget)
      .def_readwrite("start_node_index", &OptimizationConfig::start_node_index)
      .def_readwrite("end_node_type", &OptimizationConfig::end_node_type)
      .def_readwrite("end_node_index", &OptimizationConfig::end_node_index)
      .def_readwrite("end_time_limit", &OptimizationConfig::end_time_limit)
      .def_readwrite("breakfast_deadline",
                     &OptimizationConfig::breakfast_deadline)
      .def_readwrite("lunch_deadline", &OptimizationConfig::lunch_deadline)
      .def_readwrite("dinner_deadline", &OptimizationConfig::dinner_deadline)
      .def_readwrite("max_idle_time", &OptimizationConfig::max_idle_time)
      .def_readwrite("idle_time_penalty_rate",
                     &OptimizationConfig::idle_time_penalty_rate)
      .def_readwrite("max_active_time_before_fatigue",
                     &OptimizationConfig::max_active_time_before_fatigue)
      .def_readwrite("fatigue_penalty_multiplier",
                     &OptimizationConfig::fatigue_penalty_multiplier)
      .def_readwrite("min_meal_spacing", &OptimizationConfig::min_meal_spacing)
      .def_readwrite("monotony_threshold",
                     &OptimizationConfig::monotony_threshold)
      .def_readwrite("monotony_multiplier",
                     &OptimizationConfig::monotony_multiplier)
      .def_readwrite("timeout_ms", &OptimizationConfig::timeout_ms);

  py::class_<OptimizationResult>(m, "OptimizationResult",
                                 "Optimal path returned by the solver.")
      .def_readwrite("path", &OptimizationResult::path,
                     "Sequence of visited POI indices.")
      .def_readwrite("total_cost", &OptimizationResult::total_cost,
                     "Accumulated financial cost.")
      .def_readwrite("total_time", &OptimizationResult::total_time,
                     "Total elapsed time in minutes.")
      .def_readwrite("total_score", &OptimizationResult::total_score,
                     "Maximally accumulated score.");

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

        if (dur_buf.ndim != 1 || cost_buf.ndim != 1 ||
            dur_buf.shape[0] != cost_buf.shape[0]) {
          throw std::runtime_error("Transit matrices must be flattened 1D "
                                   "arrays of the same length.");
        }

        size_t expected_size = pois.size() * pois.size();
        if (dur_buf.shape[0] != static_cast<py::ssize_t>(expected_size)) {
          throw std::runtime_error(
              "Transit matrices must have size exactly equal to N*N where N is "
              "the number of POIs.");
        }

        int *d_ptr = static_cast<int *>(dur_buf.ptr);
        double *c_ptr = static_cast<double *>(cost_buf.ptr);

        // Release GIL for the core C++ loop to allow Python concurrent
        // execution
        py::gil_scoped_release release;
        return optimize_itinerary(pois, d_ptr, c_ptr, config);
      },
      "Optimize travel constraints (TSPTW + Knapsack). Releases GIL during "
      "computation.");
}
