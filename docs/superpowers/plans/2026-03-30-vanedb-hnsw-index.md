# VaneDB HNSW Index — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add an HNSW approximate nearest-neighbor index to VaneDB — the core data structure for fast similarity search.

**Architecture:** Port the existing C++ HNSWIndex (~440 lines) to idiomatic Rust. Single `hnsw/` module with the index struct, builder, graph operations, and binary persistence via `bincode`+`serde`. Thread safety via `parking_lot::RwLock` (read for search, write for add). Pre-allocated flat vector storage matching the C++ approach for cache-friendly search.

**Tech Stack:** `parking_lot` (RwLock), `rand 0.9` (level generation), `serde 1.0` + `bincode 1.3` (persistence)

**Spec:** `docs/superpowers/specs/2026-03-29-vanedb-rust-rewrite-design.md` — Phase 4

**C++ reference:** `/Users/anton/code/quiverdb/src/core/hnsw_index.h`

---

## File Structure

| File | Responsibility |
|------|---------------|
| `vanedb/Cargo.toml` | Add `rand`, `serde`, `bincode` dependencies |
| `vanedb/src/error.rs` | Add `IndexFull`, `InvalidParameter`, `Io` error variants |
| `vanedb/src/lib.rs` | Add `hnsw` module export, re-export `HnswIndex` |
| `vanedb/src/hnsw/mod.rs` | `HnswIndex` struct, `HnswIndexBuilder`, public API, graph operations |
| `vanedb/src/hnsw/persistence.rs` | `save()` / `load()` with bincode serialization |
| `vanedb/tests/hnsw_tests.rs` | Integration tests: recall, round-trip persistence |

---

### Task 1: Dependencies + error variants

**Files:**
- Modify: `vanedb/Cargo.toml`, `vanedb/src/error.rs`

- [ ] **Step 1: Add dependencies to Cargo.toml**

Add to `[dependencies]` in `vanedb/Cargo.toml`:
```toml
rand = "0.9"
serde = { version = "1.0", features = ["derive"] }
bincode = "1.3"
```

- [ ] **Step 2: Add new error variants**

Replace `vanedb/src/error.rs` with:
```rust
#[derive(Debug, Clone, PartialEq, Eq)]
pub enum VaneError {
    DimensionMismatch { expected: usize, got: usize },
    EmptyVector,
    NotFound { id: u64 },
    DuplicateId { id: u64 },
    InvalidK,
    IndexFull,
    InvalidParameter(&'static str),
    Io(String),
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
            Self::IndexFull => write!(f, "index is full"),
            Self::InvalidParameter(msg) => write!(f, "invalid parameter: {msg}"),
            Self::Io(msg) => write!(f, "I/O error: {msg}"),
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
        let err = VaneError::DimensionMismatch {
            expected: 768,
            got: 512,
        };
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

    #[test]
    fn error_display_index_full() {
        assert_eq!(VaneError::IndexFull.to_string(), "index is full");
    }

    #[test]
    fn error_display_invalid_parameter() {
        let err = VaneError::InvalidParameter("M must be >= 2");
        assert_eq!(err.to_string(), "invalid parameter: M must be >= 2");
    }
}
```

- [ ] **Step 3: Run tests**

Run: `cargo test`
Expected: All existing tests pass + 2 new error tests

- [ ] **Step 4: Commit**

```bash
git add vanedb/Cargo.toml vanedb/src/error.rs
git commit -m "feat: add HNSW dependencies and error variants"
```

---

### Task 2: HnswIndex builder + empty struct

**Files:**
- Create: `vanedb/src/hnsw/mod.rs`
- Modify: `vanedb/src/lib.rs`

- [ ] **Step 1: Create HNSW module with builder and struct**

