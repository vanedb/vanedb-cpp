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

namespace detail {

using DistanceFn = float(*)(const float*, const float*, size_t) noexcept;

// DOT participates in the "lower is closer" convention via negation.
[[nodiscard]] inline float negated_dot_product(const float* a, const float* b, size_t n) noexcept {
  return -dot_product(a, b, n);
}

[[nodiscard]] inline DistanceFn resolve_distance_fn(DistanceMetric metric) noexcept {
  switch (metric) {
    case DistanceMetric::L2:     return &l2_sq;
    case DistanceMetric::COSINE: return &cosine_distance;
    case DistanceMetric::DOT:    return &negated_dot_product;
  }
  return nullptr;
}

} // namespace detail

// Default-constructed instances are invalid; operator() returns infinity.
// Default-constructibility is required for MMapVectorStore which assigns
// dist_ in its constructor body (after parsing metric/dim from the file).
//
// operator() switches on the stored metric (rather than calling through a
// function pointer) so the compiler can inline the SIMD distance functions
// into the hot loop. The metric is invariant per instance, so the branch
// predictor pins the case after the first call.
class DistanceComputer {
public:
  DistanceComputer() noexcept = default;

  DistanceComputer(DistanceMetric metric, size_t dimension)
      : metric_(metric), dim_(dimension), valid_(true) {
    if (!detail::resolve_distance_fn(metric)) {
      throw std::invalid_argument("DistanceComputer: invalid DistanceMetric value");
    }
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
  DistanceMetric metric_ = DistanceMetric::L2;
  size_t dim_ = 0;
  bool valid_ = false;
};

} // namespace quiverdb
