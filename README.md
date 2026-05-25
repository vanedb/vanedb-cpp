<div align="center">

# VaneDB

**Embeddable vector database for edge AI**

[![Build](https://github.com/vanedb/vanedb-cpp/actions/workflows/build-and-test.yml/badge.svg)](https://github.com/vanedb/vanedb-cpp/actions/workflows/build-and-test.yml)
[![codecov](https://codecov.io/gh/vanedb/vanedb-cpp/branch/main/graph/badge.svg)](https://codecov.io/gh/vanedb/vanedb-cpp)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![C++20](https://img.shields.io/badge/C%2B%2B-20-00599C.svg)](https://en.cppreference.com/w/cpp/20)
[![Python 3.9+](https://img.shields.io/badge/Python-3.9%2B-3776AB.svg)](https://www.python.org/)

</div>

---

Header-only C++20 vector database with SIMD acceleration. Runs on Linux, macOS, Windows, iOS, and Android.

> **Two implementations.** VaneDB is maintained as both C++ and Rust under the [@vanedb](https://github.com/vanedb) org. This repo is the **C++ header-only** version — drop a header into any CMake project, no Rust toolchain needed. For the Rust crate (with Python/PyO3 and WASM bindings), see **[vanedb/vanedb](https://github.com/vanedb/vanedb)**.

## Why VaneDB?

| Feature | VaneDB | FAISS | hnswlib | Pinecone |
|---------|----------|-------|---------|----------|
| Header-only | Yes | No | No | N/A |
| Mobile/Edge | Native | No | Partial | No |
| Dependencies | Zero | Many | Few | Cloud |
| Binary size | <100KB | 200MB+ | ~1MB | N/A |
| GPU (Metal) | Yes | No | No | N/A |

**Perfect for**: Mobile AI apps, Obsidian/Logseq plugins, edge devices, offline-first applications.

## Features

- **SIMD-optimized**: ARM NEON, x86 AVX2 (~100ns for 768d vectors)
- **Multiple indexes**: Brute-force, HNSW, Memory-mapped
- **GPU acceleration**: Metal (Apple Silicon), CUDA (NVIDIA)
- **Thread-safe**: Concurrent reads with `std::shared_mutex`
- **Python bindings**: NumPy integration, GIL-safe

## Quick Start

```cpp
#include "core/vector_store.h"

vanedb::VectorStore store(768, vanedb::DistanceMetric::COSINE);
store.add(1, embedding);
auto results = store.search(query, 5);  // top-5 nearest neighbors
```

```cpp
#include "core/hnsw_index.h"

vanedb::HNSWIndex index(768, vanedb::DistanceMetric::COSINE, 100000);
index.add(1, embedding);
auto results = index.search(query, 5);
index.save("index.bin");
```

```python
import vanedb_py as vanedb
import numpy as np

index = vanedb.HNSWIndex(768, vanedb.DistanceMetric.COSINE)
index.add(1, np.random.rand(768).astype(np.float32))
ids, distances = index.search(query, 10)
```

## Build

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

## Documentation

- [Full API Guide](docs/GUIDE.md) - Detailed usage, Python bindings, mobile builds
- [CHANGELOG](CHANGELOG.md) - Version history

## License

MIT License - see [LICENSE](LICENSE) for details.
