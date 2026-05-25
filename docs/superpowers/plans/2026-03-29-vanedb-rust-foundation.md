# VaneDB Rust Foundation — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Create the VaneDB Rust project with SIMD-accelerated distance functions and thread-safe brute-force VectorStore — a working, publishable foundation crate.

**Architecture:** Cargo workspace with a `vanedb` library crate. Distance functions auto-dispatch to SIMD (NEON on aarch64, AVX2 on x86_64) with scalar fallback. VectorStore provides thread-safe brute-force k-NN search using flat vector storage with `parking_lot::RwLock`. All public APIs return `Result<T>`.

**Tech Stack:** Rust 2021 edition, `parking_lot` (RwLock), `std::arch` (SIMD intrinsics), `proptest` (property testing), GitHub Actions (CI)

**Spec:** `docs/superpowers/specs/2026-03-29-vanedb-rust-rewrite-design.md`

**Scope:** Phases 1-3 of the spec (Setup + Distance + VectorStore). Phases 4-9 will be separate plans.

---

## File Structure

| File | Responsibility |
|------|---------------|
| `Cargo.toml` | Workspace root |
| `.github/workflows/ci.yml` | CI: test, clippy, fmt on 3 platforms |
| `vanedb/Cargo.toml` | Core library crate manifest |
| `vanedb/src/lib.rs` | Public API re-exports |
| `vanedb/src/error.rs` | `VaneError` enum + `Result<T>` alias |
| `vanedb/src/distance/mod.rs` | `DistanceMetric` enum, dispatch function, `COSINE_EPSILON` |
| `vanedb/src/distance/scalar.rs` | Scalar distance implementations |
| `vanedb/src/distance/neon.rs` | ARM NEON SIMD (aarch64 only) |
| `vanedb/src/distance/avx2.rs` | x86_64 AVX2+FMA SIMD |
| `vanedb/src/store/mod.rs` | Re-exports for store module |
| `vanedb/src/store/search_result.rs` | `SearchResult` struct |
| `vanedb/src/store/vector_store.rs` | `VectorStore` implementation |
| `vanedb/tests/property_tests.rs` | Property-based tests for distance functions |

---

### Task 1: Create GitHub repo and Cargo workspace

**Files:**
- Create: `Cargo.toml`, `vanedb/Cargo.toml`, `vanedb/src/lib.rs`, `README.md`

- [ ] **Step 1: Create the GitHub repo and clone it**

```bash
cd /Users/anton/code
gh repo create vanedb/vanedb --public --description "Embeddable vector database for edge AI" --license MIT --clone
cd vanedb
```

- [ ] **Step 2: Create workspace Cargo.toml**

Create `Cargo.toml`:
```toml
[workspace]
members = ["vanedb"]
resolver = "2"
```

- [ ] **Step 3: Create library crate**

```bash
cargo init vanedb --lib
```

Replace `vanedb/Cargo.toml`:
```toml
[package]
name = "vanedb"
version = "0.1.0"
edition = "2021"
description = "Embeddable vector database for edge AI"
license = "MIT"
repository = "https://github.com/vanedb/vanedb"
keywords = ["vector", "database", "embeddings", "simd", "similarity-search"]
categories = ["database", "science"]

[features]
default = []
mmap = ["dep:memmap2"]

[dependencies]
parking_lot = "0.12"
memmap2 = { version = "0.9", optional = true }

[dev-dependencies]
proptest = "1.6"
```

Replace `vanedb/src/lib.rs`:
```rust
pub fn hello() -> &'static str {
    "vanedb"
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn it_works() {
        assert_eq!(hello(), "vanedb");
    }
}
```

- [ ] **Step 4: Verify it compiles and tests pass**

Run: `cargo test`
Expected: 1 test passes

- [ ] **Step 5: Create README.md**

Create `README.md`:
```markdown
# VaneDB

Embeddable vector database for edge AI.

## Status

Under construction — Rust rewrite of [QuiverDB](https://github.com/tsvet01/quiverdb).

## License

MIT
```

- [ ] **Step 6: Commit and push**

```bash
git add Cargo.toml vanedb/ README.md
git commit -m "feat: initialize Cargo workspace with vanedb library crate"
git push -u origin main
```

---

### Task 2: Error type

**Files:**
- Create: `vanedb/src/error.rs`
- Modify: `vanedb/src/lib.rs`

- [ ] **Step 1: Create error module with tests**

Create `vanedb/src/error.rs`:
```rust
#[derive(Debug, Clone, PartialEq, Eq)]
pub enum VaneError {
    DimensionMismatch { expected: usize, got: usize },
    EmptyVector,
    NotFound { id: u64 },
    DuplicateId { id: u64 },
    InvalidK,
}

impl std::fmt::Display for VaneError {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            Self::DimensionMismatch { expected, got } => {
                write!(f, "dimension mismatch: expected {expected}, got {got}")
            }
            Self::EmptyVector => write!(f, "empty vector"),
            Self::NotFound { id } => write!(f, "vector not found: {id}"),
            Self::DuplicateId { id } => write!(f, "duplicate id: {id}"),
            Self::InvalidK => write!(f, "k must be > 0"),
        }
    }
}

impl std::error::Error for VaneError {}

pub type Result<T> = std::result::Result<T, VaneError>;

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn error_display_dimension_mismatch() {
        let err = VaneError::DimensionMismatch { expected: 768, got: 512 };
        assert_eq!(err.to_string(), "dimension mismatch: expected 768, got 512");
    }

    #[test]
    fn error_display_not_found() {
        let err = VaneError::NotFound { id: 42 };
        assert_eq!(err.to_string(), "vector not found: 42");
    }

    #[test]
    fn error_is_send_sync() {
        fn assert_send_sync<T: Send + Sync>() {}
        assert_send_sync::<VaneError>();
    }
}
```

- [ ] **Step 2: Update lib.rs**

Replace `vanedb/src/lib.rs`:
```rust
pub mod error;

pub use error::{Result, VaneError};
```

- [ ] **Step 3: Run tests**

Run: `cargo test`
Expected: 3 tests pass

- [ ] **Step 4: Commit**

```bash
git add vanedb/src/error.rs vanedb/src/lib.rs
git commit -m "feat: add VaneError type with Result alias"
```

---

### Task 3: DistanceMetric + scalar L2

**Files:**
- Create: `vanedb/src/distance/mod.rs`, `vanedb/src/distance/scalar.rs`
- Modify: `vanedb/src/lib.rs`

- [ ] **Step 1: Create distance module with DistanceMetric and dispatch**

