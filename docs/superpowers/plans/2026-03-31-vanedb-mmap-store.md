# VaneDB MmapVectorStore — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add a memory-mapped vector store that provides zero-copy brute-force search over vectors stored in a binary file.

**Architecture:** Two structs: `MmapVectorStoreBuilder` collects vectors in memory and writes a compact binary file (header + IDs + vectors). `MmapVectorStore` memory-maps that file via `memmap2`, parses the header, and provides search directly over the mapped memory — no deserialization, no copies. Feature-gated behind `mmap`.

**Tech Stack:** `memmap2` (already in Cargo.toml as optional dep), raw binary I/O with `std::io::Write` + `to_le_bytes()`

**Spec:** `docs/superpowers/specs/2026-03-29-vanedb-rust-rewrite-design.md` — Phase 5

**C++ reference:** `/Users/anton/code/quiverdb/src/core/mmap_vector_store.h`

---

## Binary File Format

```
Offset  Size  Field
0       4     magic: u32 (0x564E4442 = "VNDB")
4       4     version: u32 (1)
8       8     dim: u64
16      8     num_vectors: u64
24      4     metric: u32 (0=L2, 1=Cosine, 2=Dot)
28      4     reserved: u32 (0)
--- HEADER: 32 bytes ---
32      num_vectors * 8    ids: [u64; num_vectors]
32 + ids_size              vectors: [f32; num_vectors * dim]
```

All values little-endian. Vectors are stored as contiguous f32 arrays.

## File Structure

| File | Responsibility |
|------|---------------|
| `vanedb/src/mmap.rs` | `MmapVectorStore` (open, search, get) + `MmapVectorStoreBuilder` (add, save) |
| `vanedb/src/lib.rs` | Add `mmap` module (cfg-gated), re-export types |
| `vanedb/tests/mmap_tests.rs` | Integration tests: build → write → open → search |

---

### Task 1: MmapVectorStoreBuilder

**Files:**
- Create: `vanedb/src/mmap.rs`
- Modify: `vanedb/src/lib.rs`

- [ ] **Step 1: Create mmap module with builder**