Create `vanedb/src/hnsw/mod.rs`:
```rust
use std::collections::HashMap;
use std::sync::atomic::{AtomicUsize, Ordering};

use parking_lot::RwLock;
use rand::rngs::StdRng;
use rand::SeedableRng;

use crate::distance::{distance_fn, DistanceFn, DistanceMetric};
use crate::error::{Result, VaneError};
use crate::store::SearchResult;

const MAX_LEVEL: i32 = 32;
const MIN_LEVEL_RANDOM: f64 = 1e-9;

pub struct HnswIndex {
    dim: usize,
    metric: DistanceMetric,
    dist_fn: DistanceFn,
    max_elements: usize,
    m: usize,
    m_max: usize,
    m_max0: usize,
    ef_construction: usize,
    ef_search: AtomicUsize,
    mult: f64,
    inner: RwLock<Inner>,
}

struct Inner {
    vectors: Vec<f32>,
    ext_ids: Vec<u64>,
    id_map: HashMap<u64, usize>,
    levels: Vec<i32>,
    neighbors: Vec<Vec<Vec<usize>>>,
    entry_point: Option<usize>,
    max_level: i32,
    count: usize,
    rng: StdRng,
}

pub struct HnswIndexBuilder {
    dim: usize,
    metric: DistanceMetric,
    capacity: usize,
    m: usize,
    ef_construction: usize,
    seed: u64,
}

impl HnswIndex {
    pub fn builder(dim: usize, metric: DistanceMetric) -> HnswIndexBuilder {
        HnswIndexBuilder {
            dim,
            metric,
            capacity: 100_000,
            m: 16,
            ef_construction: 200,
            seed: 42,
        }
    }

    pub fn size(&self) -> usize {
        self.inner.read().count
    }

    pub fn is_empty(&self) -> bool {
        self.size() == 0
    }

    pub fn capacity(&self) -> usize {
        self.max_elements
    }

    pub fn dimension(&self) -> usize {
        self.dim
    }

    pub fn metric(&self) -> DistanceMetric {
        self.metric
    }

    pub fn contains(&self, id: u64) -> bool {
        self.inner.read().id_map.contains_key(&id)
    }

    pub fn get_vector(&self, id: u64) -> Result<Vec<f32>> {
        let inner = self.inner.read();
        let &iid = inner.id_map.get(&id).ok_or(VaneError::NotFound { id })?;
        let start = iid * self.dim;
        Ok(inner.vectors[start..start + self.dim].to_vec())
    }

    pub fn set_ef_search(&self, ef: usize) {
        self.ef_search.store(ef, Ordering::Relaxed);
    }

    pub fn get_ef_search(&self) -> usize {
        self.ef_search.load(Ordering::Relaxed)
    }
}

impl HnswIndexBuilder {
    pub fn capacity(mut self, cap: usize) -> Self {
        self.capacity = cap;
        self
    }

    pub fn m(mut self, m: usize) -> Self {
        self.m = m;
        self
    }

    pub fn ef_construction(mut self, ef: usize) -> Self {
        self.ef_construction = ef;
        self
    }

    pub fn seed(mut self, seed: u64) -> Self {
        self.seed = seed;
        self
    }

    pub fn build(self) -> Result<HnswIndex> {
        if self.dim == 0 {
            return Err(VaneError::EmptyVector);
        }
        if self.capacity == 0 {
            return Err(VaneError::InvalidParameter("capacity must be > 0"));
        }
        if self.m < 2 {
            return Err(VaneError::InvalidParameter("M must be >= 2"));
        }
        let ef_construction = self.ef_construction.max(self.m);
        let mult = if self.m > 1 {
            1.0 / (self.m as f64).ln()
        } else {
            1.0
        };
        Ok(HnswIndex {
            dim: self.dim,
            metric: self.metric,
            dist_fn: distance_fn(self.metric),
            max_elements: self.capacity,
            m: self.m,
            m_max: self.m,
            m_max0: self.m * 2,
            ef_construction,
            ef_search: AtomicUsize::new(50),
            mult,
            inner: RwLock::new(Inner {
                vectors: vec![0.0; self.capacity * self.dim],
                ext_ids: vec![0; self.capacity],
                id_map: HashMap::new(),
                levels: vec![0; self.capacity],
                neighbors: (0..self.capacity).map(|_| Vec::new()).collect(),
                entry_point: None,
                max_level: -1,
                count: 0,
                rng: StdRng::seed_from_u64(self.seed),
            }),
        })
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn builder_defaults() {
        let idx = HnswIndex::builder(128, DistanceMetric::Cosine)
            .build()
            .unwrap();
        assert_eq!(idx.dimension(), 128);
        assert_eq!(idx.capacity(), 100_000);
        assert!(idx.is_empty());
        assert_eq!(idx.size(), 0);
        assert_eq!(idx.get_ef_search(), 50);
    }

    #[test]
    fn builder_custom_params() {
        let idx = HnswIndex::builder(64, DistanceMetric::L2)
            .capacity(1000)
            .m(32)
            .ef_construction(400)
            .seed(123)
            .build()
            .unwrap();
        assert_eq!(idx.capacity(), 1000);
    }

    #[test]
    fn builder_rejects_zero_dim() {
        assert!(HnswIndex::builder(0, DistanceMetric::L2).build().is_err());
    }

    #[test]
    fn builder_rejects_zero_capacity() {
        assert!(HnswIndex::builder(64, DistanceMetric::L2)
            .capacity(0)
            .build()
            .is_err());
    }

    #[test]
    fn builder_rejects_m_below_2() {
        assert!(HnswIndex::builder(64, DistanceMetric::L2)
            .m(1)
            .build()
            .is_err());
    }

    #[test]
    fn set_ef_search() {
        let idx = HnswIndex::builder(64, DistanceMetric::L2)
            .build()
            .unwrap();
        idx.set_ef_search(100);
        assert_eq!(idx.get_ef_search(), 100);
    }
}
```

