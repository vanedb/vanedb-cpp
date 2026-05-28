# VaneDB Guide

Complete documentation for VaneDB - the embeddable vector database for edge AI.

## Table of Contents

- [Performance](#performance)
- [C++ API](#c-api)
- [Python Bindings](#python-bindings)
- [Building](#building)
- [Mobile Development](#mobile-development)
- [Architecture](#architecture)
- [CI/CD Pipeline](#cicd-pipeline)
- [Known Limitations](#known-limitations)
- [Roadmap](#roadmap)

---

## Performance

| Metric | Value | Platform |
|--------|-------|----------|
| L2 Distance (768d) | ~100ns | Apple Silicon |
| Dot Product (768d) | ~95ns | Apple Silicon |
| Cosine Distance (768d) | ~115ns | Apple Silicon |
| SIMD Speedup | 3.8x vs scalar | ARM NEON |
| GPU Speedup | 3.9x at 500K vectors | Metal |
| Throughput | 10M+ ops/sec | M-series Mac |

---

## C++ API

### Distance Calculations

```cpp
#include "core/distance.h"

// 768-dimensional vectors (e.g., OpenAI embeddings)
float vec_a[768] = {/* ... */};
float vec_b[768] = {/* ... */};

// L2 squared distance (auto-selects SIMD implementation)
float l2_dist = vanedb::l2_sq(vec_a, vec_b, 768);

// Cosine distance (best for embeddings)
float cos_dist = vanedb::cosine_distance(vec_a, vec_b, 768);

// Dot product (for maximum inner product search)
float dot = vanedb::dot_product(vec_a, vec_b, 768);
```

### VectorStore (Brute-force k-NN)

```cpp
#include "core/vector_store.h"

// Create a store for 768-dimensional vectors using cosine distance
vanedb::VectorStore store(768, vanedb::DistanceMetric::COSINE);

// Add vectors with unique IDs
float doc1[768] = {/* ... */};
float doc2[768] = {/* ... */};
store.add(1, doc1);
store.add(2, doc2);

// Search for 5 nearest neighbors
float query[768] = {/* ... */};
auto results = store.search(query, 5);

for (const auto& result : results) {
    std::cout << "ID: " << result.id
              << " Distance: " << result.distance << "\n";
}
```

### HNSWIndex (Approximate Nearest Neighbor)

For large datasets, use `HNSWIndex` for much faster search:

```cpp
#include "core/hnsw_index.h"

// Create HNSW index
vanedb::HNSWIndex index(768, vanedb::DistanceMetric::COSINE, 100000);

// Add vectors
index.add(1, doc1);

// Search
auto results = index.search(query, 5);

// Save and Load
index.save("my_index.bin");
auto loaded_index = vanedb::HNSWIndex::load("my_index.bin");
```

### MMapVectorStore (Memory-Mapped)

For datasets larger than RAM, use `MMapVectorStore` for zero-copy file access:

```cpp
#include "core/mmap_vector_store.h"

// Build and save vectors to disk
vanedb::MMapVectorStoreBuilder builder(768, vanedb::DistanceMetric::COSINE);
builder.add(1, doc1);
builder.add(2, doc2);
builder.save("vectors.bin");

// Load with memory-mapping (zero-copy, instant load)
vanedb::MMapVectorStore store("vectors.bin");
auto results = store.search(query, 5);
```

---

## Python Bindings

### Installation (from source)

```bash
git clone https://github.com/tsvet01/vanedb.git
cd vanedb
cmake -B build -DCMAKE_BUILD_TYPE=Release -DVANEDB_BUILD_PYTHON=ON
cmake --build build --parallel

export PYTHONPATH=$PWD/build:$PYTHONPATH
```

### Usage

```python
import vanedb_py as vanedb
import numpy as np

# Check version
print(vanedb.__version__)  # "0.1.0"

# === HNSW Index (approximate, fastest for large datasets) ===
index = vanedb.HNSWIndex(128, vanedb.DistanceMetric.COSINE)
vec = np.random.rand(128).astype(np.float32)
index.add(1, vec)
ids, dists = index.search(vec, 10)
index.save("index.bin")

# === VectorStore (exact k-NN, thread-safe) ===
store = vanedb.VectorStore(128, vanedb.DistanceMetric.COSINE)
store.add(1, vec)
store.add(2, np.random.rand(128).astype(np.float32))
ids, dists = store.search(vec, 5)

# === MMapVectorStore (memory-mapped, for large datasets) ===
builder = vanedb.MMapVectorStoreBuilder(128, vanedb.DistanceMetric.L2)
for i in range(1000):
    builder.add(i, np.random.rand(128).astype(np.float32))
builder.save("vectors.bin")

mmap_store = vanedb.MMapVectorStore("vectors.bin")  # Instant load
ids, dists = mmap_store.search(vec, 10)
```

---

## Building

### Prerequisites

- CMake 3.20+
- C++20 compiler (Clang 17+ / GCC 11+ / MSVC 19.30+)
- Python 3.9+ (optional, for bindings)
- Git (for fetching dependencies)

### Basic Build

```bash
mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . -j

# Run tests
ctest --output-on-failure

# Run benchmarks
./bench_distance --benchmark_min_time=0.1s
./bench_vector_store --benchmark_min_time=0.1s
./bench_hnsw_index --benchmark_min_time=0.1s
```

### Build Options

| Option | Default | Description |
|--------|---------|-------------|
| `VANEDB_BUILD_TESTS` | ON | Build test suite |
| `VANEDB_BUILD_BENCHMARKS` | ON | Build benchmarks |
| `VANEDB_BUILD_PYTHON` | ON | Build Python bindings |
| `VANEDB_BUILD_EXAMPLES` | ON | Build examples |
| `VANEDB_BUILD_METAL` | OFF | Build Metal GPU support (macOS) |
| `VANEDB_BUILD_CUDA` | OFF | Enable CUDA language (experimental — kernel source is not yet wired into a build target) |

---

## Mobile Development

### iOS

```bash
cmake -B build-ios \
  -DCMAKE_SYSTEM_NAME=iOS \
  -DCMAKE_OSX_DEPLOYMENT_TARGET=13.0 \
  -DCMAKE_OSX_ARCHITECTURES=arm64 \
  -DCMAKE_BUILD_TYPE=Release \
  -GXcode

cmake --build build-ios --config Release -- -sdk iphoneos -arch arm64
```

### Android

```bash
export ANDROID_NDK=/path/to/android-ndk

cmake -B build-android \
  -DCMAKE_TOOLCHAIN_FILE=$ANDROID_NDK/build/cmake/android.toolchain.cmake \
  -DANDROID_ABI=arm64-v8a \
  -DANDROID_PLATFORM=android-24 \
  -DCMAKE_BUILD_TYPE=Release

cmake --build build-android --parallel
```

Supported ABIs: `arm64-v8a` (ARM NEON), `x86_64` (AVX2).

---

## Architecture

```
vanedb/
├── src/core/
│   ├── distance.h          # SIMD distance functions
│   ├── vector_store.h      # Thread-safe brute-force store
│   ├── hnsw_index.h        # HNSW approximate search
│   ├── mmap_vector_store.h # Memory-mapped store
│   └── gpu/
│       ├── metal_distance.h # Metal compute shaders
│       └── cuda_distance.cuh # CUDA kernels
├── tests/                   # C++ and Python tests
├── benchmarks/              # Google Benchmark suite
└── python/                  # pybind11 bindings
```

---

## CI/CD Pipeline

| Job | Platform | Description |
|-----|----------|-------------|
| build-and-test | Linux (GCC, Clang), macOS, Windows | Core build + tests |
| python-tests | All platforms, Python 3.9/3.11 | Python binding tests |
| sanitizers | Linux | AddressSanitizer, UBSan |
| coverage | Linux | Code coverage + Codecov |
| linux-arm64 | Linux ARM64 (Native) | ARM NEON validation |
| ios-build | macOS | iOS arm64 build |
| android-build | Linux | Android arm64-v8a + x86_64 |

---

## Known Limitations

- **VectorStore pointer lifetime**: `get()` returns a pointer invalidated by write operations
- **Brute-force search**: VectorStore uses O(n) search; use HNSWIndex for large datasets
- **No deletion in HNSW**: Removing vectors requires rebuilding the index
- **Single-file persistence**: No sharding for very large datasets
- **GPU dimensions**: Metal requires dimensions divisible by 4 (the experimental CUDA kernels assume the same)
- **CUDA is experimental**: `cuda_distance.cuh` contains kernel source but is not compiled into any build target, is untested, and requires NVIDIA hardware. Metal is the supported GPU path.

---

## Roadmap

- [ ] PyPI package distribution
- [ ] npm/WebAssembly bindings
- [ ] Product quantization (PQ) for memory efficiency
- [ ] Incremental index updates
- [ ] Multi-vector queries (batch search)
- [ ] Filtering/metadata support

---

## Acknowledgments

- [hnswlib](https://github.com/nmslib/hnswlib) - HNSW algorithm reference
- [Google Benchmark](https://github.com/google/benchmark) - Benchmarking framework
- [Catch2](https://github.com/catchorg/Catch2) - Testing framework
- [pybind11](https://github.com/pybind/pybind11) - Python bindings
