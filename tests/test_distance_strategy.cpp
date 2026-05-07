// VaneDB - Copyright (c) 2025 Anton Tsvetkov - MIT License
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include "core/distance_strategy.h"
#include "core/detail/file_utils.h"

using namespace vanedb;

TEST_CASE("DistanceComputer matches raw function calls", "[distance_strategy]") {
  const size_t dim = 4;
  float a[] = {1.0f, 2.0f, 3.0f, 4.0f};
  float b[] = {5.0f, 6.0f, 7.0f, 8.0f};

  SECTION("L2") {
    DistanceComputer dc(DistanceMetric::L2, dim);
    REQUIRE(dc(a, b) == l2_sq(a, b, dim));
  }

  SECTION("Cosine") {
    DistanceComputer dc(DistanceMetric::COSINE, dim);
    REQUIRE(dc(a, b) == cosine_distance(a, b, dim));
  }

  SECTION("Dot") {
    DistanceComputer dc(DistanceMetric::DOT, dim);
    REQUIRE(dc(a, b) == -dot_product(a, b, dim));
  }
}

TEST_CASE("HNSWDistanceMetric is alias for DistanceMetric", "[distance_strategy]") {
  // Compile-time check: these must be the same type (suppress deprecation for test)
#if defined(_MSC_VER)
#  pragma warning(push)
#  pragma warning(disable: 4996)
#elif defined(__GNUC__) || defined(__clang__)
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif
  static_assert(std::is_same_v<HNSWDistanceMetric, DistanceMetric>,
                "HNSWDistanceMetric must be an alias for DistanceMetric");

  // Runtime: enum values match
  REQUIRE(static_cast<int>(HNSWDistanceMetric::L2) == static_cast<int>(DistanceMetric::L2));
  REQUIRE(static_cast<int>(HNSWDistanceMetric::COSINE) == static_cast<int>(DistanceMetric::COSINE));
  REQUIRE(static_cast<int>(HNSWDistanceMetric::DOT) == static_cast<int>(DistanceMetric::DOT));
#if defined(_MSC_VER)
#  pragma warning(pop)
#elif defined(__GNUC__) || defined(__clang__)
#  pragma GCC diagnostic pop
#endif
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
    auto tmp = std::filesystem::temp_directory_path() / "vanedb_fsync_test.tmp";
    { std::ofstream f(tmp, std::ios::binary); f << "test"; }
    REQUIRE_NOTHROW(detail::fsync_file(tmp.string()));
    std::filesystem::remove(tmp);
  }

  SECTION("on nonexistent path") {
    auto missing = std::filesystem::temp_directory_path() / "vanedb_no_such_file_xyz123.tmp";
    REQUIRE_NOTHROW(detail::fsync_file(missing.string()));
  }
}

TEST_CASE("DistanceComputer throws on invalid metric", "[distance_strategy]") {
  REQUIRE_THROWS_AS(DistanceComputer(static_cast<DistanceMetric>(99), 4),
                    std::invalid_argument);
}

TEST_CASE("DistanceComputer with zero dimension", "[distance_strategy]") {
  DistanceComputer dc(DistanceMetric::L2, 0);
  float a[] = {1.0f};
  float b[] = {2.0f};
  // Zero-dimension distance should be 0 (sum of nothing)
  REQUIRE(dc(a, b) == 0.0f);
}

TEST_CASE("sizeof(DistanceComputer) is 16 bytes", "[distance_strategy]") {
  // Field layout: size_t dim_ (8) + DistanceMetric metric_ (4) + bool valid_ (1) + 3 padding.
  // Reorder breaks this contract; revisit benchmarks before changing.
  static_assert(sizeof(DistanceComputer) == 16,
                "DistanceComputer must remain 16 bytes — affects every store embedding it");
}
