#include "engine.hpp"
#include "real_pois_data.hpp"
#include <algorithm>
#include <fstream>
#include <gtest/gtest.h>
#include <iostream>
#include <string>

using namespace paladio::core;
using namespace paladio::core::test_data;

class ItineraryEngineRealDataTest : public ::testing::Test {
protected:
  void SetUp() override {
    pois = get_real_pois();
    transits = get_real_transits();
    config = get_real_config();
    names = get_real_poi_names();
  }

  std::vector<POI> pois;
  std::vector<TransitInfo> transits;
  OptimizationConfig config;
  std::vector<std::string> names;

  void write_result_to_json(const OptimizationResult &result) {
    std::ofstream out("real_itinerary_output.json");
    ASSERT_TRUE(out.is_open())
        << "Failed to open real_itinerary_output.json for writing.";

    out << "{\n";
    out << "  \"total_cost\": " << result.total_cost << ",\n";
    out << "  \"total_time\": " << result.total_time << ",\n";
    out << "  \"total_score\": " << result.total_score << ",\n";
    out << "  \"path\": [\n";
    int current_time = 0;
    int num_nodes = pois.size();

    for (size_t i = 0; i < result.path.size(); ++i) {
      int node = result.path[i];
      int arrival_time = 0;
      int transit_dur = 0;
      int idle_time = 0;
      int start_time = 0;
      int end_time = 0;

      if (i == 0) {
        arrival_time = pois[node].earliest_time;
        start_time = pois[node].earliest_time;
        end_time = start_time + pois[node].duration;
        current_time = end_time;
      } else {
        int prev_node = result.path[i - 1];
        transit_dur = transits[prev_node * num_nodes + node].duration;
        int arrival_time_before_wait = current_time + transit_dur;
        arrival_time =
            std::max(arrival_time_before_wait, pois[node].earliest_time);
        idle_time = arrival_time - arrival_time_before_wait;
        start_time = arrival_time;
        end_time = start_time + pois[node].duration;
        current_time = end_time;
      }

      auto format_time = [](int total_minutes) {
        int h = total_minutes / 60;
        int m = total_minutes % 60;
        std::string hs = std::to_string(h);
        if (hs.length() < 2)
          hs = "0" + hs;
        std::string ms = std::to_string(m);
        if (ms.length() < 2)
          ms = "0" + ms;
        return hs + ":" + ms;
      };

      out << "    {\n";
      out << "      \"id\": " << node << ",\n";
      out << "      \"name\": \"" << names[node] << "\",\n";
      out << "      \"arrival_time\": \"" << format_time(arrival_time)
          << "\",\n";
      out << "      \"start_time\": \"" << format_time(start_time) << "\",\n";
      out << "      \"end_time\": \"" << format_time(end_time) << "\",\n";
      out << "      \"duration_mins\": " << pois[node].duration << ",\n";
      out << "      \"transit_time_mins\": " << transit_dur << ",\n";
      out << "      \"idle_time_mins\": " << idle_time << "\n";
      out << "    }";
      if (i < result.path.size() - 1) {
        out << ",";
      }
      out << "\n";
    }
    out << "  ]\n";
    out << "}\n";
    out.close();

    std::cout << "Successfully wrote output to real_itinerary_output.json"
              << std::endl;
  }
};

TEST_F(ItineraryEngineRealDataTest, RealDataProducesValidItinerary) {
  // Arrange
  ASSERT_FALSE(pois.empty());
  ASSERT_FALSE(transits.empty());

  // Ensure the size of transits matches N * N
  ASSERT_EQ(transits.size(), pois.size() * pois.size());

  // Act
  auto result = optimize_itinerary(pois, transits, config);

  // Assert
  // The path should be populated with at least the start node
  ASSERT_GT(result.path.size(), 0);

  // Should respect the max budget constraint
  EXPECT_LE(result.total_cost, config.max_budget);

  // Should start and end at the correct node if specified
  if (config.start_node_index.has_value()) {
    EXPECT_EQ(result.path.front(), config.start_node_index.value());
  }
  if (config.end_node_index.has_value() && result.path.size() > 1) {
    EXPECT_EQ(result.path.back(), config.end_node_index.value());
  }

  // Total cost should be non-negative
  EXPECT_GE(result.total_cost, 0.0);

  // Write to JSON
  write_result_to_json(result);
}

TEST_F(ItineraryEngineRealDataTest, StrictBudgetPruningWithRealData) {
  // Arrange
  config.max_budget = 5.0; // Restrict budget severely

  // Act
  auto result = optimize_itinerary(pois, transits, config);

  // Assert
  EXPECT_GT(result.path.size(), 0);
  EXPECT_LE(result.total_cost, 5.0);
}
