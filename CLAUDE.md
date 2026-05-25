# vanedb-cpp

## Overview
Embeddable vector database for edge AI. Header-only C++20, SIMD-optimized, cross-platform.

## Role in the VaneDB Project

This is the **C++ header-only** implementation. The Rust implementation lives at
[vanedb/vanedb](https://github.com/vanedb/vanedb) and is the primary entry point
for Rust (`cargo add vanedb`), Python (`pip install vanedb`), and WASM consumers.

**Why two implementations:**
- **C++** (this repo): drop a header into any CMake/Bazel project. No Rust toolchain
  needed. The embed path for iOS/Android native code and existing C++ codebases.
- **Rust**: cleaner concurrency, ergonomic Python/WASM bindings, the path for new
  language ecosystems.

**Alignment policy:** Features generally land in Rust first. C++ syncs core algorithms,
distance functions, and on-disk persistence formats (HNSW files written by one
implementation should load in the other when feasible). Intentional divergence at
the edges — WASM is Rust-only; the header-only embed path is C++-only.

## Current Status: v0.1.0 (frozen for sync work)

### Features
| Feature | Status |
|---------|--------|
| Distance Functions | L2, Cosine, Dot (ARM NEON, x86 AVX2) |
| VectorStore | In-memory k-NN, thread-safe |
| HNSWIndex | Approximate NN with save/load |
| MMapVectorStore | Memory-mapped zero-copy |
| GPU Acceleration | Metal (Apple Silicon), CUDA (NVIDIA) |
| Python Bindings | pybind11 + NumPy |
| Mobile | iOS arm64, Android arm64-v8a/x86_64 |

### Test Coverage
- 38 C++ test cases (131k+ assertions)
- 28 Python tests
- 3 GPU tests (Metal)
- Sanitizers: ASan + UBSan clean

### Performance
- CPU SIMD: 3.8x speedup vs scalar (768d)
- L2 distance: ~100ns (768d, Apple Silicon)
- GPU: 3.9x speedup at 500k vectors (persistent buffers)

## Structure
```
src/core/
├── distance.h           # SIMD distance functions
├── vector_store.h       # Brute-force k-NN
├── hnsw_index.h         # HNSW approximate NN
├── mmap_vector_store.h  # Memory-mapped store
└── gpu/
    ├── metal_distance.h # Metal compute
    └── cuda_distance.cuh # CUDA kernels
```
~1,000 lines of core code total.

## Build
```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
cd build && ctest --output-on-failure
```

## CI/CD (10 jobs)
- Linux: GCC, Clang
- macOS: Apple Clang (ARM NEON)
- Windows: MSVC (AVX2)
- ARM64: Native runners
- iOS: Xcode arm64
- Android: NDK arm64-v8a, x86_64
- Python: 3 platforms x 2 versions
- Sanitizers: ASan, UBSan
- Coverage: Codecov

## API
```cpp
// Distance
float d = vanedb::l2_sq(a, b, dim);

// VectorStore
vanedb::VectorStore store(768, vanedb::DistanceMetric::COSINE);
store.add(id, vec);
auto results = store.search(query, k);

// HNSWIndex
vanedb::HNSWIndex idx(768, vanedb::DistanceMetric::COSINE, 100000);
idx.add(id, vec);
idx.save("index.bin");

// GPU (Metal)
auto& gpu = vanedb::gpu::MetalCompute::get();
auto buf = gpu.upload(vectors, n, dim);
auto dists = gpu.search(query, buf, dim, n, vanedb::gpu::MetalMetric::L2);
```

## Demo: Obsidian Semantic Search

A working semantic search tool built on VaneDB demonstrating real-world usage:
```bash
python3 search.py index ~/path/to/vault   # Index notes
python3 search.py find "your query"       # Search
python3 search.py interactive             # REPL mode
```

## Maintenance Posture

This repo accepts:
- Bug fixes and CVE patches
- Performance work on hot paths (HNSW search, SIMD distance)
- New platform/compiler support (CI matrix expansion)
- On-disk format additions that mirror the Rust repo (so files are
  interchangeable when feasible)

This repo defers to the Rust repo for:
- New high-level features (batch search API, metadata filtering, PQ, sharding)
- Language bindings beyond Python (Node/WASM, Swift, Go)
- Package distribution work (PyPI naming, npm)

Check [vanedb/vanedb](https://github.com/vanedb/vanedb) issues before filing
feature requests here. PyPI distribution (`pip install vanedb`), WASM, and
batch/metadata APIs are tracked there.

## Known Limitations
- No deletion in HNSWIndex (rebuild required)
- GPU requires dim % 4 == 0
- VectorStore `get()` pointer invalidated by writes
- Single-file persistence (no sharding)