Create `vanedb/src/mmap.rs`:
```rust
use std::collections::HashSet;
use std::fs;
use std::io::Write;
use std::path::Path;

use crate::distance::DistanceMetric;
use crate::error::{Result, VaneError};

const MAGIC: u32 = 0x564E4442; // "VNDB"
const VERSION: u32 = 1;
const HEADER_SIZE: usize = 32;

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

pub struct MmapVectorStoreBuilder {
    dim: usize,
    metric: DistanceMetric,
    ids: Vec<u64>,
    vectors: Vec<f32>,
    id_set: HashSet<u64>,
}

impl MmapVectorStoreBuilder {
    pub fn new(dim: usize, metric: DistanceMetric) -> Result<Self> {
        if dim == 0 {
            return Err(VaneError::EmptyVector);
        }
        Ok(Self {
            dim,
            metric,
            ids: Vec::new(),
            vectors: Vec::new(),
            id_set: HashSet::new(),
        })
    }

    pub fn add(&mut self, id: u64, vector: &[f32]) -> Result<()> {
        if vector.len() != self.dim {
            return Err(VaneError::DimensionMismatch {
                expected: self.dim,
                got: vector.len(),
            });
        }
        if self.id_set.contains(&id) {
            return Err(VaneError::DuplicateId { id });
        }
        self.ids.push(id);
        self.vectors.extend_from_slice(vector);
        self.id_set.insert(id);
        Ok(())
    }

    pub fn size(&self) -> usize {
        self.ids.len()
    }

    pub fn save(&self, path: impl AsRef<Path>) -> Result<()> {
        let path = path.as_ref();
        let tmp = path.with_extension("tmp");
        let mut f =
            fs::File::create(&tmp).map_err(|e| VaneError::Io(format!("create: {e}")))?;

        // Header
        f.write_all(&MAGIC.to_le_bytes())
            .map_err(|e| VaneError::Io(format!("write: {e}")))?;
        f.write_all(&VERSION.to_le_bytes())
            .map_err(|e| VaneError::Io(format!("write: {e}")))?;
        f.write_all(&(self.dim as u64).to_le_bytes())
            .map_err(|e| VaneError::Io(format!("write: {e}")))?;
        f.write_all(&(self.ids.len() as u64).to_le_bytes())
            .map_err(|e| VaneError::Io(format!("write: {e}")))?;
        f.write_all(&metric_to_u32(self.metric).to_le_bytes())
            .map_err(|e| VaneError::Io(format!("write: {e}")))?;
        f.write_all(&0u32.to_le_bytes())
            .map_err(|e| VaneError::Io(format!("write: {e}")))?; // reserved

        // IDs
        for &id in &self.ids {
            f.write_all(&id.to_le_bytes())
                .map_err(|e| VaneError::Io(format!("write: {e}")))?;
        }

        // Vectors
        for &v in &self.vectors {
            f.write_all(&v.to_le_bytes())
                .map_err(|e| VaneError::Io(format!("write: {e}")))?;
        }

        f.flush()
            .map_err(|e| VaneError::Io(format!("flush: {e}")))?;
        drop(f);

        fs::rename(&tmp, path).map_err(|e| VaneError::Io(format!("rename: {e}")))?;
        Ok(())
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn builder_add_and_size() {
        let mut b = MmapVectorStoreBuilder::new(3, DistanceMetric::L2).unwrap();
        b.add(1, &[1.0, 2.0, 3.0]).unwrap();
        b.add(2, &[4.0, 5.0, 6.0]).unwrap();
        assert_eq!(b.size(), 2);
    }

    #[test]
    fn builder_rejects_wrong_dim() {
        let mut b = MmapVectorStoreBuilder::new(3, DistanceMetric::L2).unwrap();
        assert!(b.add(1, &[1.0, 2.0]).is_err());
    }

    #[test]
    fn builder_rejects_duplicate() {
        let mut b = MmapVectorStoreBuilder::new(3, DistanceMetric::L2).unwrap();
        b.add(1, &[1.0, 2.0, 3.0]).unwrap();
        assert!(b.add(1, &[4.0, 5.0, 6.0]).is_err());
    }

    #[test]
    fn builder_rejects_zero_dim() {
        assert!(MmapVectorStoreBuilder::new(0, DistanceMetric::L2).is_err());
    }

    #[test]
    fn builder_save_creates_file() {
        let path = std::env::temp_dir().join("vanedb_test_mmap_builder.bin");
        let mut b = MmapVectorStoreBuilder::new(2, DistanceMetric::L2).unwrap();
        b.add(1, &[1.0, 2.0]).unwrap();
        b.save(&path).unwrap();
        assert!(path.exists());
        let meta = std::fs::metadata(&path).unwrap();
        // header(32) + 1 id(8) + 2 floats(8) = 48
        assert_eq!(meta.len(), 48);
        let _ = std::fs::remove_file(&path);
    }
}
```

- [ ] **Step 2: Update lib.rs with cfg-gated module**

Replace `vanedb/src/lib.rs`:
```rust
pub mod distance;
pub mod error;
pub mod hnsw;
#[cfg(feature = "mmap")]
pub mod mmap;
pub mod store;

pub use distance::DistanceMetric;
pub use error::{Result, VaneError};
pub use hnsw::HnswIndex;
#[cfg(feature = "mmap")]
pub use mmap::{MmapVectorStore, MmapVectorStoreBuilder};
pub use store::{SearchResult, VectorStore};
```

NOTE: `MmapVectorStore` doesn't exist yet — it will be added in Task 2. For now, comment out the `pub use mmap::MmapVectorStore` line, or add a placeholder in mmap.rs. The simplest approach: just add a placeholder struct to mmap.rs so the re-export compiles:

Add at the bottom of `vanedb/src/mmap.rs` (before `#[cfg(test)]`):
```rust
/// Memory-mapped vector store for zero-copy search.
pub struct MmapVectorStore {
    _private: (), // placeholder — implemented in Task 2
}
```

- [ ] **Step 3: Run tests with mmap feature**

Run: `cargo test --features mmap`
Expected: All tests pass including 5 new builder tests

- [ ] **Step 4: Commit**

```bash
cargo fmt --all
git add vanedb/src/mmap.rs vanedb/src/lib.rs
git commit -m "feat: add MmapVectorStoreBuilder with binary file writer"
```

---

### Task 2: MmapVectorStore (open + search)

**Files:**
- Modify: `vanedb/src/mmap.rs`

- [ ] **Step 1: Replace the placeholder MmapVectorStore with the real implementation**

Replace the placeholder `MmapVectorStore` struct in `vanedb/src/mmap.rs` with:

```rust
use std::collections::HashMap;
use memmap2::Mmap;
use crate::distance::{distance_fn, DistanceFn};
use crate::store::SearchResult;

pub struct MmapVectorStore {
    mmap: Mmap,
    dim: usize,
    num_vectors: usize,
    metric: DistanceMetric,
    dist_fn: DistanceFn,
    ids_offset: usize,
    vectors_offset: usize,
    id_map: HashMap<u64, usize>,
}

impl MmapVectorStore {
    pub fn open(path: impl AsRef<Path>) -> Result<Self> {
        let file =
            fs::File::open(path.as_ref()).map_err(|e| VaneError::Io(format!("open: {e}")))?;
        let mmap = unsafe { Mmap::map(&file) }
            .map_err(|e| VaneError::Io(format!("mmap: {e}")))?;

        if mmap.len() < HEADER_SIZE {
            return Err(VaneError::Io("file too small".to_string()));
        }

        let magic = u32::from_le_bytes(mmap[0..4].try_into().unwrap());
        if magic != MAGIC {
            return Err(VaneError::Io("invalid magic".to_string()));
        }
        let version = u32::from_le_bytes(mmap[4..8].try_into().unwrap());
        if version != VERSION {
            return Err(VaneError::Io(format!("unsupported version: {version}")));
        }

        let dim = u64::from_le_bytes(mmap[8..16].try_into().unwrap()) as usize;
        let num_vectors = u64::from_le_bytes(mmap[16..24].try_into().unwrap()) as usize;
        let metric_raw = u32::from_le_bytes(mmap[24..28].try_into().unwrap());
        let metric = u32_to_metric(metric_raw)?;

        if dim == 0 && num_vectors > 0 {
            return Err(VaneError::Io("zero dimension with vectors".to_string()));
        }

        let ids_size = num_vectors
            .checked_mul(8)
            .ok_or_else(|| VaneError::Io("size overflow".to_string()))?;
        let vecs_size = num_vectors
            .checked_mul(dim)
            .and_then(|n| n.checked_mul(4))
            .ok_or_else(|| VaneError::Io("size overflow".to_string()))?;
        let expected = HEADER_SIZE
            .checked_add(ids_size)
            .and_then(|n| n.checked_add(vecs_size))
            .ok_or_else(|| VaneError::Io("size overflow".to_string()))?;

        if mmap.len() < expected {
            return Err(VaneError::Io("file truncated".to_string()));
        }

        let ids_offset = HEADER_SIZE;
        let vectors_offset = HEADER_SIZE + ids_size;

        // Build ID → index map
        let mut id_map = HashMap::with_capacity(num_vectors);
        for i in 0..num_vectors {
            let off = ids_offset + i * 8;
            let id = u64::from_le_bytes(mmap[off..off + 8].try_into().unwrap());
            id_map.insert(id, i);
        }

        Ok(Self {
            mmap,
            dim,
            num_vectors,
            metric,
            dist_fn: distance_fn(metric),
            ids_offset,
            vectors_offset,
            id_map,
        })
    }

    pub fn size(&self) -> usize {
        self.num_vectors
    }

    pub fn dimension(&self) -> usize {
        self.dim
    }

    pub fn metric(&self) -> DistanceMetric {
        self.metric
    }

    pub fn contains(&self, id: u64) -> bool {
        self.id_map.contains_key(&id)
    }

    /// Get a vector by ID. Returns a slice into the memory-mapped file (zero-copy).
    pub fn get(&self, id: u64) -> Result<&[f32]> {
        let &idx = self.id_map.get(&id).ok_or(VaneError::NotFound { id })?;
        Ok(self.get_vec(idx))
    }

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

        let mut results: Vec<SearchResult> = (0..self.num_vectors)
            .map(|i| {
                let id = self.get_id(i);
                let vec = self.get_vec(i);
                SearchResult::new(id, (self.dist_fn)(query, vec))
            })
            .collect();

        results.sort();
        results.truncate(k);
        Ok(results)
    }

    fn get_id(&self, idx: usize) -> u64 {
        let off = self.ids_offset + idx * 8;
        u64::from_le_bytes(self.mmap[off..off + 8].try_into().unwrap())
    }

    /// Zero-copy vector access: reinterprets mmap'd bytes as f32 slice.
    fn get_vec(&self, idx: usize) -> &[f32] {
        let off = self.vectors_offset + idx * self.dim * 4;
        let bytes = &self.mmap[off..off + self.dim * 4];
        // SAFETY: f32 has no alignment requirement stricter than 1 on mmap'd memory,
        // and the data was written as little-endian f32s on a little-endian system.
        // memmap2 guarantees the mapping is valid for the file's lifetime.
        unsafe { std::slice::from_raw_parts(bytes.as_ptr() as *const f32, self.dim) }
    }
}
```