- [ ] **Step 2: Update lib.rs**

Replace `vanedb/src/lib.rs`:
```rust
pub mod distance;
pub mod error;
pub mod hnsw;
pub mod store;

pub use distance::DistanceMetric;
pub use error::{Result, VaneError};
pub use hnsw::HnswIndex;
pub use store::{SearchResult, VectorStore};
```

- [ ] **Step 3: Run tests**

Run: `cargo test`
Expected: All existing tests pass + 6 new builder tests

- [ ] **Step 4: Commit**

```bash
git add vanedb/src/hnsw/ vanedb/src/lib.rs
git commit -m "feat: add HnswIndex struct with builder pattern"
```

---

### Task 3: Add operation + internal graph methods

**Files:**
- Modify: `vanedb/src/hnsw/mod.rs`

- [ ] **Step 1: Add internal graph methods and the `add` method**

Add these methods to the `impl HnswIndex` block in `vanedb/src/hnsw/mod.rs`, after `get_ef_search()`:

```rust
    pub fn add(&self, id: u64, vector: &[f32]) -> Result<()> {
        if vector.len() != self.dim {
            return Err(VaneError::DimensionMismatch {
                expected: self.dim,
                got: vector.len(),
            });
        }
        let mut inner = self.inner.write();
        if inner.id_map.contains_key(&id) {
            return Err(VaneError::DuplicateId { id });
        }
        if inner.count >= self.max_elements {
            return Err(VaneError::IndexFull);
        }

        let iid = inner.count;
        inner.count += 1;
        inner.id_map.insert(id, iid);
        inner.ext_ids[iid] = id;
        let start = iid * self.dim;
        inner.vectors[start..start + self.dim].copy_from_slice(vector);

        let level = Self::get_level(&mut inner.rng, self.mult);
        inner.levels[iid] = level;
        inner.neighbors[iid] = Vec::with_capacity((level + 1) as usize);
        for l in 0..=level {
            let cap = if l == 0 { self.m_max0 } else { self.m_max };
            inner.neighbors[iid].push(Vec::with_capacity(cap));
        }

        if inner.entry_point.is_none() {
            inner.entry_point = Some(iid);
            inner.max_level = level;
            return Ok(());
        }

        let mut curr = inner.entry_point.unwrap();
        let cur_max_level = inner.max_level;

        // Greedy descent through upper layers
        if level < cur_max_level {
            let mut d = (self.dist_fn)(vector, Self::get_vec(&inner.vectors, curr, self.dim));
            for l in (level + 1..=cur_max_level).rev() {
                let mut changed = true;
                while changed {
                    changed = false;
                    if (l as usize) < inner.neighbors[curr].len() {
                        for &n in &inner.neighbors[curr][l as usize] {
                            let nd =
                                (self.dist_fn)(vector, Self::get_vec(&inner.vectors, n, self.dim));
                            if nd < d {
                                d = nd;
                                curr = n;
                                changed = true;
                            }
                        }
                    }
                }
            }
        }

        // Insert at each layer from min(level, cur_max_level) down to 0
        for l in (0..=level.min(cur_max_level)).rev() {
            let lu = l as usize;
            let top = Self::search_layer(
                &inner.vectors,
                self.dist_fn,
                self.dim,
                &inner.neighbors,
                vector,
                curr,
                self.ef_construction,
                lu,
            );
            let sel = Self::select_neighbors(
                &inner.vectors,
                self.dist_fn,
                self.dim,
                &top,
                self.m,
            );

            inner.neighbors[iid][lu] = sel.iter().map(|&(_, nid)| nid).collect();

            let max_conn = if lu == 0 { self.m_max0 } else { self.m_max };
            for &(_, nid) in &sel {
                if lu < inner.neighbors[nid].len() {
                    let nc = &mut inner.neighbors[nid][lu];
                    if nc.len() < max_conn {
                        nc.push(iid);
                    } else {
                        // Prune: keep closest max_conn neighbors
                        let mut cands: Vec<(f32, usize)> = nc
                            .iter()
                            .map(|&c| {
                                let d = (self.dist_fn)(
                                    Self::get_vec(&inner.vectors, nid, self.dim),
                                    Self::get_vec(&inner.vectors, c, self.dim),
                                );
                                (d, c)
                            })
                            .collect();
                        cands.push((
                            (self.dist_fn)(
                                Self::get_vec(&inner.vectors, nid, self.dim),
                                vector,
                            ),
                            iid,
                        ));
                        cands.sort_by(|a, b| a.0.partial_cmp(&b.0).unwrap());
                        cands.truncate(max_conn);
                        *nc = cands.into_iter().map(|(_, c)| c).collect();
                    }
                }
            }

            // Use closest from search results as entry for next layer
            if let Some(&(_, best)) = top.first() {
                curr = best;
            }
        }

        if level > cur_max_level {
            inner.entry_point = Some(iid);
            inner.max_level = level;
        }
        Ok(())
    }

    fn get_level(rng: &mut StdRng, mult: f64) -> i32 {
        use rand::Rng;
        let r: f64 = rng.random::<f64>().max(MIN_LEVEL_RANDOM);
        let level = (-r.ln() * mult) as i32;
        level.min(MAX_LEVEL)
    }

    fn get_vec(vectors: &[f32], iid: usize, dim: usize) -> &[f32] {
        let start = iid * dim;
        &vectors[start..start + dim]
    }

    /// Beam search on a single graph layer. Returns candidates sorted by distance (ascending).
    fn search_layer(
        vectors: &[f32],
        dist_fn: DistanceFn,
        dim: usize,
        neighbors: &[Vec<Vec<usize>>],
        query: &[f32],
        entry: usize,
        ef: usize,
        level: usize,
    ) -> Vec<(f32, usize)> {
        use std::collections::{BinaryHeap, HashSet};
        use std::cmp::Reverse;

        let mut visited = HashSet::new();
        visited.insert(entry);

        let d = dist_fn(query, Self::get_vec(vectors, entry, dim));

        // Min-heap for candidates (closest first)
        let mut candidates: BinaryHeap<Reverse<(FloatOrd, usize)>> = BinaryHeap::new();
        candidates.push(Reverse((FloatOrd(d), entry)));

        // Max-heap for results (farthest first, so we can pop the worst)
        let mut results: BinaryHeap<(FloatOrd, usize)> = BinaryHeap::new();
        results.push((FloatOrd(d), entry));

        let mut lower_bound = d;

        while let Some(Reverse((FloatOrd(cd), cid))) = candidates.pop() {
            if cd > lower_bound && results.len() >= ef {
                break;
            }
            if level < neighbors[cid].len() {
                for &n in &neighbors[cid][level] {
                    if visited.contains(&n) {
                        continue;
                    }
                    visited.insert(n);
                    let nd = dist_fn(query, Self::get_vec(vectors, n, dim));
                    if results.len() < ef || nd < lower_bound {
                        candidates.push(Reverse((FloatOrd(nd), n)));
                        results.push((FloatOrd(nd), n));
                        if results.len() > ef {
                            results.pop();
                        }
                        if let Some(&(FloatOrd(top), _)) = results.peek() {
                            lower_bound = top;
                        }
                    }
                }
            }
        }

        let mut sorted: Vec<(f32, usize)> =
            results.into_iter().map(|(FloatOrd(d), id)| (d, id)).collect();
        sorted.sort_by(|a, b| a.0.partial_cmp(&b.0).unwrap());
        sorted
    }

    /// Heuristic neighbor selection (Algorithm 4 from the HNSW paper).
    fn select_neighbors(
        vectors: &[f32],
        dist_fn: DistanceFn,
        dim: usize,
        candidates: &[(f32, usize)],
        m: usize,
    ) -> Vec<(f32, usize)> {
        if candidates.len() <= m {
            return candidates.to_vec();
        }

        let mut result = Vec::with_capacity(m);
        for &(dq, cid) in candidates {
            if result.len() >= m {
                break;
            }
            let mut ok = true;
            for &(_, sid) in &result {
                let ds = dist_fn(
                    Self::get_vec(vectors, cid, dim),
                    Self::get_vec(vectors, sid, dim),
                );
                if ds < dq {
                    ok = false;
                    break;
                }
            }
            if ok {
                result.push((dq, cid));
            }
        }
        // Fill remaining with closest candidates not already selected
        if result.len() < m {
            let selected: HashSet<usize> = result.iter().map(|&(_, id)| id).collect();
            for &(d, cid) in candidates {
                if result.len() >= m {
                    break;
                }
                if !selected.contains(&cid) {
                    result.push((d, cid));
                }
            }
        }
        result
    }
```

