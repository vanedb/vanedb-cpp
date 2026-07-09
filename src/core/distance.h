// VaneDB - Copyright (c) 2025 Anton Tsvetkov - MIT License
#pragma once
#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstddef>

#if defined(__ARM_NEON) || defined(__aarch64__)
#include <arm_neon.h>
#define VANE_ARM_NEON
#elif defined(__AVX2__)
#include <immintrin.h>
#define VANE_AVX2
#endif

#if defined(_MSC_VER)
#define RESTRICT __restrict
#else
#define RESTRICT __restrict__
#endif

namespace vanedb {

// Below this denominator (||a|| · ||b||) the vectors are treated as orthogonal.
inline constexpr float COSINE_EPSILON = 1e-12f;

#ifdef VANE_ARM_NEON
[[nodiscard]] inline float hsum(float32x4_t v) noexcept {
#if defined(__aarch64__)
  return vaddvq_f32(v);
#else
  float32x2_t r = vadd_f32(vget_low_f32(v), vget_high_f32(v));
  return vget_lane_f32(vpadd_f32(r, r), 0);
#endif
}
#endif

#ifdef VANE_AVX2
[[nodiscard]] inline float hsum(__m256 v) noexcept {
  __m128 lo = _mm256_castps256_ps128(v);
  __m128 hi = _mm256_extractf128_ps(v, 1);
  lo = _mm_add_ps(lo, hi);
  __m128 shuf = _mm_movehdup_ps(lo);
  lo = _mm_add_ps(lo, shuf);
  return _mm_cvtss_f32(_mm_add_ss(lo, _mm_movehl_ps(shuf, lo)));
}
#endif

[[nodiscard]] inline float l2_sq(const float* RESTRICT a, const float* RESTRICT b, size_t n) noexcept {
  assert(a && b);
  float sum = 0.0f;
  size_t i = 0;

#ifdef VANE_ARM_NEON
  // Four independent accumulators hide the fused multiply-add latency
  // (~4 cycles); a single-accumulator loop is latency-bound at one vector
  // per FMA-latency regardless of ALU width.
  float32x4_t acc0 = vdupq_n_f32(0.0f), acc1 = acc0, acc2 = acc0, acc3 = acc0;
  for (; i + 16 <= n; i += 16) {
    float32x4_t d0 = vsubq_f32(vld1q_f32(a + i), vld1q_f32(b + i));
    float32x4_t d1 = vsubq_f32(vld1q_f32(a + i + 4), vld1q_f32(b + i + 4));
    float32x4_t d2 = vsubq_f32(vld1q_f32(a + i + 8), vld1q_f32(b + i + 8));
    float32x4_t d3 = vsubq_f32(vld1q_f32(a + i + 12), vld1q_f32(b + i + 12));
    acc0 = vmlaq_f32(acc0, d0, d0);
    acc1 = vmlaq_f32(acc1, d1, d1);
    acc2 = vmlaq_f32(acc2, d2, d2);
    acc3 = vmlaq_f32(acc3, d3, d3);
  }
  float32x4_t acc = vaddq_f32(vaddq_f32(acc0, acc1), vaddq_f32(acc2, acc3));
  for (; i + 4 <= n; i += 4) {
    float32x4_t d = vsubq_f32(vld1q_f32(a + i), vld1q_f32(b + i));
    acc = vmlaq_f32(acc, d, d);
  }
  sum = hsum(acc);
#elif defined(VANE_AVX2)
  __m256 acc0 = _mm256_setzero_ps(), acc1 = _mm256_setzero_ps();
  __m256 acc2 = _mm256_setzero_ps(), acc3 = _mm256_setzero_ps();
  for (; i + 32 <= n; i += 32) {
    __m256 d0 = _mm256_sub_ps(_mm256_loadu_ps(a + i), _mm256_loadu_ps(b + i));
    __m256 d1 = _mm256_sub_ps(_mm256_loadu_ps(a + i + 8), _mm256_loadu_ps(b + i + 8));
    __m256 d2 = _mm256_sub_ps(_mm256_loadu_ps(a + i + 16), _mm256_loadu_ps(b + i + 16));
    __m256 d3 = _mm256_sub_ps(_mm256_loadu_ps(a + i + 24), _mm256_loadu_ps(b + i + 24));
    acc0 = _mm256_fmadd_ps(d0, d0, acc0);
    acc1 = _mm256_fmadd_ps(d1, d1, acc1);
    acc2 = _mm256_fmadd_ps(d2, d2, acc2);
    acc3 = _mm256_fmadd_ps(d3, d3, acc3);
  }
  __m256 acc = _mm256_add_ps(_mm256_add_ps(acc0, acc1), _mm256_add_ps(acc2, acc3));
  for (; i + 8 <= n; i += 8) {
    __m256 d = _mm256_sub_ps(_mm256_loadu_ps(a + i), _mm256_loadu_ps(b + i));
    acc = _mm256_fmadd_ps(d, d, acc);
  }
  sum = hsum(acc);
#endif

  for (; i < n; ++i) {
    float d = a[i] - b[i];
    sum += d * d;
  }
  return sum;
}

[[nodiscard]] inline float dot_product(const float* RESTRICT a, const float* RESTRICT b, size_t n) noexcept {
  assert(a && b);
  float sum = 0.0f;
  size_t i = 0;

#ifdef VANE_ARM_NEON
  // Same latency-hiding unroll as l2_sq.
  float32x4_t acc0 = vdupq_n_f32(0.0f), acc1 = acc0, acc2 = acc0, acc3 = acc0;
  for (; i + 16 <= n; i += 16) {
    acc0 = vmlaq_f32(acc0, vld1q_f32(a + i), vld1q_f32(b + i));
    acc1 = vmlaq_f32(acc1, vld1q_f32(a + i + 4), vld1q_f32(b + i + 4));
    acc2 = vmlaq_f32(acc2, vld1q_f32(a + i + 8), vld1q_f32(b + i + 8));
    acc3 = vmlaq_f32(acc3, vld1q_f32(a + i + 12), vld1q_f32(b + i + 12));
  }
  float32x4_t acc = vaddq_f32(vaddq_f32(acc0, acc1), vaddq_f32(acc2, acc3));
  for (; i + 4 <= n; i += 4)
    acc = vmlaq_f32(acc, vld1q_f32(a + i), vld1q_f32(b + i));
  sum = hsum(acc);
#elif defined(VANE_AVX2)
  __m256 acc0 = _mm256_setzero_ps(), acc1 = _mm256_setzero_ps();
  __m256 acc2 = _mm256_setzero_ps(), acc3 = _mm256_setzero_ps();
  for (; i + 32 <= n; i += 32) {
    acc0 = _mm256_fmadd_ps(_mm256_loadu_ps(a + i), _mm256_loadu_ps(b + i), acc0);
    acc1 = _mm256_fmadd_ps(_mm256_loadu_ps(a + i + 8), _mm256_loadu_ps(b + i + 8), acc1);
    acc2 = _mm256_fmadd_ps(_mm256_loadu_ps(a + i + 16), _mm256_loadu_ps(b + i + 16), acc2);
    acc3 = _mm256_fmadd_ps(_mm256_loadu_ps(a + i + 24), _mm256_loadu_ps(b + i + 24), acc3);
  }
  __m256 acc = _mm256_add_ps(_mm256_add_ps(acc0, acc1), _mm256_add_ps(acc2, acc3));
  for (; i + 8 <= n; i += 8)
    acc = _mm256_fmadd_ps(_mm256_loadu_ps(a + i), _mm256_loadu_ps(b + i), acc);
  sum = hsum(acc);
#endif

  for (; i < n; ++i)
    sum += a[i] * b[i];
  return sum;
}

[[nodiscard]] inline float cosine_distance(const float* RESTRICT a, const float* RESTRICT b, size_t n) noexcept {
  assert(a && b);
  float dot = 0.0f, na = 0.0f, nb = 0.0f;
  size_t i = 0;

#ifdef VANE_ARM_NEON
  // Two-way unroll on top of the three naturally independent chains.
  float32x4_t vdot0 = vdupq_n_f32(0.0f), vna0 = vdot0, vnb0 = vdot0;
  float32x4_t vdot1 = vdot0, vna1 = vdot0, vnb1 = vdot0;
  for (; i + 8 <= n; i += 8) {
    float32x4_t va0 = vld1q_f32(a + i), vb0 = vld1q_f32(b + i);
    float32x4_t va1 = vld1q_f32(a + i + 4), vb1 = vld1q_f32(b + i + 4);
    vdot0 = vmlaq_f32(vdot0, va0, vb0);
    vna0 = vmlaq_f32(vna0, va0, va0);
    vnb0 = vmlaq_f32(vnb0, vb0, vb0);
    vdot1 = vmlaq_f32(vdot1, va1, vb1);
    vna1 = vmlaq_f32(vna1, va1, va1);
    vnb1 = vmlaq_f32(vnb1, vb1, vb1);
  }
  float32x4_t vdot = vaddq_f32(vdot0, vdot1);
  float32x4_t vna = vaddq_f32(vna0, vna1);
  float32x4_t vnb = vaddq_f32(vnb0, vnb1);
  for (; i + 4 <= n; i += 4) {
    float32x4_t va = vld1q_f32(a + i), vb = vld1q_f32(b + i);
    vdot = vmlaq_f32(vdot, va, vb);
    vna = vmlaq_f32(vna, va, va);
    vnb = vmlaq_f32(vnb, vb, vb);
  }
  dot = hsum(vdot); na = hsum(vna); nb = hsum(vnb);
#elif defined(VANE_AVX2)
  __m256 vdot = _mm256_setzero_ps(), vna = _mm256_setzero_ps(), vnb = _mm256_setzero_ps();
  for (; i + 8 <= n; i += 8) {
    __m256 va = _mm256_loadu_ps(a + i), vb = _mm256_loadu_ps(b + i);
    vdot = _mm256_fmadd_ps(va, vb, vdot);
    vna = _mm256_fmadd_ps(va, va, vna);
    vnb = _mm256_fmadd_ps(vb, vb, vnb);
  }
  dot = hsum(vdot); na = hsum(vna); nb = hsum(vnb);
#endif

  for (; i < n; ++i) {
    dot += a[i] * b[i];
    na += a[i] * a[i];
    nb += b[i] * b[i];
  }

  float denom = na * nb;
  if (denom < COSINE_EPSILON) return 1.0f;
  float sim = dot / sqrtf(denom);
  return 1.0f - std::clamp(sim, -1.0f, 1.0f);
}

} // namespace vanedb