Create `vanedb/src/distance/mod.rs`:
```rust
pub mod scalar;

/// Distance metric for vector comparison.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum DistanceMetric {
    /// Squared Euclidean distance
    L2,
    /// Cosine distance (1 - cosine similarity)
    Cosine,
    /// Negative dot product (higher similarity = lower distance)
    Dot,
}

/// Function type for distance computation.
pub type DistanceFn = fn(&[f32], &[f32]) -> f32;

/// Zero-norm threshold for cosine distance.
pub const COSINE_EPSILON: f32 = 1e-12;

/// Returns the distance function for the given metric.
/// Automatically selects SIMD implementation when available.
pub fn distance_fn(metric: DistanceMetric) -> DistanceFn {
    match metric {
        DistanceMetric::L2 => l2_squared,
        DistanceMetric::Cosine => cosine_distance,
        DistanceMetric::Dot => dot_distance,
    }
}

fn l2_squared(a: &[f32], b: &[f32]) -> f32 {
    scalar::l2_squared(a, b)
}

fn cosine_distance(a: &[f32], b: &[f32]) -> f32 {
    scalar::cosine_distance(a, b)
}

fn dot_distance(a: &[f32], b: &[f32]) -> f32 {
    scalar::dot_distance(a, b)
}
```

- [ ] **Step 2: Create scalar L2 implementation with tests**

Create `vanedb/src/distance/scalar.rs`:
```rust
use crate::distance::COSINE_EPSILON;

pub fn l2_squared(a: &[f32], b: &[f32]) -> f32 {
    debug_assert_eq!(a.len(), b.len());
    a.iter()
        .zip(b.iter())
        .map(|(x, y)| {
            let d = x - y;
            d * d
        })
        .sum()
}

pub fn cosine_distance(_a: &[f32], _b: &[f32]) -> f32 {
    let _ = COSINE_EPSILON; // used in Task 4
    todo!()
}

pub fn dot_distance(_a: &[f32], _b: &[f32]) -> f32 {
    todo!()
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn l2_identical_vectors() {
        let a = vec![1.0, 2.0, 3.0];
        assert_eq!(l2_squared(&a, &a), 0.0);
    }

    #[test]
    fn l2_known_result() {
        let a = vec![1.0, 0.0, 0.0];
        let b = vec![0.0, 1.0, 0.0];
        // (1-0)^2 + (0-1)^2 + (0-0)^2 = 2.0
        assert_eq!(l2_squared(&a, &b), 2.0);
    }

    #[test]
    fn l2_single_dimension() {
        let a = vec![3.0];
        let b = vec![7.0];
        // (3-7)^2 = 16.0
        assert_eq!(l2_squared(&a, &b), 16.0);
    }
}
```

- [ ] **Step 3: Update lib.rs**

Replace `vanedb/src/lib.rs`:
```rust
pub mod distance;
pub mod error;

pub use distance::DistanceMetric;
pub use error::{Result, VaneError};
```

- [ ] **Step 4: Run tests**

Run: `cargo test`
Expected: 6 tests pass (3 error + 3 L2)

- [ ] **Step 5: Commit**

```bash
git add vanedb/src/distance/ vanedb/src/lib.rs
git commit -m "feat: add DistanceMetric enum and scalar L2 distance"
```

---

### Task 4: Scalar cosine + dot distance

**Files:**
- Modify: `vanedb/src/distance/scalar.rs`

- [ ] **Step 1: Add cosine and dot tests to scalar.rs**

Add to the `tests` module in `vanedb/src/distance/scalar.rs`:
```rust
    #[test]
    fn cosine_identical_vectors() {
        let a = vec![1.0, 2.0, 3.0];
        assert!(cosine_distance(&a, &a).abs() < 1e-6);
    }

    #[test]
    fn cosine_orthogonal_vectors() {
        let a = vec![1.0, 0.0];
        let b = vec![0.0, 1.0];
        // cos(90°) = 0, distance = 1.0
        assert!((cosine_distance(&a, &b) - 1.0).abs() < 1e-6);
    }

    #[test]
    fn cosine_opposite_vectors() {
        let a = vec![1.0, 0.0];
        let b = vec![-1.0, 0.0];
        // cos(180°) = -1, distance = 2.0
        assert!((cosine_distance(&a, &b) - 2.0).abs() < 1e-6);
    }

    #[test]
    fn cosine_zero_vector_returns_one() {
        let a = vec![0.0, 0.0, 0.0];
        let b = vec![1.0, 2.0, 3.0];
        assert_eq!(cosine_distance(&a, &b), 1.0);
    }

    #[test]
    fn dot_known_result() {
        let a = vec![1.0, 2.0, 3.0];
        let b = vec![4.0, 5.0, 6.0];
        // -(1*4 + 2*5 + 3*6) = -32.0
        assert_eq!(dot_distance(&a, &b), -32.0);
    }

    #[test]
    fn dot_orthogonal() {
        let a = vec![1.0, 0.0];
        let b = vec![0.0, 1.0];
        assert_eq!(dot_distance(&a, &b), 0.0);
    }
```

- [ ] **Step 2: Run tests to verify cosine/dot tests fail**

Run: `cargo test`
Expected: 6 new tests panic with `todo!()`

- [ ] **Step 3: Implement cosine_distance and dot_distance**