Also add this helper struct at the top of the file (after the `use` statements, before `const MAX_LEVEL`):

```rust
use std::collections::HashSet;

/// Wrapper for f32 that implements Ord (needed for BinaryHeap).
#[derive(Debug, Clone, Copy, PartialEq)]
struct FloatOrd(f32);

impl Eq for FloatOrd {}

impl PartialOrd for FloatOrd {
    fn partial_cmp(&self, other: &Self) -> Option<std::cmp::Ordering> {
        Some(self.cmp(other))
    }
}

impl Ord for FloatOrd {
    fn cmp(&self, other: &Self) -> std::cmp::Ordering {
        self.0.partial_cmp(&other.0).unwrap_or(std::cmp::Ordering::Equal)
    }
}
```

- [ ] **Step 2: Add tests for the add operation**

Add to the `tests` module:
```rust
    #[test]
    fn add_single_vector() {
        let idx = HnswIndex::builder(3, DistanceMetric::L2)
            .capacity(100)
            .build()
            .unwrap();
        idx.add(1, &[1.0, 2.0, 3.0]).unwrap();
        assert_eq!(idx.size(), 1);
        assert!(idx.contains(1));
        assert_eq!(idx.get_vector(1).unwrap(), vec![1.0, 2.0, 3.0]);
    }

    #[test]
    fn add_multiple_vectors() {
        let idx = HnswIndex::builder(3, DistanceMetric::L2)
            .capacity(100)
            .build()
            .unwrap();
        for i in 0..50u64 {
            idx.add(i, &[i as f32, 0.0, 0.0]).unwrap();
        }
        assert_eq!(idx.size(), 50);
        for i in 0..50u64 {
            assert!(idx.contains(i));
        }
    }

    #[test]
    fn add_rejects_duplicate() {
        let idx = HnswIndex::builder(3, DistanceMetric::L2)
            .capacity(100)
            .build()
            .unwrap();
        idx.add(1, &[1.0, 2.0, 3.0]).unwrap();
        assert!(idx.add(1, &[4.0, 5.0, 6.0]).is_err());
    }

    #[test]
    fn add_rejects_wrong_dim() {
        let idx = HnswIndex::builder(3, DistanceMetric::L2)
            .capacity(100)
            .build()
            .unwrap();
        assert!(idx.add(1, &[1.0, 2.0]).is_err());
    }

    #[test]
    fn add_rejects_when_full() {
        let idx = HnswIndex::builder(2, DistanceMetric::L2)
            .capacity(2)
            .build()
            .unwrap();
        idx.add(0, &[0.0, 0.0]).unwrap();
        idx.add(1, &[1.0, 1.0]).unwrap();
        assert!(matches!(idx.add(2, &[2.0, 2.0]), Err(VaneError::IndexFull)));
    }
```

