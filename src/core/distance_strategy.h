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
class DistanceComputer {
public:
  DistanceComputer() noexcept = default;

  DistanceComputer(DistanceMetric metric, size_t dimension)
      : fn_(detail::resolve_distance_fn(metric)), dim_(dimension) {
    if (!fn_) throw std::invalid_argument("DistanceComputer: invalid DistanceMetric value");
  }

  [[nodiscard]] float operator()(const float* a, const float* b) const noexcept {
    if (!fn_) [[unlikely]] return std::numeric_limits<float>::infinity();
    return fn_(a, b, dim_);
  }

  [[nodiscard]] size_t dimension() const noexcept { return dim_; }

private:
  detail::DistanceFn fn_ = nullptr;
  size_t dim_ = 0;
};

} // namespace quiverdb
