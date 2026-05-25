# VaneDB GPU Acceleration — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add GPU-accelerated distance computation via Metal (Apple Silicon) and CUDA (NVIDIA), feature-gated behind `gpu-metal` and `gpu-cuda`.

**Architecture:** `gpu/` module with a `MetalCompute` struct that compiles MSL shaders at runtime, manages GPU buffers, and dispatches compute kernels for L2/Cosine/Dot distance. `GpuBuffer` wraps persistent GPU memory. `CudaCompute` is a stub for future NVIDIA support. All GPU types require `dim % 4 == 0` (float4 vectorization). The public API: `upload()` vectors to GPU, then `search()` against them.

**Tech Stack:** `metal 0.33` (Apple Metal bindings), `objc 0.2` (Objective-C runtime for autoreleasepool), `cudarc 0.19` (CUDA, stub only)

**Spec:** `docs/superpowers/specs/2026-03-29-vanedb-rust-rewrite-design.md` — Phase 7

**C++ reference:** `/Users/anton/code/quiverdb/src/core/gpu/metal_distance.h`, `cuda_distance.cuh`

---

## File Structure

| File | Responsibility |
|------|---------------|
| `vanedb/Cargo.toml` | Add `metal`, `objc`, `cudarc` as optional deps + feature flags |
| `vanedb/src/gpu/mod.rs` | Re-exports, `GpuBuffer` type, `DistanceMetric` → GPU metric mapping |
| `vanedb/src/gpu/metal.rs` | `MetalCompute`: init device, compile MSL, upload, distances, search |
| `vanedb/src/gpu/cuda.rs` | `CudaCompute` stub (returns error without NVIDIA GPU) |
| `vanedb/src/lib.rs` | Add `gpu` module (cfg-gated) |
| `vanedb/tests/gpu_tests.rs` | Integration tests: GPU vs CPU comparison, multiple metrics |

---

### Task 1: Dependencies + GPU module skeleton

**Files:**
- Modify: `vanedb/Cargo.toml`, `vanedb/src/lib.rs`
- Create: `vanedb/src/gpu/mod.rs`

- [ ] **Step 1: Add GPU dependencies and feature flags**

Add to `[features]` in `vanedb/Cargo.toml`:
```toml
gpu-metal = ["dep:metal", "dep:objc"]
gpu-cuda = ["dep:cudarc"]
```

Add to `[dependencies]`:
```toml
metal = { version = "0.33", optional = true }
objc = { version = "0.2", optional = true }
cudarc = { version = "0.19", optional = true }
```

- [ ] **Step 2: Create gpu/mod.rs with GpuBuffer and shared types**

Create `vanedb/src/gpu/mod.rs`:
```rust
#[cfg(feature = "gpu-metal")]
pub mod metal;
#[cfg(feature = "gpu-cuda")]
pub mod cuda;

#[cfg(feature = "gpu-metal")]
pub use self::metal::MetalCompute;
#[cfg(feature = "gpu-cuda")]
pub use self::cuda::CudaCompute;

use crate::distance::DistanceMetric;

/// GPU distance metric (maps from DistanceMetric).
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum GpuMetric {
    L2,
    Cosine,
    Dot,
}

impl From<DistanceMetric> for GpuMetric {
    fn from(m: DistanceMetric) -> Self {
        match m {
            DistanceMetric::L2 => GpuMetric::L2,
            DistanceMetric::Cosine => GpuMetric::Cosine,
            DistanceMetric::Dot => GpuMetric::Dot,
        }
    }
}
```

- [ ] **Step 3: Update lib.rs**

Add to `vanedb/src/lib.rs` (after the `mmap` module):
```rust
#[cfg(any(feature = "gpu-metal", feature = "gpu-cuda"))]
pub mod gpu;
```

- [ ] **Step 4: Verify it compiles**