- [ ] **Step 3: Run tests**

Run: `cargo test`
Expected: All tests pass

- [ ] **Step 4: Commit**

```bash
git add vanedb/src/hnsw/mod.rs
git commit -m "feat: add HNSW add operation with graph construction"
```

---

### Task 4: Search operation + recall test

**Files:**
- Modify: `vanedb/src/hnsw/mod.rs`
- Create: `vanedb/tests/hnsw_tests.rs`

- [ ] **Step 1: Add search method to HnswIndex**

Add this method to the `impl HnswIndex` block, after `add()`:
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
        if inner.count == 0 {
            return Ok(Vec::new());
        }

        let mut curr = inner.entry_point.unwrap();
        let mut d = (self.dist_fn)(query, Self::get_vec(&inner.vectors, curr, self.dim));

        // Greedy descent through upper layers
        for l in (1..=inner.max_level).rev() {
            let lu = l as usize;
            let mut changed = true;
            while changed {
                changed = false;
                if lu < inner.neighbors[curr].len() {
                    for &n in &inner.neighbors[curr][lu] {
                        let nd =
                            (self.dist_fn)(query, Self::get_vec(&inner.vectors, n, self.dim));
                        if nd < d {
                            d = nd;
                            curr = n;
                            changed = true;
                        }
                    }
                }
            }
        }

        // Search at layer 0 with ef = max(ef_search, k)
        let ef = self.ef_search.load(Ordering::Relaxed).max(k);
        let top = Self::search_layer(
            &inner.vectors,
            self.dist_fn,
            self.dim,
            &inner.neighbors,
            query,
            curr,
            ef,
            0,
        );

        let mut results: Vec<SearchResult> = top
            .into_iter()
            .take(k)
            .map(|(dist, iid)| SearchResult::new(inner.ext_ids[iid], dist))
            .collect();
        results.sort();
        results.truncate(k);
        Ok(results)
    }