Make sure these imports are at the top of the file (add any that are missing):
```rust
use std::collections::{HashMap, HashSet};
use std::fs;
use std::io::Write;
use std::path::Path;

use memmap2::Mmap;

use crate::distance::{distance_fn, DistanceFn, DistanceMetric};
use crate::error::{Result, VaneError};
use crate::store::SearchResult;
```

- [ ] **Step 2: Add unit tests for MmapVectorStore**

Add to the `tests` module in `vanedb/src/mmap.rs`:
```rust
    #[test]
    fn roundtrip_build_open_search() {
        let path = std::env::temp_dir().join("vanedb_test_mmap_roundtrip.bin");

        // Build
        let mut b = MmapVectorStoreBuilder::new(3, DistanceMetric::L2).unwrap();
        b.add(10, &[0.0, 0.0, 0.0]).unwrap();
        b.add(20, &[1.0, 0.0, 0.0]).unwrap();
        b.add(30, &[10.0, 10.0, 10.0]).unwrap();
        b.save(&path).unwrap();

        // Open
        let store = MmapVectorStore::open(&path).unwrap();
        assert_eq!(store.size(), 3);
        assert_eq!(store.dimension(), 3);
        assert!(store.contains(10));
        assert!(!store.contains(99));

        // Get (zero-copy)
        assert_eq!(store.get(10).unwrap(), &[0.0, 0.0, 0.0]);
        assert_eq!(store.get(20).unwrap(), &[1.0, 0.0, 0.0]);

        // Search
        let results = store.search(&[0.0, 0.1, 0.0], 2).unwrap();
        assert_eq!(results.len(), 2);
        assert_eq!(results[0].id, 10); // closest
        assert_eq!(results[1].id, 20); // second

        let _ = std::fs::remove_file(&path);
    }

    #[test]
    fn open_rejects_bad_file() {
        let path = std::env::temp_dir().join("vanedb_test_mmap_bad.bin");
        std::fs::write(&path, b"garbage").unwrap();
        assert!(MmapVectorStore::open(&path).is_err());
        let _ = std::fs::remove_file(&path);
    }

    #[test]
    fn open_rejects_truncated_file() {
        let path = std::env::temp_dir().join("vanedb_test_mmap_trunc.bin");
        // Valid header but claims 1000 vectors with no data
        let mut data = Vec::new();
        data.extend_from_slice(&MAGIC.to_le_bytes());
        data.extend_from_slice(&VERSION.to_le_bytes());
        data.extend_from_slice(&(3u64).to_le_bytes()); // dim
        data.extend_from_slice(&(1000u64).to_le_bytes()); // num_vectors
        data.extend_from_slice(&(0u32).to_le_bytes()); // metric
        data.extend_from_slice(&(0u32).to_le_bytes()); // reserved
        std::fs::write(&path, &data).unwrap();
        assert!(MmapVectorStore::open(&path).is_err());
        let _ = std::fs::remove_file(&path);
    }

    #[test]
    fn search_wrong_dimension() {
        let path = std::env::temp_dir().join("vanedb_test_mmap_dim.bin");
        let mut b = MmapVectorStoreBuilder::new(3, DistanceMetric::L2).unwrap();
        b.add(1, &[1.0, 2.0, 3.0]).unwrap();
        b.save(&path).unwrap();

        let store = MmapVectorStore::open(&path).unwrap();
        assert!(store.search(&[1.0, 2.0], 1).is_err());
        let _ = std::fs::remove_file(&path);
    }
```

- [ ] **Step 3: Run tests**

Run: `cargo test --features mmap`
Expected: All tests pass

- [ ] **Step 4: Commit**

```bash
cargo fmt --all
git add vanedb/src/mmap.rs
git commit -m "feat: add MmapVectorStore with zero-copy search"
```

---

### Task 3: Integration tests + thread safety

**Files:**
- Create: `vanedb/tests/mmap_tests.rs`

- [ ] **Step 1: Create integration tests**

