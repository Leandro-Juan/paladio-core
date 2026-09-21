#include "engine.hpp"
#include <gtest/gtest.h>
#include <random>

using namespace paladio::core;

class EngineStressTest : public ::testing::Test {
protected:
  void SetUp() override {
    // Nothing special for setup, we will create things dynamically
  }
  OptimizationConfig config;
};

TEST_F(EngineStressTest, SixtyFourPOIDenseGraph) {
  // Arrange
  const int N = 64;
  std::vector<POI> pois;
  std::vector<TransitInfo> transit_times;

  // Create 64 POIs
  for (int i = 0; i < N; ++i) {
    pois.push_back({NodeType::ATTRACTION, 5.0, 10.0, 0, 1440, 20});
  }

  // Assign start/end types to make sure it doesn't fail parsing
  pois[0].type = NodeType::HOTEL;
  pois[N - 1].type = NodeType::HOTEL;

  // Create a 64x64 transit matrix (4096 elements)
  // We make all transit times small so we can visit many nodes
  for (int i = 0; i < N; ++i) {
    for (int j = 0; j < N; ++j) {
      transit_times.push_back({5, 1.0}); // 5 mins, 1 eur
    }
  }

  config.max_budget = 1000.0;
  config.start_node_index = 0;
  config.end_node_index = N - 1;
  config.end_time_limit = 1440;

  // To ensure the test finishes in a reasonable time, we use a tight budget
  // or time limit to prune branches. Branch and bound with 64 nodes is O(N!)
  // so we need to limit the actual tree depth, but we still ensure we CAN
  // bitwise map node 63.
  config.end_time_limit = 100; // Allows at most a few nodes

  // Act
  auto result = optimize_itinerary(pois, transit_times, config);

  // Assert
  // Verify it doesn't crash and returns a path
  EXPECT_GT(result.path.size(), 0);
}

TEST_F(EngineStressTest, MaximumBitmaskIndex) {
  // Arrange
  // We want to force the algorithm to visit node index 63 and verify it
  // prunes/visits correctly
  const int N = 64;
  std::vector<POI> pois(N, {NodeType::ATTRACTION, 0.0, 1.0, 0, 1440, 10});
  std::vector<TransitInfo> transit(N * N, {0, 0.0});

  pois[0].type = NodeType::HOTEL;  // start
  pois[63].type = NodeType::HOTEL; // end

  // Node 63 is the destination, but we give it a massive score so the algorithm
  // WANTS to go there
  pois[63].score = 1000000.0;

  config.start_node_index = 0;
  config.end_node_index = 63;
  config.max_budget = 1000.0;

  // We allow visiting node 0 and node 63 only (duration 0 for hotels, 10 for
  // attractions)
  config.end_time_limit = 5;

  // Act
  auto result = optimize_itinerary(pois, transit, config);

  // Assert
  // The path should be 0 -> 63
  ASSERT_EQ(result.path.size(), 2);
  EXPECT_EQ(result.path[0], 0);
  EXPECT_EQ(result.path[1], 63);
}

TEST_F(EngineStressTest, RandomTimeConstraintFuzzing) {
  // Arrange
  std::mt19937 gen(42);
  std::uniform_int_distribution<> time_dist(0, 1440);
  std::uniform_int_distribution<> dur_dist(10, 120);

  const int N = 10;
  std::vector<POI> pois;
  for (int i = 0; i < N; ++i) {
    int e = time_dist(gen);
    int l = time_dist(gen);
    if (e > l)
      std::swap(e, l);
    pois.push_back({NodeType::ATTRACTION, 10.0, 50.0, e, l, dur_dist(gen)});
  }
  pois[0].type = NodeType::HOTEL;
  pois[N - 1].type = NodeType::HOTEL;

  std::vector<TransitInfo> transit(N * N, {15, 2.0});

  config.max_budget = 1000.0;
  config.start_node_index = 0;
  config.end_node_index = N - 1;

  // Act
  auto result = optimize_itinerary(pois, transit, config);

  // Assert
  // A valid path might not be found due to random tight constraints,
  // but the engine must not crash or infinite loop.
  if (!result.path.empty()) {
    EXPECT_EQ(result.path.front(), 0);
    EXPECT_EQ(result.path.back(), N - 1);
    EXPECT_LE(result.total_cost, config.max_budget);
  }
}