Replace the full content of `vanedb/src/distance/scalar.rs`:
```rust
use crate::distance::COSINE_EPSILON;

pub fn l2_squared(a: &[f32], b: &[f32]) -> f32 {
    debug_assert_eq!(a.len(), b.len());
    a.iter()
        .zip(b.iter())
        .map(|(x, y)| {
            let d = x - y;
            d * d
        })
        .sum()
}

pub fn cosine_distance(a: &[f32], b: &[f32]) -> f32 {
    debug_assert_eq!(a.len(), b.len());
    let mut dot = 0.0f32;
    let mut norm_a = 0.0f32;
    let mut norm_b = 0.0f32;
    for (x, y) in a.iter().zip(b.iter()) {
        dot += x * y;
        norm_a += x * x;
        norm_b += y * y;
    }
    let denom = norm_a * norm_b;
    if denom < COSINE_EPSILON {
        return 1.0;
    }
    let sim = dot / denom.sqrt();
    1.0 - sim.clamp(-1.0, 1.0)
}

pub fn dot_distance(a: &[f32], b: &[f32]) -> f32 {
    debug_assert_eq!(a.len(), b.len());
    -a.iter().zip(b.iter()).map(|(x, y)| x * y).sum::<f32>()
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn l2_identical_vectors() {
        let a = vec![1.0, 2.0, 3.0];
        assert_eq!(l2_squared(&a, &a), 0.0);
    }

    #[test]
    fn l2_known_result() {
        let a = vec![1.0, 0.0, 0.0];
        let b = vec![0.0, 1.0, 0.0];
        assert_eq!(l2_squared(&a, &b), 2.0);
    }

    #[test]
    fn l2_single_dimension() {
        let a = vec![3.0];
        let b = vec![7.0];
        assert_eq!(l2_squared(&a, &b), 16.0);
    }

    #[test]
    fn cosine_identical_vectors() {
        let a = vec![1.0, 2.0, 3.0];
        assert!(cosine_distance(&a, &a).abs() < 1e-6);
    }

    #[test]
    fn cosine_orthogonal_vectors() {
        let a = vec![1.0, 0.0];
        let b = vec![0.0, 1.0];
        assert!((cosine_distance(&a, &b) - 1.0).abs() < 1e-6);
    }

    #[test]
    fn cosine_opposite_vectors() {
        let a = vec![1.0, 0.0];
        let b = vec![-1.0, 0.0];
        assert!((cosine_distance(&a, &b) - 2.0).abs() < 1e-6);
    }

    #[test]
    fn cosine_zero_vector_returns_one() {
        let a = vec![0.0, 0.0, 0.0];
        let b = vec![1.0, 2.0, 3.0];
        assert_eq!(cosine_distance(&a, &b), 1.0);
    }

    #[test]
    fn dot_known_result() {
        let a = vec![1.0, 2.0, 3.0];
        let b = vec![4.0, 5.0, 6.0];
        assert_eq!(dot_distance(&a, &b), -32.0);
    }

    #[test]
    fn dot_orthogonal() {
        let a = vec![1.0, 0.0];
        let b = vec![0.0, 1.0];
        assert_eq!(dot_distance(&a, &b), 0.0);
    }
}
```

- [ ] **Step 4: Run tests**

Run: `cargo test`
Expected: 12 tests pass

- [ ] **Step 5: Commit**

```bash
git add vanedb/src/distance/scalar.rs
git commit -m "feat: add scalar cosine and dot distance functions"
```

---

### Task 5: NEON SIMD distance functions (aarch64)

**Files:**
- Create: `vanedb/src/distance/neon.rs`
- Modify: `vanedb/src/distance/mod.rs`

- [ ] **Step 1: Create NEON implementations with tests**

Create `vanedb/src/distance/neon.rs`:
```rust
#[cfg(target_arch = "aarch64")]
use std::arch::aarch64::*;

use crate::distance::COSINE_EPSILON;

#[cfg(target_arch = "aarch64")]
pub fn l2_squared(a: &[f32], b: &[f32]) -> f32 {
    debug_assert_eq!(a.len(), b.len());
    let n = a.len();
    let mut i = 0;

    // SAFETY: NEON is always available on aarch64.
    // Pointer arithmetic stays within slice bounds (i + 4 <= n).
    let mut sum = unsafe {
        let mut acc = vdupq_n_f32(0.0);
        while i + 4 <= n {
            let va = vld1q_f32(a.as_ptr().add(i));
            let vb = vld1q_f32(b.as_ptr().add(i));
            let d = vsubq_f32(va, vb);
            acc = vmlaq_f32(acc, d, d);
            i += 4;
        }
        vaddvq_f32(acc)
    };

    // Scalar remainder
    while i < n {
        let d = a[i] - b[i];
        sum += d * d;
        i += 1;
    }
    sum
}

#[cfg(target_arch = "aarch64")]
pub fn cosine_distance(a: &[f32], b: &[f32]) -> f32 {
    debug_assert_eq!(a.len(), b.len());
    let n = a.len();
    let mut i = 0;

    let (mut dot, mut norm_a, mut norm_b) = unsafe {
        let mut vdot = vdupq_n_f32(0.0);
        let mut vna = vdupq_n_f32(0.0);
        let mut vnb = vdupq_n_f32(0.0);
        while i + 4 <= n {
            let va = vld1q_f32(a.as_ptr().add(i));
            let vb = vld1q_f32(b.as_ptr().add(i));
            vdot = vmlaq_f32(vdot, va, vb);
            vna = vmlaq_f32(vna, va, va);
            vnb = vmlaq_f32(vnb, vb, vb);
            i += 4;
        }
        (vaddvq_f32(vdot), vaddvq_f32(vna), vaddvq_f32(vnb))
    };

    while i < n {
        dot += a[i] * b[i];
        norm_a += a[i] * a[i];
        norm_b += b[i] * b[i];
        i += 1;
    }

    let denom = norm_a * norm_b;
    if denom < COSINE_EPSILON {
        return 1.0;
    }
    let sim = dot / denom.sqrt();
    1.0 - sim.clamp(-1.0, 1.0)
}

#[cfg(target_arch = "aarch64")]
pub fn dot_distance(a: &[f32], b: &[f32]) -> f32 {
    debug_assert_eq!(a.len(), b.len());
    let n = a.len();
    let mut i = 0;

    let mut sum = unsafe {
        let mut acc = vdupq_n_f32(0.0);
        while i + 4 <= n {
            let va = vld1q_f32(a.as_ptr().add(i));
            let vb = vld1q_f32(b.as_ptr().add(i));
            acc = vmlaq_f32(acc, va, vb);
            i += 4;
        }
        vaddvq_f32(acc)
    };

    while i < n {
        sum += a[i] * b[i];
        i += 1;
    }
    -sum
}

#[cfg(test)]
#[cfg(target_arch = "aarch64")]
mod tests {
    use super::*;

    #[test]
    fn neon_l2_matches_scalar() {
        // 33 elements: tests both SIMD loop (8 iterations) and scalar remainder (1 element)
        let a: Vec<f32> = (0..33).map(|i| i as f32 * 0.1).collect();
        let b: Vec<f32> = (0..33).map(|i| (33 - i) as f32 * 0.1).collect();
        let neon_result = l2_squared(&a, &b);
        let scalar_result = crate::distance::scalar::l2_squared(&a, &b);
        assert!(
            (neon_result - scalar_result).abs() < 1e-4,
            "neon={neon_result}, scalar={scalar_result}"
        );
    }

    #[test]
    fn neon_cosine_matches_scalar() {
        let a: Vec<f32> = (0..33).map(|i| i as f32 * 0.1).collect();
        let b: Vec<f32> = (0..33).map(|i| (33 - i) as f32 * 0.1).collect();
        let neon_result = cosine_distance(&a, &b);
        let scalar_result = crate::distance::scalar::cosine_distance(&a, &b);
        assert!(
            (neon_result - scalar_result).abs() < 1e-4,
            "neon={neon_result}, scalar={scalar_result}"
        );
    }

    #[test]
    fn neon_dot_matches_scalar() {
        let a: Vec<f32> = (0..33).map(|i| i as f32 * 0.1).collect();
        let b: Vec<f32> = (0..33).map(|i| (33 - i) as f32 * 0.1).collect();
        let neon_result = dot_distance(&a, &b);
        let scalar_result = crate::distance::scalar::dot_distance(&a, &b);
        assert!(
            (neon_result - scalar_result).abs() < 1e-4,
            "neon={neon_result}, scalar={scalar_result}"
        );
    }

    #[test]
    fn neon_l2_small_vector() {
        // 3 elements — all scalar remainder, no SIMD loop iterations
        let a = vec![1.0, 2.0, 3.0];
        let b = vec![4.0, 5.0, 6.0];
        assert_eq!(l2_squared(&a, &b), 27.0);
    }
}
```

