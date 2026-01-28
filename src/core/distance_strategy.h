// QuiverDB - Copyright (c) 2025 Anton Tsvetkov - MIT License
#pragma once
#include "distance.h"
#include <cstddef>
#include <limits>

namespace quiverdb {

enum class DistanceMetric { L2 = 0, COSINE = 1, DOT = 2 };

/// Backward-compatible alias: HNSWDistanceMetric is now DistanceMetric
using HNSWDistanceMetric [[deprecated("Use DistanceMetric instead")]] = DistanceMetric;

/// Negated dot product as a named function (used as distance for DOT metric)
[[nodiscard]] inline float negated_dot_product(const float* a, const float* b, size_t n) noexcept {
  return -dot_product(a, b, n);
}

/// Function pointer type for distance computation
using DistanceFn = float(*)(const float*, const float*, size_t) noexcept;

/// Resolves a DistanceMetric to its corresponding distance function pointer.
[[nodiscard]] inline DistanceFn resolve_distance_fn(DistanceMetric metric) {
  switch (metric) {
    case DistanceMetric::L2:     return &l2_sq;
    case DistanceMetric::COSINE: return &cosine_distance;
    case DistanceMetric::DOT:    return &negated_dot_product;
    default:                     return nullptr;
  }
}

/// Precomputed distance computation: stores function pointer + dimension.
class DistanceComputer {
public:
  DistanceComputer() noexcept = default;

  DistanceComputer(DistanceMetric metric, size_t dimension) noexcept
      : fn_(resolve_distance_fn(metric)), dim_(dimension) {}

  [[nodiscard]] float operator()(const float* a, const float* b) const noexcept {
    if (!fn_) return std::numeric_limits<float>::infinity();
    return fn_(a, b, dim_);
  }

  [[nodiscard]] size_t dimension() const noexcept { return dim_; }

private:
  DistanceFn fn_ = nullptr;
  size_t dim_ = 0;
};

} // namespace quiverdb
