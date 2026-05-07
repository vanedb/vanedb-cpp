// VaneDB - Copyright (c) 2025 Anton Tsvetkov - MIT License
#pragma once
#include <cstddef>

#if defined(__APPLE__)
#include <TargetConditionals.h>
#if TARGET_OS_MAC || TARGET_OS_IPHONE
#define VANE_HAS_METAL 1
#endif
#endif

#if defined(__CUDACC__) || defined(VANE_CUDA_ENABLED)
#define VANE_HAS_CUDA 1
#endif

namespace vanedb {
namespace gpu {

enum class Backend { NONE, METAL, CUDA };

inline Backend available() {
#if defined(VANE_HAS_METAL)
  return Backend::METAL;
#elif defined(VANE_HAS_CUDA)
  return Backend::CUDA;
#else
  return Backend::NONE;
#endif
}

constexpr size_t GPU_THRESHOLD = 50000;
inline bool use_gpu(size_t n, size_t d) { return available() != Backend::NONE && n * d >= GPU_THRESHOLD; }

} // namespace gpu
} // namespace vanedb