Run: `cargo check --features gpu-metal`
Expected: Compiles (with warnings about empty metal module — that's fine, added next task)

Note: If `cargo check --features gpu-metal` fails because the `metal` module file doesn't exist yet, create an empty placeholder:

Create `vanedb/src/gpu/metal.rs`:
```rust
// MetalCompute — implemented in Task 2
```

Create `vanedb/src/gpu/cuda.rs`:
```rust
// CudaCompute — implemented in Task 3
```

- [ ] **Step 5: Commit**

```bash
cargo fmt --all
git add vanedb/Cargo.toml vanedb/src/gpu/ vanedb/src/lib.rs
git commit -m "feat: add GPU module skeleton with feature flags"
```

---

### Task 2: MetalCompute implementation

**Files:**
- Modify: `vanedb/src/gpu/metal.rs`

- [ ] **Step 1: Implement MetalCompute**

Replace `vanedb/src/gpu/metal.rs` with:
```rust
use crate::error::{Result, VaneError};
use crate::gpu::GpuMetric;
use crate::store::SearchResult;

use metal::*;
use objc::rc::autoreleasepool;

/// MSL (Metal Shading Language) compute kernels for distance computation.
/// All kernels work on float4 vectors (dim must be divisible by 4).
const MSL_SOURCE: &str = r#"
#include <metal_stdlib>
using namespace metal;

kernel void l2(
    device const float4* q [[buffer(0)]],
    device const float4* v [[buffer(1)]],
    device float* r [[buffer(2)]],
    constant uint& d4 [[buffer(3)]],
    uint i [[thread_position_in_grid]]
) {
    float4 s = 0;
    uint o = i * d4;
    for (uint j = 0; j < d4; ++j) {
        float4 x = q[j] - v[o + j];
        s += x * x;
    }
    r[i] = s.x + s.y + s.z + s.w;
}

kernel void dp(
    device const float4* q [[buffer(0)]],
    device const float4* v [[buffer(1)]],
    device float* r [[buffer(2)]],
    constant uint& d4 [[buffer(3)]],
    uint i [[thread_position_in_grid]]
) {
    float4 s = 0;
    uint o = i * d4;
    for (uint j = 0; j < d4; ++j) {
        s += q[j] * v[o + j];
    }
    r[i] = -(s.x + s.y + s.z + s.w);
}

kernel void cs(
    device const float4* q [[buffer(0)]],
    device const float4* v [[buffer(1)]],
    device float* r [[buffer(2)]],
    constant uint& d4 [[buffer(3)]],
    uint i [[thread_position_in_grid]]
) {
    float4 d = 0, nq = 0, nv = 0;
    uint o = i * d4;
    for (uint j = 0; j < d4; ++j) {
        float4 a = q[j], b = v[o + j];
        d += a * b;
        nq += a * a;
        nv += b * b;
    }
    float dot = d.x + d.y + d.z + d.w;
    float na = nq.x + nq.y + nq.z + nq.w;
    float nb = nv.x + nv.y + nv.z + nv.w;
    float dn = na * nb;
    r[i] = 1.0f - clamp((dn < 1e-12f) ? 0.0f : dot * rsqrt(dn), -1.0f, 1.0f);
}
"#;

/// Handle to vectors uploaded to GPU memory.
pub struct GpuBuffer {
    buffer: Buffer,
    n: usize,
    dim: usize,
}

impl GpuBuffer {
    pub fn n(&self) -> usize {
        self.n
    }
    pub fn dim(&self) -> usize {
        self.dim
    }
}

/// Metal GPU compute for distance calculations.
pub struct MetalCompute {
    device: Device,
    queue: CommandQueue,
    l2_pipeline: ComputePipelineState,
    dot_pipeline: ComputePipelineState,
    cos_pipeline: ComputePipelineState,
}

impl MetalCompute {
    /// Initialize Metal compute. Returns error if no Metal device is available.
    pub fn new() -> Result<Self> {
        let device = Device::system_default()
            .ok_or_else(|| VaneError::Io("no Metal device available".to_string()))?;
        let queue = device.new_command_queue();

        let options = CompileOptions::new();
        let library = device
            .new_library_with_source(MSL_SOURCE, &options)
            .map_err(|e| VaneError::Io(format!("MSL compile error: {e}")))?;

        let l2_fn = library
            .get_function("l2", None)
            .map_err(|e| VaneError::Io(format!("get l2 function: {e}")))?;
        let dp_fn = library
            .get_function("dp", None)
            .map_err(|e| VaneError::Io(format!("get dp function: {e}")))?;
        let cs_fn = library
            .get_function("cs", None)
            .map_err(|e| VaneError::Io(format!("get cs function: {e}")))?;

        let l2_pipeline = device
            .new_compute_pipeline_state_with_function(&l2_fn)
            .map_err(|e| VaneError::Io(format!("l2 pipeline: {e}")))?;
        let dot_pipeline = device
            .new_compute_pipeline_state_with_function(&dp_fn)
            .map_err(|e| VaneError::Io(format!("dot pipeline: {e}")))?;
        let cos_pipeline = device
            .new_compute_pipeline_state_with_function(&cs_fn)
            .map_err(|e| VaneError::Io(format!("cos pipeline: {e}")))?;

        Ok(Self {
            device,
            queue,
            l2_pipeline,
            dot_pipeline,
            cos_pipeline,
        })
    }

    /// Upload vectors to GPU memory. Vectors is a flat array: [v0_0..v0_dim, v1_0..v1_dim, ...].
    /// Dimension must be divisible by 4.
    pub fn upload(&self, vectors: &[f32], n: usize, dim: usize) -> Result<GpuBuffer> {
        if dim % 4 != 0 {
            return Err(VaneError::InvalidParameter("GPU requires dim % 4 == 0"));
        }
        if vectors.len() != n * dim {
            return Err(VaneError::DimensionMismatch {
                expected: n * dim,
                got: vectors.len(),
            });
        }
        let buffer = self.device.new_buffer_with_data(
            vectors.as_ptr() as *const _,
            (vectors.len() * std::mem::size_of::<f32>()) as u64,
            MTLResourceOptions::StorageModeShared,
        );
        Ok(GpuBuffer { buffer, n, dim })
    }

    /// Compute distances from query to all vectors in the buffer.
    /// Returns Vec<f32> of length buffer.n().
    pub fn distances(
        &self,
        query: &[f32],
        buffer: &GpuBuffer,
        metric: GpuMetric,
    ) -> Result<Vec<f32>> {
        if query.len() != buffer.dim {
            return Err(VaneError::DimensionMismatch {
                expected: buffer.dim,
                got: query.len(),
            });
        }

        let pipeline = match metric {
            GpuMetric::L2 => &self.l2_pipeline,
            GpuMetric::Dot => &self.dot_pipeline,
            GpuMetric::Cosine => &self.cos_pipeline,
        };

        let n = buffer.n;
        let dim = buffer.dim;

        let result = autoreleasepool(|| {
            let query_buf = self.device.new_buffer_with_data(
                query.as_ptr() as *const _,
                (dim * std::mem::size_of::<f32>()) as u64,
                MTLResourceOptions::StorageModeShared,
            );
            let result_buf = self.device.new_buffer(
                (n * std::mem::size_of::<f32>()) as u64,
                MTLResourceOptions::StorageModeShared,
            );
            let d4: u32 = (dim / 4) as u32;
            let dim_buf = self.device.new_buffer_with_data(
                &d4 as *const u32 as *const _,
                std::mem::size_of::<u32>() as u64,
                MTLResourceOptions::StorageModeShared,
            );

            let command_buffer = self.queue.new_command_buffer();
            let encoder = command_buffer.new_compute_command_encoder();
            encoder.set_compute_pipeline_state(pipeline);
            encoder.set_buffer(0, Some(&query_buf), 0);
            encoder.set_buffer(1, Some(&buffer.buffer), 0);
            encoder.set_buffer(2, Some(&result_buf), 0);
            encoder.set_buffer(3, Some(&dim_buf), 0);

            let grid = MTLSize::new(n as u64, 1, 1);
            let max_threads = pipeline.max_total_threads_per_threadgroup();
            let group = MTLSize::new(max_threads.min(n as u64), 1, 1);
            encoder.dispatch_threads(grid, group);
            encoder.end_encoding();
            command_buffer.commit();
            command_buffer.wait_until_completed();

            let ptr = result_buf.contents() as *const f32;
            unsafe { std::slice::from_raw_parts(ptr, n) }.to_vec()
        });

        Ok(result)
    }

    /// Search for k nearest neighbors using GPU distance computation.
    /// Returns results sorted by distance (ascending).
    pub fn search(
        &self,
        query: &[f32],
        ids: &[u64],
        buffer: &GpuBuffer,
        k: usize,
        metric: GpuMetric,
    ) -> Result<Vec<SearchResult>> {
        if k == 0 {
            return Err(VaneError::InvalidK);
        }
        let dists = self.distances(query, buffer, metric)?;
        let mut results: Vec<SearchResult> = ids
            .iter()
            .zip(dists.iter())
            .map(|(&id, &d)| SearchResult::new(id, d))
            .collect();
        results.sort();
        results.truncate(k);
        Ok(results)
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::distance::{self, DistanceMetric};

    #[test]
    fn metal_init() {
        let gpu = MetalCompute::new();
        assert!(gpu.is_ok(), "Metal should be available on macOS");
    }

    #[test]
    fn metal_l2_matches_cpu() {
        let gpu = MetalCompute::new().unwrap();
        let dim = 128;
        let n = 100;
        let vectors: Vec<f32> = (0..n * dim)
            .map(|i| (i as f32 * 0.01).sin())
            .collect();
        let query: Vec<f32> = (0..dim).map(|i| (i as f32 * 0.02).cos()).collect();

        let buf = gpu.upload(&vectors, n, dim).unwrap();
        let gpu_dists = gpu.distances(&query, &buf, GpuMetric::L2).unwrap();

        let cpu_dist = distance::distance_fn(DistanceMetric::L2);
        for i in 0..n {
            let cpu_d = cpu_dist(&query, &vectors[i * dim..(i + 1) * dim]);
            assert!(
                (gpu_dists[i] - cpu_d).abs() < 1e-3,
                "vector {i}: gpu={} cpu={cpu_d}",
                gpu_dists[i]
            );
        }
    }

    #[test]
    fn metal_cosine_matches_cpu() {
        let gpu = MetalCompute::new().unwrap();
        let dim = 128;
        let n = 100;
        let vectors: Vec<f32> = (0..n * dim)
            .map(|i| (i as f32 * 0.01).sin() + 0.1)
            .collect();
        let query: Vec<f32> = (0..dim).map(|i| (i as f32 * 0.02).cos() + 0.1).collect();

        let buf = gpu.upload(&vectors, n, dim).unwrap();
        let gpu_dists = gpu.distances(&query, &buf, GpuMetric::Cosine).unwrap();

        let cpu_dist = distance::distance_fn(DistanceMetric::Cosine);
        for i in 0..n {
            let cpu_d = cpu_dist(&query, &vectors[i * dim..(i + 1) * dim]);
            assert!(
                (gpu_dists[i] - cpu_d).abs() < 1e-3,
                "vector {i}: gpu={} cpu={cpu_d}",
                gpu_dists[i]
            );
        }
    }

    #[test]
    fn metal_dot_matches_cpu() {
        let gpu = MetalCompute::new().unwrap();
        let dim = 128;
        let n = 100;
        let vectors: Vec<f32> = (0..n * dim)
            .map(|i| (i as f32 * 0.01).sin())
            .collect();
        let query: Vec<f32> = (0..dim).map(|i| (i as f32 * 0.02).cos()).collect();

        let buf = gpu.upload(&vectors, n, dim).unwrap();
        let gpu_dists = gpu.distances(&query, &buf, GpuMetric::Dot).unwrap();

        let cpu_dist = distance::distance_fn(DistanceMetric::Dot);
        for i in 0..n {
            let cpu_d = cpu_dist(&query, &vectors[i * dim..(i + 1) * dim]);
            assert!(
                (gpu_dists[i] - cpu_d).abs() < 1e-3,
                "vector {i}: gpu={} cpu={cpu_d}",
                gpu_dists[i]
            );
        }
    }

    #[test]
    fn metal_search_returns_sorted() {
        let gpu = MetalCompute::new().unwrap();
        let dim = 128;
        let n = 50;
        let vectors: Vec<f32> = (0..n * dim)
            .map(|i| (i as f32 * 0.01).sin())
            .collect();
        let ids: Vec<u64> = (0..n as u64).collect();
        let query: Vec<f32> = (0..dim).map(|i| (i as f32 * 0.02).cos()).collect();

        let buf = gpu.upload(&vectors, n, dim).unwrap();
        let results = gpu
            .search(&query, &ids, &buf, 5, GpuMetric::L2)
            .unwrap();
        assert_eq!(results.len(), 5);
        // Verify sorted by distance
        for w in results.windows(2) {
            assert!(w[0].distance <= w[1].distance);
        }
    }

    #[test]
    fn metal_rejects_dim_not_divisible_by_4() {
        let gpu = MetalCompute::new().unwrap();
        let vectors = vec![0.0f32; 300]; // 100 vectors of dim 3
        assert!(gpu.upload(&vectors, 100, 3).is_err());
    }
}
```

- [ ] **Step 2: Run tests**

Run: `cargo test --features gpu-metal`
Expected: All tests pass (6 new Metal tests + all existing)

- [ ] **Step 3: Commit**

```bash
cargo fmt --all
git add vanedb/src/gpu/metal.rs
git commit -m "feat: add Metal GPU distance computation with MSL kernels"
```

---

### Task 3: CUDA stub + integration tests + push

**Files:**
- Modify: `vanedb/src/gpu/cuda.rs`
- Create: `vanedb/tests/gpu_tests.rs`

- [ ] **Step 1: Create CUDA stub**

Replace `vanedb/src/gpu/cuda.rs` with:
```rust
use crate::error::{Result, VaneError};
use crate::gpu::GpuMetric;
use crate::store::SearchResult;

/// CUDA GPU compute for distance calculations.
/// Requires NVIDIA GPU with CUDA toolkit installed.
pub struct CudaCompute {
    _private: (),
}

/// Handle to vectors in CUDA device memory.
pub struct CudaBuffer {
    _private: (),
}

impl CudaCompute {
    /// Initialize CUDA compute. Returns error if no CUDA device is available.
    pub fn new() -> Result<Self> {
        Err(VaneError::Io(
            "CUDA support requires NVIDIA GPU and CUDA toolkit".to_string(),
        ))
    }

    /// Upload vectors to GPU memory.
    pub fn upload(&self, _vectors: &[f32], _n: usize, _dim: usize) -> Result<CudaBuffer> {
        Err(VaneError::Io("CUDA not available".to_string()))
    }

    /// Compute distances from query to all vectors.
    pub fn distances(
        &self,
        _query: &[f32],
        _buffer: &CudaBuffer,
        _metric: GpuMetric,
    ) -> Result<Vec<f32>> {
        Err(VaneError::Io("CUDA not available".to_string()))
    }

    /// Search for k nearest neighbors using GPU.
    pub fn search(
        &self,
        _query: &[f32],
        _ids: &[u64],
        _buffer: &CudaBuffer,
        _k: usize,
        _metric: GpuMetric,
    ) -> Result<Vec<SearchResult>> {
        Err(VaneError::Io("CUDA not available".to_string()))
    }
}
```

- [ ] **Step 2: Create integration tests**

Create `vanedb/tests/gpu_tests.rs`:
```rust
#![cfg(feature = "gpu-metal")]

use vanedb::distance::{self, DistanceMetric};
use vanedb::gpu::{GpuMetric, MetalCompute};
use vanedb::VectorStore;

#[test]
fn gpu_search_matches_brute_force() {
    let gpu = MetalCompute::new().unwrap();
    let dim = 128;
    let n = 500;
    let k = 10;

    // Generate vectors
    let flat: Vec<f32> = (0..n * dim)
        .map(|i| ((i * 31 + 7) % 1000) as f32 / 100.0)
        .collect();
    let ids: Vec<u64> = (0..n as u64).collect();

    // CPU brute force
    let store = VectorStore::new(dim, DistanceMetric::L2).unwrap();
    for i in 0..n {
        store.add(i as u64, &flat[i * dim..(i + 1) * dim]).unwrap();
    }

    // GPU
    let buf = gpu.upload(&flat, n, dim).unwrap();

    let query: Vec<f32> = (0..dim).map(|d| (d * 13 % 1000) as f32 / 100.0).collect();

    let cpu_results = store.search(&query, k).unwrap();
    let gpu_results = gpu.search(&query, &ids, &buf, k, GpuMetric::L2).unwrap();

    assert_eq!(cpu_results.len(), gpu_results.len());
    for (cpu, gpu_r) in cpu_results.iter().zip(gpu_results.iter()) {
        assert_eq!(cpu.id, gpu_r.id, "CPU and GPU disagree on nearest neighbor");
    }
}

#[test]
fn gpu_all_metrics() {
    let gpu = MetalCompute::new().unwrap();
    let dim = 128;
    let n = 100;
    let flat: Vec<f32> = (0..n * dim)
        .map(|i| (i as f32 * 0.01).sin() + 0.1)
        .collect();
    let query: Vec<f32> = (0..dim).map(|i| (i as f32 * 0.02).cos() + 0.1).collect();
    let buf = gpu.upload(&flat, n, dim).unwrap();

    for (gpu_metric, cpu_metric) in [
        (GpuMetric::L2, DistanceMetric::L2),
        (GpuMetric::Cosine, DistanceMetric::Cosine),
        (GpuMetric::Dot, DistanceMetric::Dot),
    ] {
        let gpu_dists = gpu.distances(&query, &buf, gpu_metric).unwrap();
        let cpu_fn = distance::distance_fn(cpu_metric);
        for i in 0..n {
            let cpu_d = cpu_fn(&query, &flat[i * dim..(i + 1) * dim]);
            assert!(
                (gpu_dists[i] - cpu_d).abs() < 1e-2,
                "{gpu_metric:?} vector {i}: gpu={} cpu={cpu_d}",
                gpu_dists[i]
            );
        }
    }
}
```

- [ ] **Step 3: Update CI to test GPU feature on macOS only**

Modify `.github/workflows/ci.yml` — change the macOS job to test with gpu-metal:
```yaml
  test-macos:
    name: Test (macOS ARM)
    runs-on: macos-latest
    steps:
      - uses: actions/checkout@v4
      - uses: dtolnay/rust-toolchain@stable
      - run: cargo test --features gpu-metal
```

Note: Only change the macOS job. Linux and Windows jobs stay as `--all-features` (which won't include gpu-metal since it's not in default).

- [ ] **Step 4: Run all tests + clippy + fmt**

Run: `cargo test --features gpu-metal && cargo clippy --all-targets --features gpu-metal -- -D warnings && cargo fmt --all -- --check`
Expected: All pass

- [ ] **Step 5: Commit and push**

```bash
cargo fmt --all
git add vanedb/src/gpu/cuda.rs vanedb/tests/gpu_tests.rs .github/workflows/ci.yml
git commit -m "feat: add CUDA stub, GPU integration tests, Metal CI"
git push
```

---

## Notes

- **dim % 4 == 0 requirement:** The MSL kernels use `float4` (128-bit SIMD) for vectorization. Common embedding dimensions (128, 256, 384, 512, 768, 1536) all satisfy this.
- **Thread safety:** `MetalCompute` is NOT `Send + Sync` by default because Metal objects use Objective-C reference counting. If concurrent GPU access is needed, create one `MetalCompute` per thread or wrap in a Mutex.
- **CUDA stub:** Returns `Err` on all operations. Real CUDA implementation would use `cudarc` to load pre-compiled PTX kernels. Requires NVIDIA GPU and CUDA toolkit to test.
- **Persistent buffers:** The `upload()` → `search()` pattern keeps vectors in GPU memory across multiple searches, avoiding repeated host→device transfers.