- [ ] **Step 2: Update distance/mod.rs to dispatch to NEON**

Replace `vanedb/src/distance/mod.rs`:
```rust
pub mod scalar;
#[cfg(target_arch = "aarch64")]
pub mod neon;

/// Distance metric for vector comparison.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum DistanceMetric {
    /// Squared Euclidean distance
    L2,
    /// Cosine distance (1 - cosine similarity)
    Cosine,
    /// Negative dot product (higher similarity = lower distance)
    Dot,
}

/// Function type for distance computation.
pub type DistanceFn = fn(&[f32], &[f32]) -> f32;

/// Zero-norm threshold for cosine distance.
pub const COSINE_EPSILON: f32 = 1e-12;

/// Returns the distance function for the given metric.
/// Automatically selects SIMD implementation when available.
pub fn distance_fn(metric: DistanceMetric) -> DistanceFn {
    match metric {
        DistanceMetric::L2 => l2_squared,
        DistanceMetric::Cosine => cosine_distance,
        DistanceMetric::Dot => dot_distance,
    }
}

fn l2_squared(a: &[f32], b: &[f32]) -> f32 {
    #[cfg(target_arch = "aarch64")]
    {
        neon::l2_squared(a, b)
    }
    #[cfg(not(target_arch = "aarch64"))]
    {
        scalar::l2_squared(a, b)
    }
}

fn cosine_distance(a: &[f32], b: &[f32]) -> f32 {
    #[cfg(target_arch = "aarch64")]
    {
        neon::cosine_distance(a, b)
    }
    #[cfg(not(target_arch = "aarch64"))]
    {
        scalar::cosine_distance(a, b)
    }
}

fn dot_distance(a: &[f32], b: &[f32]) -> f32 {
    #[cfg(target_arch = "aarch64")]
    {
        neon::dot_distance(a, b)
    }
    #[cfg(not(target_arch = "aarch64"))]
    {
        scalar::dot_distance(a, b)
    }
}
```

- [ ] **Step 3: Run tests**

Run: `cargo test`
Expected: On aarch64 (macOS ARM): 16 tests pass (12 scalar + 4 NEON). On x86_64: 12 tests pass (NEON tests excluded by cfg).

- [ ] **Step 4: Commit**

```bash
git add vanedb/src/distance/
git commit -m "feat: add NEON SIMD distance functions for aarch64"
```

---

### Task 6: AVX2 SIMD distance functions (x86_64)

**Files:**
- Create: `vanedb/src/distance/avx2.rs`
- Modify: `vanedb/src/distance/mod.rs`

- [ ] **Step 1: Create AVX2 implementations with tests**

