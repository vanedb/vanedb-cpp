// QuiverDB - Copyright (c) 2025 Anton Tsvetkov - MIT License
#pragma once
#include "distance.h"
#include <cstddef>
#include <limits>
#include <stdexcept>

namespace quiverdb {

enum class DistanceMetric { L2 = 0, COSINE = 1, DOT = 2 };

using HNSWDistanceMetric
    [[deprecated("Use quiverdb::DistanceMetric (core/distance_strategy.h). HNSWDistanceMetric is removed in v0.3.0.")]]
    = DistanceMetric;

// Default-constructed instances are invalid; operator() returns infinity.
// Default-constructibility is required for MMapVectorStore which assigns
// dist_ in its constructor body (after parsing metric/dim from the file).
//
// operator() dispatches via switch (not a function pointer) so the SIMD
// distance functions inline into the hot loop. Don't refactor this back.
class DistanceComputer {
public:
  DistanceComputer() noexcept = default;

  DistanceComputer(DistanceMetric metric, size_t dimension)
      : dim_(dimension), metric_(metric), valid_(true) {
    switch (metric) {
      case DistanceMetric::L2:
      case DistanceMetric::COSINE:
      case DistanceMetric::DOT:
        return;
    }
    throw std::invalid_argument("DistanceComputer: invalid DistanceMetric value");
  }

  [[nodiscard]] float operator()(const float* a, const float* b) const noexcept {
    if (!valid_) [[unlikely]] return std::numeric_limits<float>::infinity();
    switch (metric_) {
      case DistanceMetric::L2:     return l2_sq(a, b, dim_);
      case DistanceMetric::COSINE: return cosine_distance(a, b, dim_);
      case DistanceMetric::DOT:    return -dot_product(a, b, dim_);
    }
    return std::numeric_limits<float>::infinity();
  }

  [[nodiscard]] size_t dimension() const noexcept { return dim_; }

private:
  size_t dim_ = 0;
  DistanceMetric metric_ = DistanceMetric::L2;
  bool valid_ = false;
};

} // namespace quiverdb
