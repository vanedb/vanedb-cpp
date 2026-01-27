// QuiverDB - Copyright (c) 2025 Anton Tsvetkov - MIT License
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include "core/distance_strategy.h"
#include "core/detail/file_utils.h"
#include "core/hnsw_index.h"  // For MIN_LEVEL_RANDOM

using namespace quiverdb;

TEST_CASE("DistanceComputer matches raw function calls", "[distance_strategy]") {
  const size_t dim = 4;
  float a[] = {1.0f, 2.0f, 3.0f, 4.0f};
  float b[] = {5.0f, 6.0f, 7.0f, 8.0f};

  SECTION("L2") {
    DistanceComputer dc(DistanceMetric::L2, dim);
    float expected = l2_sq(a, b, dim);
    REQUIRE(dc(a, b) == expected);
  }

  SECTION("Cosine") {
    DistanceComputer dc(DistanceMetric::COSINE, dim);
    float expected = cosine_distance(a, b, dim);
    REQUIRE(dc(a, b) == expected);
  }

  SECTION("Dot") {
    DistanceComputer dc(DistanceMetric::DOT, dim);
    float expected = negated_dot_product(a, b, dim);
    REQUIRE(dc(a, b) == expected);
    REQUIRE(dc(a, b) == -dot_product(a, b, dim));
  }
}

TEST_CASE("HNSWDistanceMetric is alias for DistanceMetric", "[distance_strategy]") {
  // Compile-time check: these must be the same type (suppress deprecation for test)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
  static_assert(std::is_same_v<HNSWDistanceMetric, DistanceMetric>,
                "HNSWDistanceMetric must be an alias for DistanceMetric");

  // Runtime: enum values match
  REQUIRE(static_cast<int>(HNSWDistanceMetric::L2) == static_cast<int>(DistanceMetric::L2));
  REQUIRE(static_cast<int>(HNSWDistanceMetric::COSINE) == static_cast<int>(DistanceMetric::COSINE));
  REQUIRE(static_cast<int>(HNSWDistanceMetric::DOT) == static_cast<int>(DistanceMetric::DOT));
#pragma GCC diagnostic pop
}

TEST_CASE("Named constants have expected values", "[distance_strategy]") {
  REQUIRE(COSINE_EPSILON == 1e-12f);
  REQUIRE(MIN_LEVEL_RANDOM == 1e-9);
}

TEST_CASE("Explicit enum values are stable", "[distance_strategy]") {
  REQUIRE(static_cast<int>(DistanceMetric::L2) == 0);
  REQUIRE(static_cast<int>(DistanceMetric::COSINE) == 1);
  REQUIRE(static_cast<int>(DistanceMetric::DOT) == 2);
}

TEST_CASE("Default DistanceComputer returns infinity", "[distance_strategy]") {
  DistanceComputer dc;
  float a[] = {1.0f};
  float b[] = {2.0f};
  REQUIRE(std::isinf(dc(a, b)));
}

TEST_CASE("detail::fsync_file does not crash", "[distance_strategy]") {
  SECTION("on valid file") {
    auto tmp = std::filesystem::temp_directory_path() / "quiverdb_fsync_test.tmp";
    { std::ofstream f(tmp, std::ios::binary); f << "test"; }
    REQUIRE_NOTHROW(detail::fsync_file(tmp.string()));
    std::filesystem::remove(tmp);
  }

  SECTION("on nonexistent path") {
    REQUIRE_NOTHROW(detail::fsync_file("/nonexistent/path/should/not/crash"));
  }
}

TEST_CASE("negated_dot_product is correct", "[distance_strategy]") {
  float a[] = {1.0f, 0.0f, 0.0f};
  float b[] = {0.0f, 1.0f, 0.0f};
  REQUIRE(negated_dot_product(a, b, 3) == 0.0f);

  float c[] = {1.0f, 2.0f, 3.0f};
  float d[] = {4.0f, 5.0f, 6.0f};
  REQUIRE(negated_dot_product(c, d, 3) == -(1*4 + 2*5 + 3*6));
}

TEST_CASE("resolve_distance_fn returns correct functions", "[distance_strategy]") {
  REQUIRE(resolve_distance_fn(DistanceMetric::L2) != nullptr);
  REQUIRE(resolve_distance_fn(DistanceMetric::COSINE) != nullptr);
  REQUIRE(resolve_distance_fn(DistanceMetric::DOT) != nullptr);

  float a[] = {1.0f, 2.0f};
  float b[] = {3.0f, 4.0f};

  auto l2_fn = resolve_distance_fn(DistanceMetric::L2);
  REQUIRE(l2_fn(a, b, 2) == l2_sq(a, b, 2));
}

TEST_CASE("DistanceComputer with zero dimension", "[distance_strategy]") {
  DistanceComputer dc(DistanceMetric::L2, 0);
  float a[] = {1.0f};
  float b[] = {2.0f};
  // Zero-dimension distance should be 0 (sum of nothing)
  REQUIRE(dc(a, b) == 0.0f);
}