```

- [ ] **Step 2: Add unit tests for search**

Add to the `tests` module in `vanedb/src/hnsw/mod.rs`:
```rust
    #[test]
    fn search_finds_exact_match() {
        let idx = HnswIndex::builder(3, DistanceMetric::L2)
            .capacity(100)
            .seed(42)
            .build()
            .unwrap();
        idx.add(1, &[0.0, 0.0, 0.0]).unwrap();
        idx.add(2, &[10.0, 10.0, 10.0]).unwrap();
        idx.add(3, &[20.0, 20.0, 20.0]).unwrap();

        let results = idx.search(&[0.0, 0.0, 0.0], 1).unwrap();
        assert_eq!(results[0].id, 1);
        assert!(results[0].distance < 1e-6);
    }

    #[test]
    fn search_returns_k_results() {
        let idx = HnswIndex::builder(2, DistanceMetric::L2)
            .capacity(100)
            .seed(42)
            .build()
            .unwrap();
        for i in 0..20u64 {
            idx.add(i, &[i as f32, 0.0]).unwrap();
        }
        let results = idx.search(&[5.0, 0.0], 3).unwrap();
        assert_eq!(results.len(), 3);
    }

    #[test]
    fn search_empty_index() {
        let idx = HnswIndex::builder(3, DistanceMetric::L2)
            .capacity(100)
            .build()
            .unwrap();
        let results = idx.search(&[1.0, 2.0, 3.0], 5).unwrap();
        assert!(results.is_empty());
    }

    #[test]
    fn search_wrong_dimension() {
        let idx = HnswIndex::builder(3, DistanceMetric::L2)
            .capacity(100)
            .build()
            .unwrap();
        assert!(idx.search(&[1.0, 2.0], 5).is_err());
    }
```

- [ ] **Step 3: Create integration test with recall verification**

Create `vanedb/tests/hnsw_tests.rs`:
```rust
use vanedb::{DistanceMetric, HnswIndex, VectorStore};

/// Brute-force search using VectorStore as ground truth, then check HNSW recall.
#[test]
fn hnsw_recall_vs_brute_force() {
    let dim = 32;
    let n = 500;
    let k = 10;

    let hnsw = HnswIndex::builder(dim, DistanceMetric::L2)
        .capacity(n)
        .m(16)
        .ef_construction(200)
        .seed(42)
        .build()
        .unwrap();

    let brute = VectorStore::new(dim, DistanceMetric::L2).unwrap();

    // Generate deterministic vectors
    let mut vectors = Vec::new();
    for i in 0..n as u64 {
        let v: Vec<f32> = (0..dim)
            .map(|d| ((i * 31 + d as u64 * 7) % 1000) as f32 / 100.0)
            .collect();
        hnsw.add(i, &v).unwrap();
        brute.add(i, &v).unwrap();
        vectors.push(v);
    }

    hnsw.set_ef_search(100);

    // Test 10 queries
    let mut total_recall = 0.0;
    for q in 0..10 {
        let query: Vec<f32> = (0..dim)
            .map(|d| ((q * 17 + d * 13) % 1000) as f32 / 100.0)
            .collect();

        let hnsw_results = hnsw.search(&query, k).unwrap();
        let brute_results = brute.search(&query, k).unwrap();

        let brute_ids: std::collections::HashSet<u64> =
            brute_results.iter().map(|r| r.id).collect();
        let hits = hnsw_results
            .iter()
            .filter(|r| brute_ids.contains(&r.id))
            .count();

        total_recall += hits as f64 / k as f64;
    }

    let avg_recall = total_recall / 10.0;
    assert!(
        avg_recall >= 0.8,
        "HNSW recall too low: {avg_recall:.2} (expected >= 0.80)"
    );
}

#[test]
fn hnsw_cosine_search() {
    let idx = HnswIndex::builder(3, DistanceMetric::Cosine)
        .capacity(100)
        .seed(42)
        .build()
        .unwrap();

    idx.add(1, &[1.0, 0.0, 0.0]).unwrap(); // right
    idx.add(2, &[0.0, 1.0, 0.0]).unwrap(); // up
    idx.add(3, &[-1.0, 0.0, 0.0]).unwrap(); // left

    let results = idx.search(&[0.9, 0.1, 0.0], 1).unwrap();
    assert_eq!(results[0].id, 1);
}
```

- [ ] **Step 4: Run tests**

Run: `cargo test`
Expected: All tests pass, recall >= 80%

- [ ] **Step 5: Commit**

```bash
git add vanedb/src/hnsw/mod.rs vanedb/tests/hnsw_tests.rs
git commit -m "feat: add HNSW search with recall verification"
```

---

### Task 5: Persistence (save/load)

**Files:**
- Create: `vanedb/src/hnsw/persistence.rs`
- Modify: `vanedb/src/hnsw/mod.rs`

- [ ] **Step 1: Create persistence module**

Create `vanedb/src/hnsw/persistence.rs`:
```rust
use std::collections::HashMap;
use std::fs;
use std::path::Path;