Create `vanedb/src/distance/avx2.rs`:
```rust
#[cfg(target_arch = "x86_64")]
use std::arch::x86_64::*;

use crate::distance::COSINE_EPSILON;

/// Horizontal sum of 8 floats in a 256-bit register.
#[cfg(target_arch = "x86_64")]
#[target_feature(enable = "avx2")]
unsafe fn hsum_avx2(v: __m256) -> f32 {
    let lo = _mm256_castps256_ps128(v);
    let hi = _mm256_extractf128_ps(v, 1);
    let sum128 = _mm_add_ps(lo, hi);
    let shuf = _mm_movehdup_ps(sum128);
    let sum64 = _mm_add_ps(sum128, shuf);
    _mm_cvtss_f32(_mm_add_ss(sum64, _mm_movehl_ps(shuf, sum64)))
}

#[cfg(target_arch = "x86_64")]
#[target_feature(enable = "avx2,fma")]
pub unsafe fn l2_squared(a: &[f32], b: &[f32]) -> f32 {
    debug_assert_eq!(a.len(), b.len());
    let n = a.len();
    let mut i = 0;
    let mut acc = _mm256_setzero_ps();

    while i + 8 <= n {
        let va = _mm256_loadu_ps(a.as_ptr().add(i));
        let vb = _mm256_loadu_ps(b.as_ptr().add(i));
        let d = _mm256_sub_ps(va, vb);
        acc = _mm256_fmadd_ps(d, d, acc);
        i += 8;
    }

    let mut sum = hsum_avx2(acc);
    while i < n {
        let d = a[i] - b[i];
        sum += d * d;
        i += 1;
    }
    sum
}

#[cfg(target_arch = "x86_64")]
#[target_feature(enable = "avx2,fma")]
pub unsafe fn cosine_distance(a: &[f32], b: &[f32]) -> f32 {
    debug_assert_eq!(a.len(), b.len());
    let n = a.len();
    let mut i = 0;
    let mut vdot = _mm256_setzero_ps();
    let mut vna = _mm256_setzero_ps();
    let mut vnb = _mm256_setzero_ps();

    while i + 8 <= n {
        let va = _mm256_loadu_ps(a.as_ptr().add(i));
        let vb = _mm256_loadu_ps(b.as_ptr().add(i));
        vdot = _mm256_fmadd_ps(va, vb, vdot);
        vna = _mm256_fmadd_ps(va, va, vna);
        vnb = _mm256_fmadd_ps(vb, vb, vnb);
        i += 8;
    }

    let mut dot = hsum_avx2(vdot);
    let mut norm_a = hsum_avx2(vna);
    let mut norm_b = hsum_avx2(vnb);

    while i < n {
        dot += a[i] * b[i];
        norm_a += a[i] * a[i];
        norm_b += b[i] * b[i];
        i += 1;
    }

    let denom = norm_a * norm_b;
    if denom < COSINE_EPSILON {
        return 1.0;
    }
    let sim = dot / denom.sqrt();
    1.0 - sim.clamp(-1.0, 1.0)
}

#[cfg(target_arch = "x86_64")]
#[target_feature(enable = "avx2,fma")]
pub unsafe fn dot_distance(a: &[f32], b: &[f32]) -> f32 {
    debug_assert_eq!(a.len(), b.len());
    let n = a.len();
    let mut i = 0;
    let mut acc = _mm256_setzero_ps();

    while i + 8 <= n {
        let va = _mm256_loadu_ps(a.as_ptr().add(i));
        let vb = _mm256_loadu_ps(b.as_ptr().add(i));
        acc = _mm256_fmadd_ps(va, vb, acc);
        i += 8;
    }

    let mut sum = hsum_avx2(acc);
    while i < n {
        sum += a[i] * b[i];
        i += 1;
    }
    -sum
}

#[cfg(test)]
#[cfg(target_arch = "x86_64")]
mod tests {
    use super::*;

    #[test]
    fn avx2_l2_matches_scalar() {
        if !is_x86_feature_detected!("avx2") || !is_x86_feature_detected!("fma") {
            return; // skip on CPUs without AVX2+FMA
        }
        let a: Vec<f32> = (0..33).map(|i| i as f32 * 0.1).collect();
        let b: Vec<f32> = (0..33).map(|i| (33 - i) as f32 * 0.1).collect();
        let avx2_result = unsafe { l2_squared(&a, &b) };
        let scalar_result = crate::distance::scalar::l2_squared(&a, &b);
        assert!(
            (avx2_result - scalar_result).abs() < 1e-4,
            "avx2={avx2_result}, scalar={scalar_result}"
        );
    }

    #[test]
    fn avx2_cosine_matches_scalar() {
        if !is_x86_feature_detected!("avx2") || !is_x86_feature_detected!("fma") {
            return;
        }
        let a: Vec<f32> = (0..33).map(|i| i as f32 * 0.1).collect();
        let b: Vec<f32> = (0..33).map(|i| (33 - i) as f32 * 0.1).collect();
        let avx2_result = unsafe { cosine_distance(&a, &b) };
        let scalar_result = crate::distance::scalar::cosine_distance(&a, &b);
        assert!(
            (avx2_result - scalar_result).abs() < 1e-4,
            "avx2={avx2_result}, scalar={scalar_result}"
        );
    }

    #[test]
    fn avx2_dot_matches_scalar() {
        if !is_x86_feature_detected!("avx2") || !is_x86_feature_detected!("fma") {
            return;
        }
        let a: Vec<f32> = (0..33).map(|i| i as f32 * 0.1).collect();
        let b: Vec<f32> = (0..33).map(|i| (33 - i) as f32 * 0.1).collect();
        let avx2_result = unsafe { dot_distance(&a, &b) };
        let scalar_result = crate::distance::scalar::dot_distance(&a, &b);
        assert!(
            (avx2_result - scalar_result).abs() < 1e-4,
            "avx2={avx2_result}, scalar={scalar_result}"
        );
    }
}
```

- [ ] **Step 2: Update distance/mod.rs to dispatch to AVX2 with runtime detection**

Replace `vanedb/src/distance/mod.rs`:
```rust
pub mod scalar;
#[cfg(target_arch = "aarch64")]
pub mod neon;
#[cfg(target_arch = "x86_64")]
pub mod avx2;

/// Distance metric for vector comparison.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum DistanceMetric {
    /// Squared Euclidean distance
    L2,
    /// Cosine distance (1 - cosine similarity)
    Cosine,
    /// Negative dot product (higher similarity = lower distance)
    Dot,
}

/// Function type for distance computation.
pub type DistanceFn = fn(&[f32], &[f32]) -> f32;

/// Zero-norm threshold for cosine distance.
pub const COSINE_EPSILON: f32 = 1e-12;

/// Returns the distance function for the given metric.
/// Automatically selects SIMD implementation when available.
pub fn distance_fn(metric: DistanceMetric) -> DistanceFn {
    match metric {
        DistanceMetric::L2 => l2_squared,
        DistanceMetric::Cosine => cosine_distance,
        DistanceMetric::Dot => dot_distance,
    }
}

fn l2_squared(a: &[f32], b: &[f32]) -> f32 {
    #[cfg(target_arch = "aarch64")]
    {
        neon::l2_squared(a, b)
    }
    #[cfg(target_arch = "x86_64")]
    {
        if is_x86_feature_detected!("avx2") && is_x86_feature_detected!("fma") {
            unsafe { avx2::l2_squared(a, b) }
        } else {
            scalar::l2_squared(a, b)
        }
    }
    #[cfg(not(any(target_arch = "aarch64", target_arch = "x86_64")))]
    {
        scalar::l2_squared(a, b)
    }
}

fn cosine_distance(a: &[f32], b: &[f32]) -> f32 {
    #[cfg(target_arch = "aarch64")]
    {
        neon::cosine_distance(a, b)
    }
    #[cfg(target_arch = "x86_64")]
    {
        if is_x86_feature_detected!("avx2") && is_x86_feature_detected!("fma") {
            unsafe { avx2::cosine_distance(a, b) }
        } else {
            scalar::cosine_distance(a, b)
        }
    }
    #[cfg(not(any(target_arch = "aarch64", target_arch = "x86_64")))]
    {
        scalar::cosine_distance(a, b)
    }
}

fn dot_distance(a: &[f32], b: &[f32]) -> f32 {
    #[cfg(target_arch = "aarch64")]
    {
        neon::dot_distance(a, b)
    }
    #[cfg(target_arch = "x86_64")]
    {
        if is_x86_feature_detected!("avx2") && is_x86_feature_detected!("fma") {
            unsafe { avx2::dot_distance(a, b) }
        } else {
            scalar::dot_distance(a, b)
        }
    }
    #[cfg(not(any(target_arch = "aarch64", target_arch = "x86_64")))]
    {
        scalar::dot_distance(a, b)
    }
}
```

- [ ] **Step 3: Run tests**

Run: `cargo test`
Expected: All previous tests pass. On x86_64 with AVX2+FMA: 3 additional AVX2 tests pass.

- [ ] **Step 4: Commit**

```bash
git add vanedb/src/distance/
git commit -m "feat: add AVX2 SIMD distance functions for x86_64"
```

---

### Task 7: CI pipeline

**Files:**
- Create: `.github/workflows/ci.yml`

