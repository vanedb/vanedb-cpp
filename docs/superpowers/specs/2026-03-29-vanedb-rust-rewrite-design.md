# VaneDB: Rust Rewrite Design Spec

**Date:** 2026-03-29
**Status:** Approved

## 1. Project Identity

- **Name:** VaneDB
- **Crate:** `vanedb` (available on crates.io)
- **GitHub org:** `vanedb` (available)
- **Tagline:** Embeddable vector database for edge AI
- **License:** MIT (same as current)

### Repository Structure

- `vanedb/vanedb` — Rust rewrite (primary, active development)
- `vanedb/vanedb-cpp` — C++ version (transferred from `tsvet01/quiverdb`, maintenance mode)

## 2. Motivation

In priority order:

1. **Learning** — Build Rust expertise on a well-scoped project (~1,360 lines C++ to port)
2. **Ecosystem** — `cargo publish`, PyO3/maturin for Python, `wasm-pack` for WASM — all simpler than the current CMake+pybind11 stack
3. **Safety** — Memory safety for mmap, GPU buffer management, and concurrent access without relying on ASan/UBSan

## 3. Scope

Full port of all current C++ functionality:

| C++ Component | Rust Equivalent | Key Dependency |
|---|---|---|
| `distance.h` (SIMD dispatch) | `distance/` module, trait-based dispatch | `std::arch` (stable) |
| `distance_strategy.h` | `DistanceComputer` trait | Rust traits |
| `vector_store.h` | `VectorStore` with `RwLock` | `parking_lot` |
| `hnsw_index.h` | `HnswIndex` with builder pattern | `bincode` or `rkyv` |
| `mmap_vector_store.h` | `MmapVectorStore` | `memmap2` |
| `metal_distance.h` | Metal GPU compute | `metal-rs` |
| `cuda_distance.cuh` | CUDA kernels | `cudarc` |
| pybind11 bindings | Python bindings | `pyo3` + `maturin` |

Both C++ and Rust versions will be maintained. C++ receives bugfixes only; Rust is the primary for new features.

## 4. Project Layout

```
vanedb/
├── Cargo.toml              # workspace root
├── vanedb/                 # core library crate
│   ├── Cargo.toml
│   └── src/
│       ├── lib.rs
│       ├── distance/       # SIMD distance functions + DistanceComputer trait
│       ├── store/          # VectorStore, MmapVectorStore
│       ├── hnsw/           # HNSW index
│       └── gpu/            # Metal + CUDA (feature-gated)
├── vanedb-py/              # PyO3 bindings crate
│   ├── Cargo.toml
│   └── src/lib.rs
└── benches/                # criterion benchmarks
```

### Feature Flags (`vanedb/Cargo.toml`)

- `default = ["simd"]`
- `simd` — NEON/AVX2 intrinsics (auto-detected via `#[cfg(target_arch)]`)
- `mmap` — memory-mapped store (enables `memmap2`)
- `gpu-metal` — Apple Metal compute
- `gpu-cuda` — NVIDIA CUDA

## 5. Public API

```rust
use vanedb::{VectorStore, HnswIndex, DistanceMetric};

// Brute-force store
let mut store = VectorStore::new(768, DistanceMetric::Cosine);
store.add(42, &embedding)?;
let results = store.search(&query, 10)?;  // Vec<SearchResult>

// HNSW index
let mut index = HnswIndex::builder(768, DistanceMetric::Cosine)
    .capacity(100_000)
    .ef_construction(200)
    .m(16)
    .build()?;
index.add(42, &embedding)?;
let results = index.search(&query, 10)?;

// Persistence
index.save("index.bin")?;
let index = HnswIndex::load("index.bin")?;

// Mmap store
let store = MmapVectorStore::open("vectors.bin", 768, DistanceMetric::L2)?;
let results = store.search(&query, 10)?;

// GPU (feature-gated)
#[cfg(feature = "gpu-metal")]
{
    use vanedb::gpu::MetalCompute;
    let gpu = MetalCompute::new()?;
    let buffer = gpu.upload(&vectors, dim)?;
    let results = gpu.search(&query, &buffer, 10, DistanceMetric::L2)?;
}
```

### API Design Principles

- **`Result<T>` everywhere** — no silent failures
- **Builder pattern** for types with many config options (HNSW)
- **`&[f32]` slices** — no raw pointers with separate dimension args
- **`SearchResult { id: u64, distance: f32 }`** — typed return values

## 6. Testing Strategy

### Correctness

- Unit tests inline (`#[cfg(test)] mod tests` per module)
- Integration tests in `tests/` (full build → search → verify results)
- Property-based tests via `proptest` for distance functions (random vectors, triangle inequality)

### Performance Regression

- `criterion` benchmarks for key operations:
  - Distance functions at 128d, 768d, 1536d
  - VectorStore search at 1k, 10k, 100k vectors
  - HNSW build + search at 10k, 100k vectors
  - Serialization round-trip (save/load)
- CI compares PR benchmarks against `main` baseline using `critcmp`
- Threshold: fail PR if any benchmark regresses >5%

## 7. CI/CD

| Job | What |
|-----|------|
| Linux (x86_64) | `cargo test` + `clippy` + `cargo fmt --check` |
| macOS (ARM) | `cargo test` (NEON intrinsics) |
| Windows (x86_64) | `cargo test` (AVX2) |
| Perf regression | `criterion` bench: main vs PR, fail at >5% |
| Metal GPU | `--features gpu-metal` tests |
| Python wheels | `maturin build` (Linux/macOS/Windows) |
| WASM | `wasm-pack build` smoke test |
| Coverage | `cargo-llvm-cov` → Codecov |
| Release | `cargo publish` + `maturin publish` on git tag |

## 8. Distribution

| Channel | Tool | Package name |
|---------|------|-------------|
| Rust | `cargo publish` | `vanedb` on crates.io |
| Python | `maturin publish` | `vanedb` on PyPI |
| WASM | `wasm-pack publish` | `vanedb` on npm |
| Mobile | `cargo-ndk` (Android), Xcode integration (iOS) | N/A |

## 9. Migration Phases

Each phase produces a usable, testable milestone.

| Phase | Deliverable | Key Rust concepts learned |
|-------|-------------|--------------------------|
| 1 | Project setup — Cargo workspace, CI skeleton, GitHub org | Cargo, workspaces, GitHub Actions for Rust |
| 2 | Distance functions + SIMD | `unsafe`, `std::arch`, traits, `#[cfg]` dispatch |
| 3 | VectorStore (brute-force k-NN) | Ownership, `RwLock`, lifetimes, `Result<T>` |
| 4 | HNSW index + persistence | Complex data structures, `bincode`/`rkyv` serialization |
| 5 | MmapVectorStore | `memmap2`, unsafe memory mapping, lifetime-bound borrows |
| 6 | Performance benchmarks + CI regression | `criterion`, `critcmp`, CI pipeline |
| 7 | GPU — Metal + CUDA | FFI, feature gates, `metal-rs`, `cudarc` |
| 8 | Python bindings | PyO3, `maturin`, PyPI publishing |
| 9 | WASM bindings | `wasm-bindgen`, `wasm-pack`, npm publishing |

## 10. C++ Version Transition

- Transfer `tsvet01/quiverdb` → `vanedb/vanedb-cpp`
- GitHub auto-redirects preserve all existing links
- Update README: "Maintenance mode — active development at vanedb/vanedb"
- CI stays active, bugfixes accepted, no new features