use serde::{Deserialize, Serialize};

use super::{HnswIndex, Inner};
use crate::distance::{distance_fn, DistanceMetric};
use crate::error::{Result, VaneError};

use parking_lot::RwLock;
use rand::rngs::StdRng;
use rand::SeedableRng;
use std::sync::atomic::AtomicUsize;

#[derive(Serialize, Deserialize)]
struct HnswData {
    dim: usize,
    metric: u32,
    max_elements: usize,
    m: usize,
    m_max: usize,
    m_max0: usize,
    ef_construction: usize,
    ef_search: usize,
    mult: f64,
    count: usize,
    entry_point: Option<usize>,
    max_level: i32,
    vectors: Vec<f32>,
    ext_ids: Vec<u64>,
    levels: Vec<i32>,
    neighbors: Vec<Vec<Vec<usize>>>,
    id_map: HashMap<u64, usize>,
}

fn metric_to_u32(m: DistanceMetric) -> u32 {
    match m {
        DistanceMetric::L2 => 0,
        DistanceMetric::Cosine => 1,
        DistanceMetric::Dot => 2,
    }
}

fn u32_to_metric(v: u32) -> Result<DistanceMetric> {
    match v {
        0 => Ok(DistanceMetric::L2),
        1 => Ok(DistanceMetric::Cosine),
        2 => Ok(DistanceMetric::Dot),
        _ => Err(VaneError::Io("invalid metric in file".to_string())),
    }
}

impl HnswIndex {
    pub fn save(&self, path: impl AsRef<Path>) -> Result<()> {
        let inner = self.inner.read();
        let data = HnswData {
            dim: self.dim,
            metric: metric_to_u32(self.metric),
            max_elements: self.max_elements,
            m: self.m,
            m_max: self.m_max,
            m_max0: self.m_max0,
            ef_construction: self.ef_construction,
            ef_search: self.ef_search.load(std::sync::atomic::Ordering::Relaxed),
            mult: self.mult,
            count: inner.count,
            entry_point: inner.entry_point,
            max_level: inner.max_level,
            vectors: inner.vectors.clone(),
            ext_ids: inner.ext_ids.clone(),
            levels: inner.levels.clone(),
            neighbors: inner.neighbors.clone(),
            id_map: inner.id_map.clone(),
        };

        let bytes =
            bincode::serialize(&data).map_err(|e| VaneError::Io(format!("serialize: {e}")))?;

        let path = path.as_ref();
        let tmp = path.with_extension("tmp");
        fs::write(&tmp, &bytes).map_err(|e| VaneError::Io(format!("write: {e}")))?;
        fs::rename(&tmp, path).map_err(|e| VaneError::Io(format!("rename: {e}")))?;
        Ok(())
    }