- [ ] **Step 1: Create CI workflow**

Create `.github/workflows/ci.yml`:
```yaml
name: CI

on:
  push:
    branches: [main]
  pull_request:
    branches: [main]

env:
  CARGO_TERM_COLOR: always

jobs:
  check:
    name: Check & Lint
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      - uses: dtolnay/rust-toolchain@stable
        with:
          components: clippy, rustfmt
      - run: cargo fmt --all -- --check
      - run: cargo clippy --all-targets --all-features -- -D warnings

  test-linux:
    name: Test (Linux x86_64)
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      - uses: dtolnay/rust-toolchain@stable
      - run: cargo test --all-features

  test-macos:
    name: Test (macOS ARM)
    runs-on: macos-latest
    steps:
      - uses: actions/checkout@v4
      - uses: dtolnay/rust-toolchain@stable
      - run: cargo test --all-features

  test-windows:
    name: Test (Windows x86_64)
    runs-on: windows-latest
    steps:
      - uses: actions/checkout@v4
      - uses: dtolnay/rust-toolchain@stable
      - run: cargo test --all-features
```

- [ ] **Step 2: Commit and push**

```bash
mkdir -p .github/workflows
git add .github/workflows/ci.yml
git commit -m "ci: add cross-platform test and lint workflow"
git push
```

- [ ] **Step 3: Verify CI passes**

Run: `gh run watch`
Expected: All 4 jobs pass (check, linux, macos, windows)

---

### Task 8: SearchResult type

**Files:**
- Create: `vanedb/src/store/mod.rs`, `vanedb/src/store/search_result.rs`
- Modify: `vanedb/src/lib.rs`

- [ ] **Step 1: Create SearchResult with tests**

Create `vanedb/src/store/search_result.rs`:
```rust
/// A single result from a vector search.
#[derive(Debug, Clone, PartialEq)]
pub struct SearchResult {
    /// The ID of the matched vector.
    pub id: u64,
    /// The distance from the query vector (lower = closer).
    pub distance: f32,
}

impl SearchResult {
    pub fn new(id: u64, distance: f32) -> Self {
        Self { id, distance }
    }
}

impl Eq for SearchResult {}

impl PartialOrd for SearchResult {
    fn partial_cmp(&self, other: &Self) -> Option<std::cmp::Ordering> {
        Some(self.cmp(other))
    }
}

impl Ord for SearchResult {
    fn cmp(&self, other: &Self) -> std::cmp::Ordering {
        self.distance
            .partial_cmp(&other.distance)
            .unwrap_or(std::cmp::Ordering::Equal)
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn search_results_sort_by_distance() {
        let mut results = vec![
            SearchResult::new(1, 5.0),
            SearchResult::new(2, 1.0),
            SearchResult::new(3, 3.0),
        ];
        results.sort();
        assert_eq!(results[0].id, 2);
        assert_eq!(results[1].id, 3);
        assert_eq!(results[2].id, 1);
    }

    #[test]
    fn search_result_equality() {
        let a = SearchResult::new(1, 2.5);
        let b = SearchResult::new(1, 2.5);
        assert_eq!(a, b);
    }
}
```

- [ ] **Step 2: Create store/mod.rs**

Create `vanedb/src/store/mod.rs`:
```rust
mod search_result;

pub use search_result::SearchResult;
```

- [ ] **Step 3: Update lib.rs**

Replace `vanedb/src/lib.rs`:
```rust
pub mod distance;
pub mod error;
pub mod store;

pub use distance::DistanceMetric;
pub use error::{Result, VaneError};
pub use store::SearchResult;
```

- [ ] **Step 4: Run tests**

Run: `cargo test`
Expected: All tests pass including 2 new SearchResult tests

- [ ] **Step 5: Commit**

```bash
git add vanedb/src/store/ vanedb/src/lib.rs
git commit -m "feat: add SearchResult type with ordering"
```

---

### Task 9: VectorStore — struct, add, get, remove

**Files:**
- Create: `vanedb/src/store/vector_store.rs`
- Modify: `vanedb/src/store/mod.rs`, `vanedb/src/lib.rs`

- [ ] **Step 1: Create VectorStore with add, get, remove, and tests**