Create `vanedb/tests/mmap_tests.rs`:
```rust
#![cfg(feature = "mmap")]

use vanedb::{DistanceMetric, MmapVectorStore, MmapVectorStoreBuilder, VectorStore};

#[test]
fn mmap_matches_brute_force() {
    let dim = 16;
    let path = std::env::temp_dir().join("vanedb_test_mmap_vs_brute.bin");

    // Build mmap file
    let mut builder = MmapVectorStoreBuilder::new(dim, DistanceMetric::L2).unwrap();
    let brute = VectorStore::new(dim, DistanceMetric::L2).unwrap();

    for i in 0..100u64 {
        let v: Vec<f32> = (0..dim)
            .map(|d| ((i * 31 + d as u64 * 7) % 1000) as f32 / 100.0)
            .collect();
        builder.add(i, &v).unwrap();
        brute.add(i, &v).unwrap();
    }
    builder.save(&path).unwrap();

    let mmap = MmapVectorStore::open(&path).unwrap();
    assert_eq!(mmap.size(), 100);

    // 5 queries — results must match exactly (both are brute force)
    for q in 0..5u64 {
        let query: Vec<f32> = (0..dim)
            .map(|d| ((q * 17 + d as u64 * 13) % 1000) as f32 / 100.0)
            .collect();

        let mmap_results = mmap.search(&query, 5).unwrap();
        let brute_results = brute.search(&query, 5).unwrap();

        assert_eq!(mmap_results.len(), brute_results.len());
        for (a, b) in mmap_results.iter().zip(brute_results.iter()) {
            assert_eq!(a.id, b.id, "query {q}: mmap vs brute mismatch");
        }
    }

    let _ = std::fs::remove_file(&path);
}

#[test]
fn mmap_cosine_search() {
    let path = std::env::temp_dir().join("vanedb_test_mmap_cosine.bin");
    let mut builder = MmapVectorStoreBuilder::new(3, DistanceMetric::Cosine).unwrap();
    builder.add(1, &[1.0, 0.0, 0.0]).unwrap();
    builder.add(2, &[0.0, 1.0, 0.0]).unwrap();
    builder.add(3, &[-1.0, 0.0, 0.0]).unwrap();
    builder.save(&path).unwrap();

    let store = MmapVectorStore::open(&path).unwrap();
    let results = store.search(&[0.9, 0.1, 0.0], 1).unwrap();
    assert_eq!(results[0].id, 1);

    let _ = std::fs::remove_file(&path);
}

#[test]
fn mmap_concurrent_search() {
    use std::sync::Arc;
    use std::thread;

    let dim = 8;
    let path = std::env::temp_dir().join("vanedb_test_mmap_concurrent.bin");

    let mut builder = MmapVectorStoreBuilder::new(dim, DistanceMetric::L2).unwrap();
    for i in 0..50u64 {
        let v: Vec<f32> = (0..dim).map(|d| (i + d as u64) as f32).collect();
        builder.add(i, &v).unwrap();
    }
    builder.save(&path).unwrap();

    let store = Arc::new(MmapVectorStore::open(&path).unwrap());

    let mut handles = vec![];
    for t in 0..10u64 {
        let store = Arc::clone(&store);
        handles.push(thread::spawn(move || {
            let query: Vec<f32> = (0..dim).map(|d| (t * 5 + d as u64) as f32).collect();
            let results = store.search(&query, 3).unwrap();
            assert_eq!(results.len(), 3);
        }));
    }
    for h in handles {
        h.join().unwrap();
    }

    let _ = std::fs::remove_file(&path);
}

#[test]
fn mmap_is_send_sync() {
    fn assert_send_sync<T: Send + Sync>() {}
    assert_send_sync::<MmapVectorStore>();
}
```

- [ ] **Step 2: Run full test suite + clippy**

Run: `cargo test --features mmap && cargo clippy --all-targets --all-features -- -D warnings && cargo fmt --all -- --check`
Expected: All pass

- [ ] **Step 3: Commit and push**

```bash
cargo fmt --all
git add vanedb/tests/mmap_tests.rs
git commit -m "test: add MmapVectorStore integration tests with thread safety"
git push
```

---

## Future Plans

- **Phase 6:** Performance benchmarks + CI regression testing
- **Phase 7:** GPU — Metal + CUDA
- **Phase 8:** Python bindings (PyO3 + maturin)
- **Phase 9:** WASM bindings