    pub fn load(path: impl AsRef<Path>) -> Result<Self> {
        let bytes =
            fs::read(path.as_ref()).map_err(|e| VaneError::Io(format!("read: {e}")))?;
        let data: HnswData =
            bincode::deserialize(&bytes).map_err(|e| VaneError::Io(format!("deserialize: {e}")))?;

        let metric = u32_to_metric(data.metric)?;

        Ok(HnswIndex {
            dim: data.dim,
            metric,
            dist_fn: distance_fn(metric),
            max_elements: data.max_elements,
            m: data.m,
            m_max: data.m_max,
            m_max0: data.m_max0,
            ef_construction: data.ef_construction,
            ef_search: AtomicUsize::new(data.ef_search),
            mult: data.mult,
            inner: RwLock::new(Inner {
                vectors: data.vectors,
                ext_ids: data.ext_ids,
                id_map: data.id_map,
                levels: data.levels,
                neighbors: data.neighbors,
                entry_point: data.entry_point,
                max_level: data.max_level,
                count: data.count,
                rng: StdRng::seed_from_u64(data.count as u64),
            }),
        })
    }
}
```

- [ ] **Step 2: Update hnsw/mod.rs to include persistence module**

Add `mod persistence;` at the top of `vanedb/src/hnsw/mod.rs` (after the `use` statements, before `struct FloatOrd`):
```rust
mod persistence;
```

Also change `Inner` from private to `pub(super)` so persistence.rs can access it:
```rust
pub(super) struct Inner {
```

And change all fields of `Inner` to `pub(super)`:
```rust
pub(super) struct Inner {
    pub(super) vectors: Vec<f32>,
    pub(super) ext_ids: Vec<u64>,
    pub(super) id_map: HashMap<u64, usize>,
    pub(super) levels: Vec<i32>,
    pub(super) neighbors: Vec<Vec<Vec<usize>>>,
    pub(super) entry_point: Option<usize>,
    pub(super) max_level: i32,
    pub(super) count: usize,
    pub(super) rng: StdRng,
}
```

Do the same for HnswIndex fields — add `pub(super)` to each field so persistence.rs can access them.

- [ ] **Step 3: Add persistence tests**

Add to `vanedb/tests/hnsw_tests.rs`:
```rust
#[test]
fn hnsw_save_load_roundtrip() {
    let dim = 8;
    let path = std::env::temp_dir().join("vanedb_test_hnsw.bin");

    // Build and populate index
    let idx = HnswIndex::builder(dim, DistanceMetric::L2)
        .capacity(100)
        .seed(42)
        .build()
        .unwrap();
    for i in 0..20u64 {
        let v: Vec<f32> = (0..dim).map(|d| (i * 10 + d as u64) as f32).collect();
        idx.add(i, &v).unwrap();
    }
    idx.set_ef_search(100);

    // Save
    idx.save(&path).unwrap();

    // Load
    let loaded = HnswIndex::load(&path).unwrap();

    // Verify metadata
    assert_eq!(loaded.dimension(), dim);
    assert_eq!(loaded.size(), 20);
    assert_eq!(loaded.get_ef_search(), 100);

    // Verify vectors
    for i in 0..20u64 {
        assert_eq!(idx.get_vector(i).unwrap(), loaded.get_vector(i).unwrap());
    }

    // Verify search produces same results
    let query = vec![5.0; dim];
    let orig_results = idx.search(&query, 5).unwrap();
    let load_results = loaded.search(&query, 5).unwrap();
    assert_eq!(orig_results.len(), load_results.len());
    for (a, b) in orig_results.iter().zip(load_results.iter()) {
        assert_eq!(a.id, b.id);
    }

    // Cleanup
    let _ = std::fs::remove_file(&path);
}
```

- [ ] **Step 4: Run tests**

Run: `cargo test`
Expected: All tests pass

- [ ] **Step 5: Commit**

```bash
git add vanedb/src/hnsw/
git commit -m "feat: add HNSW save/load persistence with bincode"
```

---

### Task 6: Thread safety + final polish

**Files:**
- Modify: `vanedb/tests/hnsw_tests.rs`

- [ ] **Step 1: Add thread safety tests**

Add to `vanedb/tests/hnsw_tests.rs`:
```rust
#[test]
fn hnsw_concurrent_search() {
    use std::sync::Arc;
    use std::thread;

    let idx = Arc::new(
        HnswIndex::builder(8, DistanceMetric::L2)
            .capacity(200)
            .seed(42)
            .build()
            .unwrap(),
    );

    // Add vectors sequentially
    for i in 0..100u64 {
        let v: Vec<f32> = (0..8).map(|d| (i + d as u64) as f32).collect();
        idx.add(i, &v).unwrap();
    }

    // 10 concurrent search threads
    let mut handles = vec![];
    for t in 0..10 {
        let idx = Arc::clone(&idx);
        handles.push(thread::spawn(move || {
            let query: Vec<f32> = (0..8).map(|d| (t * 10 + d) as f32).collect();
            let results = idx.search(&query, 5).unwrap();
            assert_eq!(results.len(), 5);
        }));
    }

    for h in handles {
        h.join().unwrap();
    }
}

#[test]
fn hnsw_is_send_sync() {
    fn assert_send_sync<T: Send + Sync>() {}
    assert_send_sync::<HnswIndex>();
}
```

- [ ] **Step 2: Run full test suite + clippy + fmt**

Run: `cargo test && cargo clippy --all-targets -- -D warnings && cargo fmt --all -- --check`
Expected: All pass

- [ ] **Step 3: Commit and push**

```bash
git add vanedb/tests/hnsw_tests.rs
git commit -m "test: add HNSW thread safety and Send+Sync tests"
cargo fmt --all
git add -A && git diff --cached --name-only | grep -v target && git commit -m "style: cargo fmt" || true
git push
```

---

## Future Plans

- **Phase 5:** MmapVectorStore (`memmap2`)
- **Phase 6:** Performance benchmarks + CI regression testing (`criterion` + `critcmp`)
- **Phase 7:** GPU — Metal + CUDA
- **Phase 8:** Python bindings (PyO3 + maturin)
- **Phase 9:** WASM bindings