Create `vanedb/src/store/vector_store.rs`:
```rust
use std::collections::HashMap;

use parking_lot::RwLock;

use crate::distance::{distance_fn, DistanceFn, DistanceMetric};
use crate::error::{Result, VaneError};
use crate::store::SearchResult;

pub struct VectorStore {
    dim: usize,
    metric: DistanceMetric,
    dist_fn: DistanceFn,
    inner: RwLock<Inner>,
}

struct Inner {
    ids: Vec<u64>,
    data: Vec<f32>,
    id_to_index: HashMap<u64, usize>,
}

impl VectorStore {
    pub fn new(dim: usize, metric: DistanceMetric) -> Result<Self> {
        if dim == 0 {
            return Err(VaneError::EmptyVector);
        }
        Ok(Self {
            dim,
            metric,
            dist_fn: distance_fn(metric),
            inner: RwLock::new(Inner {
                ids: Vec::new(),
                data: Vec::new(),
                id_to_index: HashMap::new(),
            }),
        })
    }

    pub fn add(&self, id: u64, vector: &[f32]) -> Result<()> {
        if vector.len() != self.dim {
            return Err(VaneError::DimensionMismatch {
                expected: self.dim,
                got: vector.len(),
            });
        }
        let mut inner = self.inner.write();
        if inner.id_to_index.contains_key(&id) {
            return Err(VaneError::DuplicateId { id });
        }
        let index = inner.ids.len();
        inner.ids.push(id);
        inner.data.extend_from_slice(vector);
        inner.id_to_index.insert(id, index);
        Ok(())
    }

    pub fn get(&self, id: u64) -> Result<Vec<f32>> {
        let inner = self.inner.read();
        let &index = inner
            .id_to_index
            .get(&id)
            .ok_or(VaneError::NotFound { id })?;
        let start = index * self.dim;
        Ok(inner.data[start..start + self.dim].to_vec())
    }

    pub fn remove(&self, id: u64) -> Result<()> {
        let mut inner = self.inner.write();
        let index = inner
            .id_to_index
            .remove(&id)
            .ok_or(VaneError::NotFound { id })?;
        let last = inner.ids.len() - 1;
        if index != last {
            // Swap-remove: move last vector into the removed slot
            let last_id = inner.ids[last];
            inner.ids[index] = last_id;
            let src_start = last * self.dim;
            let dst_start = index * self.dim;
            for j in 0..self.dim {
                inner.data[dst_start + j] = inner.data[src_start + j];
            }
            inner.id_to_index.insert(last_id, index);
        }
        inner.ids.pop();
        inner.data.truncate(inner.ids.len() * self.dim);
        Ok(())
    }

    pub fn contains(&self, id: u64) -> bool {
        self.inner.read().id_to_index.contains_key(&id)
    }

    pub fn len(&self) -> usize {
        self.inner.read().ids.len()
    }

    pub fn is_empty(&self) -> bool {
        self.len() == 0
    }

    pub fn dimension(&self) -> usize {
        self.dim
    }

    pub fn metric(&self) -> DistanceMetric {
        self.metric
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn new_rejects_zero_dimension() {
        assert!(VectorStore::new(0, DistanceMetric::L2).is_err());
    }

    #[test]
    fn add_and_get() {
        let store = VectorStore::new(3, DistanceMetric::L2).unwrap();
        let vec = vec![1.0, 2.0, 3.0];
        store.add(1, &vec).unwrap();
        assert_eq!(store.get(1).unwrap(), vec);
    }

    #[test]
    fn add_wrong_dimension() {
        let store = VectorStore::new(3, DistanceMetric::L2).unwrap();
        let result = store.add(1, &[1.0, 2.0]);
        assert!(matches!(
            result,
            Err(VaneError::DimensionMismatch {
                expected: 3,
                got: 2
            })
        ));
    }

    #[test]
    fn add_duplicate_id() {
        let store = VectorStore::new(3, DistanceMetric::L2).unwrap();
        store.add(1, &[1.0, 2.0, 3.0]).unwrap();
        assert!(matches!(
            store.add(1, &[4.0, 5.0, 6.0]),
            Err(VaneError::DuplicateId { id: 1 })
        ));
    }

    #[test]
    fn get_missing_id() {
        let store = VectorStore::new(3, DistanceMetric::L2).unwrap();
        assert!(matches!(
            store.get(42),
            Err(VaneError::NotFound { id: 42 })
        ));
    }

    #[test]
    fn remove_vector() {
        let store = VectorStore::new(3, DistanceMetric::L2).unwrap();
        store.add(1, &[1.0, 2.0, 3.0]).unwrap();
        store.add(2, &[4.0, 5.0, 6.0]).unwrap();
        store.remove(1).unwrap();
        assert!(!store.contains(1));
        assert!(store.contains(2));
        assert_eq!(store.len(), 1);
        // After swap-remove, vector 2 should still be retrievable
        assert_eq!(store.get(2).unwrap(), vec![4.0, 5.0, 6.0]);
    }

    #[test]
    fn remove_missing_id() {
        let store = VectorStore::new(3, DistanceMetric::L2).unwrap();
        assert!(matches!(
            store.remove(42),
            Err(VaneError::NotFound { id: 42 })
        ));
    }

    #[test]
    fn len_and_is_empty() {
        let store = VectorStore::new(3, DistanceMetric::L2).unwrap();
        assert!(store.is_empty());
        assert_eq!(store.len(), 0);
        store.add(1, &[1.0, 2.0, 3.0]).unwrap();
        assert!(!store.is_empty());
        assert_eq!(store.len(), 1);
    }
}
```

- [ ] **Step 2: Export VectorStore**

Replace `vanedb/src/store/mod.rs`:
```rust
mod search_result;
mod vector_store;

pub use search_result::SearchResult;
pub use vector_store::VectorStore;
```

Replace `vanedb/src/lib.rs`:
```rust
pub mod distance;
pub mod error;
pub mod store;

pub use distance::DistanceMetric;
pub use error::{Result, VaneError};
pub use store::{SearchResult, VectorStore};
```

- [ ] **Step 3: Run tests**

Run: `cargo test`
Expected: All tests pass including 8 new VectorStore tests

- [ ] **Step 4: Commit**

```bash
git add vanedb/src/store/ vanedb/src/lib.rs
git commit -m "feat: add VectorStore with add, get, remove operations"
```

---

### Task 10: VectorStore search

**Files:**
- Modify: `vanedb/src/store/vector_store.rs`

- [ ] **Step 1: Add search tests**

Add to the `tests` module in `vanedb/src/store/vector_store.rs`:
```rust
    #[test]
    fn search_l2_finds_nearest() {
        let store = VectorStore::new(2, DistanceMetric::L2).unwrap();
        store.add(1, &[0.0, 0.0]).unwrap();
        store.add(2, &[1.0, 0.0]).unwrap();
        store.add(3, &[10.0, 10.0]).unwrap();

        let results = store.search(&[0.0, 0.1], 2).unwrap();
        assert_eq!(results.len(), 2);
        assert_eq!(results[0].id, 1); // closest
        assert_eq!(results[1].id, 2); // second closest
    }

    #[test]
    fn search_cosine_finds_similar() {
        let store = VectorStore::new(2, DistanceMetric::Cosine).unwrap();
        store.add(1, &[1.0, 0.0]).unwrap(); // pointing right
        store.add(2, &[0.0, 1.0]).unwrap(); // pointing up
        store.add(3, &[-1.0, 0.0]).unwrap(); // pointing left

        let results = store.search(&[0.9, 0.1], 1).unwrap();
        assert_eq!(results[0].id, 1); // most similar direction
    }

    #[test]
    fn search_k_larger_than_store() {
        let store = VectorStore::new(2, DistanceMetric::L2).unwrap();
        store.add(1, &[0.0, 0.0]).unwrap();
        let results = store.search(&[1.0, 1.0], 10).unwrap();
        assert_eq!(results.len(), 1); // only 1 vector in store
    }

    #[test]
    fn search_empty_store() {
        let store = VectorStore::new(2, DistanceMetric::L2).unwrap();
        let results = store.search(&[1.0, 1.0], 5).unwrap();
        assert!(results.is_empty());
    }

    #[test]
    fn search_wrong_dimension() {
        let store = VectorStore::new(3, DistanceMetric::L2).unwrap();
        assert!(matches!(
            store.search(&[1.0, 2.0], 5),
            Err(VaneError::DimensionMismatch {
                expected: 3,
                got: 2
            })
        ));
    }

    #[test]
    fn search_k_zero() {
        let store = VectorStore::new(2, DistanceMetric::L2).unwrap();
        assert!(matches!(
            store.search(&[1.0, 2.0], 0),
            Err(VaneError::InvalidK)
        ));
    }
```

- [ ] **Step 2: Run tests to verify they fail**

Run: `cargo test`
Expected: FAIL — `search` method not defined

- [ ] **Step 3: Implement search**

Add the `search` method to `VectorStore` in `vanedb/src/store/vector_store.rs` (add it after the `metric()` method, before the closing `}`):
```rust
    pub fn search(&self, query: &[f32], k: usize) -> Result<Vec<SearchResult>> {
        if query.len() != self.dim {
            return Err(VaneError::DimensionMismatch {
                expected: self.dim,
                got: query.len(),
            });
        }
        if k == 0 {
            return Err(VaneError::InvalidK);
        }
        let inner = self.inner.read();
        let n = inner.ids.len();
        if n == 0 {
            return Ok(Vec::new());
        }

        let mut results: Vec<SearchResult> = (0..n)
            .map(|i| {
                let start = i * self.dim;
                let vec = &inner.data[start..start + self.dim];
                SearchResult::new(inner.ids[i], (self.dist_fn)(query, vec))
            })
            .collect();

        results.sort();
        results.truncate(k);
        Ok(results)
    }
```

- [ ] **Step 4: Run tests**

Run: `cargo test`
Expected: All tests pass

- [ ] **Step 5: Commit**

```bash
git add vanedb/src/store/vector_store.rs
git commit -m "feat: add brute-force k-NN search to VectorStore"
```

---

### Task 11: Thread safety tests

**Files:**
- Modify: `vanedb/src/store/vector_store.rs`

- [ ] **Step 1: Add concurrent access tests**

Add to the `tests` module in `vanedb/src/store/vector_store.rs`:
```rust
    #[test]
    fn concurrent_add_and_search() {
        use std::sync::Arc;
        use std::thread;

        let store = Arc::new(VectorStore::new(3, DistanceMetric::L2).unwrap());
        let mut handles = vec![];

        // 10 writer threads
        for i in 0..10u64 {
            let store = Arc::clone(&store);
            handles.push(thread::spawn(move || {
                let v = vec![i as f32; 3];
                store.add(i, &v).unwrap();
            }));
        }

        for h in handles {
            h.join().unwrap();
        }

        assert_eq!(store.len(), 10);

        // 10 reader threads searching concurrently
        let mut handles = vec![];
        for _ in 0..10 {
            let store = Arc::clone(&store);
            handles.push(thread::spawn(move || {
                let results = store.search(&[5.0, 5.0, 5.0], 3).unwrap();
                assert_eq!(results.len(), 3);
            }));
        }

        for h in handles {
            h.join().unwrap();
        }
    }

    #[test]
    fn store_is_send_sync() {
        fn assert_send_sync<T: Send + Sync>() {}
        assert_send_sync::<VectorStore>();
    }
```

- [ ] **Step 2: Run tests**

Run: `cargo test`
Expected: All tests pass including thread safety tests

- [ ] **Step 3: Commit**

```bash
git add vanedb/src/store/vector_store.rs
git commit -m "test: add thread safety tests for VectorStore"
```

---

### Task 12: Property-based tests

**Files:**
- Create: `vanedb/tests/property_tests.rs`

- [ ] **Step 1: Write property-based tests**

Create `vanedb/tests/property_tests.rs`:
```rust
use proptest::prelude::*;
use vanedb::distance::{self, DistanceMetric};

fn arb_vector(dim: usize) -> impl Strategy<Value = Vec<f32>> {
    prop::collection::vec(-100.0f32..100.0, dim)
}

proptest! {
    #[test]
    fn l2_is_non_negative(a in arb_vector(128), b in arb_vector(128)) {
        let dist_fn = distance::distance_fn(DistanceMetric::L2);
        let d = dist_fn(&a, &b);
        prop_assert!(d >= 0.0, "L2 distance was negative: {d}");
    }

    #[test]
    fn l2_self_distance_is_zero(a in arb_vector(128)) {
        let dist_fn = distance::distance_fn(DistanceMetric::L2);
        let d = dist_fn(&a, &a);
        prop_assert!(d.abs() < 1e-5, "L2 self-distance was {d}");
    }

    #[test]
    fn l2_is_symmetric(a in arb_vector(64), b in arb_vector(64)) {
        let dist_fn = distance::distance_fn(DistanceMetric::L2);
        let d_ab = dist_fn(&a, &b);
        let d_ba = dist_fn(&b, &a);
        prop_assert!((d_ab - d_ba).abs() < 1e-4,
            "L2 not symmetric: {d_ab} vs {d_ba}");
    }

    #[test]
    fn cosine_is_bounded(a in arb_vector(64), b in arb_vector(64)) {
        let dist_fn = distance::distance_fn(DistanceMetric::Cosine);
        let d = dist_fn(&a, &b);
        prop_assert!(d >= 0.0 && d <= 2.0,
            "Cosine distance out of [0, 2]: {d}");
    }

    #[test]
    fn cosine_self_distance_near_zero(
        a in prop::collection::vec(0.1f32..100.0, 64)
    ) {
        let dist_fn = distance::distance_fn(DistanceMetric::Cosine);
        let d = dist_fn(&a, &a);
        prop_assert!(d.abs() < 1e-5, "Cosine self-distance was {d}");
    }

    #[test]
    fn dot_is_symmetric(a in arb_vector(64), b in arb_vector(64)) {
        let dist_fn = distance::distance_fn(DistanceMetric::Dot);
        let d_ab = dist_fn(&a, &b);
        let d_ba = dist_fn(&b, &a);
        prop_assert!((d_ab - d_ba).abs() < 1e-3,
            "Dot not symmetric: {d_ab} vs {d_ba}");
    }
}
```

- [ ] **Step 2: Run property tests**

Run: `cargo test --test property_tests`
Expected: All 6 property tests pass (each runs many randomized cases)

- [ ] **Step 3: Run full test suite + clippy**

Run: `cargo test && cargo clippy --all-targets -- -D warnings`
Expected: All tests pass, no clippy warnings

- [ ] **Step 4: Commit and push**

```bash
git add vanedb/tests/
git commit -m "test: add property-based tests for distance functions"
git push
```

---

## Future Plans

The following phases from the spec will be implemented as separate plans:

- **Phase 4:** HNSW index + persistence (`bincode`/`rkyv` serialization)
- **Phase 5:** MmapVectorStore (`memmap2`)
- **Phase 6:** Performance benchmarks + CI regression testing (`criterion` + `critcmp`)
- **Phase 7:** GPU — Metal + CUDA (`metal-rs`, `cudarc`)
- **Phase 8:** Python bindings (PyO3 + maturin)
- **Phase 9:** WASM bindings (`wasm-bindgen` + `wasm-pack`)
