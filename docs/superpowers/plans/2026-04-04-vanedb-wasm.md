# VaneDB WASM Bindings — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Create WebAssembly bindings for VaneDB so it can run in browsers and Node.js via `npm install vanedb`.

**Architecture:** Separate `vanedb-wasm` crate in the workspace, using `wasm-bindgen` to expose VectorStore and HnswIndex to JavaScript. Built with `wasm-pack` for npm package generation. SIMD dispatch falls back to scalar automatically (no NEON/AVX2 in WASM). The `wasm32-unknown-unknown` target doesn't support file I/O, so HnswIndex save/load is excluded.

**Tech Stack:** `wasm-bindgen 0.2` (JS FFI), `wasm-pack` (build tool), `js-sys` (JS types), `serde-wasm-bindgen` (type conversion)

**Spec:** `docs/superpowers/specs/2026-03-29-vanedb-rust-rewrite-design.md` — Phase 9

---

## File Structure

| File | Responsibility |
|------|---------------|
| `Cargo.toml` (workspace root) | Add `vanedb-wasm` to members |
| `vanedb-wasm/Cargo.toml` | wasm-bindgen crate config, cdylib target |
| `vanedb-wasm/src/lib.rs` | `#[wasm_bindgen]` wrappers for VectorStore + HnswIndex |
| `vanedb-wasm/tests/web.rs` | wasm-bindgen-test suite (runs in headless browser) |

---

### Task 1: Crate setup + wasm-pack

**Files:**
- Modify: `Cargo.toml` (workspace root)
- Create: `vanedb-wasm/Cargo.toml`, `vanedb-wasm/src/lib.rs`

- [ ] **Step 1: Install wasm-pack**

```bash
cargo install wasm-pack
```

- [ ] **Step 2: Add vanedb-wasm to workspace**

Update workspace root `Cargo.toml`:
```toml
[workspace]
members = ["vanedb", "vanedb-py", "vanedb-wasm"]
resolver = "2"
```

- [ ] **Step 3: Create vanedb-wasm/Cargo.toml**

```toml
[package]
name = "vanedb-wasm"
version = "0.1.0"
edition = "2021"
publish = false

[lib]
crate-type = ["cdylib"]

[dependencies]
wasm-bindgen = "0.2"
js-sys = "0.3"
vanedb = { path = "../vanedb" }

[dev-dependencies]
wasm-bindgen-test = "0.3"
```

- [ ] **Step 4: Create minimal lib.rs**

Create `vanedb-wasm/src/lib.rs`:
```rust
use wasm_bindgen::prelude::*;

#[wasm_bindgen]
pub fn version() -> String {
    "0.1.0".to_string()
}
```

- [ ] **Step 5: Build and verify**

```bash
cd /Users/anton/code/vanedb/vanedb-wasm
wasm-pack build --target web
```

Expected: Builds successfully, creates `pkg/` directory with `.wasm` + `.js` files

- [ ] **Step 6: Commit**

```bash
cd /Users/anton/code/vanedb
cargo fmt --all
git add Cargo.toml vanedb-wasm/
git commit -m "feat: add vanedb-wasm crate with wasm-pack setup"
```

---

### Task 2: VectorStore + HnswIndex WASM wrappers

**Files:**
- Modify: `vanedb-wasm/src/lib.rs`

- [ ] **Step 1: Implement WASM wrappers**

Replace `vanedb-wasm/src/lib.rs` with:
```rust
use wasm_bindgen::prelude::*;

use vanedb::distance::DistanceMetric;
use vanedb::hnsw::HnswIndex;
use vanedb::store::VectorStore;

fn to_jserr(e: vanedb::VaneError) -> JsError {
    JsError::new(&e.to_string())
}

fn parse_metric(metric: &str) -> Result<DistanceMetric, JsError> {
    match metric {
        "l2" | "L2" => Ok(DistanceMetric::L2),
        "cosine" | "Cosine" => Ok(DistanceMetric::Cosine),
        "dot" | "Dot" => Ok(DistanceMetric::Dot),
        _ => Err(JsError::new(&format!("unknown metric: {metric}. Use 'l2', 'cosine', or 'dot'"))),
    }
}

#[wasm_bindgen]
pub fn version() -> String {
    "0.1.0".to_string()
}

/// Brute-force vector store for the browser.
#[wasm_bindgen]
pub struct WasmVectorStore {
    inner: VectorStore,
}

#[wasm_bindgen]
impl WasmVectorStore {
    /// Create a new VectorStore.
    /// metric: "l2", "cosine", or "dot"
    #[wasm_bindgen(constructor)]
    pub fn new(dim: usize, metric: &str) -> Result<WasmVectorStore, JsError> {
        let m = parse_metric(metric)?;
        let inner = VectorStore::new(dim, m).map_err(to_jserr)?;
        Ok(Self { inner })
    }

    pub fn add(&self, id: u64, vector: &[f32]) -> Result<(), JsError> {
        self.inner.add(id, vector).map_err(to_jserr)
    }

    /// Search for k nearest neighbors. Returns flat array: [id0, dist0, id1, dist1, ...].
    pub fn search(&self, query: &[f32], k: usize) -> Result<Vec<f32>, JsError> {
        let results = self.inner.search(query, k).map_err(to_jserr)?;
        let mut flat = Vec::with_capacity(results.len() * 2);
        for r in results {
            flat.push(r.id as f32);
            flat.push(r.distance);
        }
        Ok(flat)
    }

    pub fn get(&self, id: u64) -> Result<Vec<f32>, JsError> {
        self.inner.get(id).map_err(to_jserr)
    }

    pub fn remove(&self, id: u64) -> Result<(), JsError> {
        self.inner.remove(id).map_err(to_jserr)
    }

    pub fn contains(&self, id: u64) -> bool {
        self.inner.contains(id)
    }

    pub fn size(&self) -> usize {
        self.inner.len()
    }

    pub fn dimension(&self) -> usize {
        self.inner.dimension()
    }
}

/// HNSW approximate nearest-neighbor index for the browser.
#[wasm_bindgen]
pub struct WasmHnswIndex {
    inner: HnswIndex,
}

#[wasm_bindgen]
impl WasmHnswIndex {
    /// Create a new HnswIndex.
    /// metric: "l2", "cosine", or "dot"
    #[wasm_bindgen(constructor)]
    pub fn new(
        dim: usize,
        metric: &str,
        capacity: usize,
        m: usize,
        ef_construction: usize,
    ) -> Result<WasmHnswIndex, JsError> {
        let met = parse_metric(metric)?;
        let inner = HnswIndex::builder(dim, met)
            .capacity(capacity)
            .m(m)
            .ef_construction(ef_construction)
            .seed(42)
            .build()
            .map_err(to_jserr)?;
        Ok(Self { inner })
    }

    pub fn add(&self, id: u64, vector: &[f32]) -> Result<(), JsError> {
        self.inner.add(id, vector).map_err(to_jserr)
    }

    /// Search for k nearest neighbors. Returns flat array: [id0, dist0, id1, dist1, ...].
    pub fn search(&self, query: &[f32], k: usize) -> Result<Vec<f32>, JsError> {
        let results = self.inner.search(query, k).map_err(to_jserr)?;
        let mut flat = Vec::with_capacity(results.len() * 2);
        for r in results {
            flat.push(r.id as f32);
            flat.push(r.distance);
        }
        Ok(flat)
    }

    pub fn contains(&self, id: u64) -> bool {
        self.inner.contains(id)
    }

    pub fn size(&self) -> usize {
        self.inner.size()
    }

    pub fn dimension(&self) -> usize {
        self.inner.dimension()
    }

    #[wasm_bindgen(getter)]
    pub fn ef_search(&self) -> usize {
        self.inner.get_ef_search()
    }

    #[wasm_bindgen(setter)]
    pub fn set_ef_search(&self, ef: usize) {
        self.inner.set_ef_search(ef);
    }
}
```

- [ ] **Step 2: Build**

```bash
cd /Users/anton/code/vanedb/vanedb-wasm
wasm-pack build --target web
```

Expected: Builds successfully

- [ ] **Step 3: Commit**

```bash
cd /Users/anton/code/vanedb
cargo fmt --all
git add vanedb-wasm/src/lib.rs
git commit -m "feat: add WASM VectorStore and HnswIndex bindings"
```

---

### Task 3: WASM tests + CI + push

**Files:**
- Create: `vanedb-wasm/tests/web.rs`
- Modify: `.github/workflows/ci.yml`

- [ ] **Step 1: Create wasm-bindgen tests**

Create `vanedb-wasm/tests/web.rs`:
```rust
use wasm_bindgen_test::*;
use vanedb_wasm::*;

wasm_bindgen_test_configure!(run_in_browser);

#[wasm_bindgen_test]
fn test_version() {
    assert_eq!(version(), "0.1.0");
}

#[wasm_bindgen_test]
fn test_vector_store_basic() {
    let store = WasmVectorStore::new(3, "l2").unwrap();
    store.add(1, &[1.0, 0.0, 0.0]).unwrap();
    store.add(2, &[0.0, 1.0, 0.0]).unwrap();
    assert_eq!(store.size(), 2);
    assert_eq!(store.dimension(), 3);
    assert!(store.contains(1));
    assert!(!store.contains(99));
}

#[wasm_bindgen_test]
fn test_vector_store_search() {
    let store = WasmVectorStore::new(2, "l2").unwrap();
    store.add(1, &[0.0, 0.0]).unwrap();
    store.add(2, &[1.0, 0.0]).unwrap();
    store.add(3, &[10.0, 10.0]).unwrap();

    let flat = store.search(&[0.0, 0.1], 2).unwrap();
    // flat = [id0, dist0, id1, dist1]
    assert_eq!(flat.len(), 4);
    assert_eq!(flat[0] as u64, 1); // closest
}

#[wasm_bindgen_test]
fn test_hnsw_basic() {
    let idx = WasmHnswIndex::new(3, "l2", 100, 16, 200).unwrap();
    idx.add(1, &[1.0, 0.0, 0.0]).unwrap();
    idx.add(2, &[0.0, 1.0, 0.0]).unwrap();
    assert_eq!(idx.size(), 2);
    assert!(idx.contains(1));
}

#[wasm_bindgen_test]
fn test_hnsw_search() {
    let idx = WasmHnswIndex::new(3, "l2", 100, 16, 200).unwrap();
    idx.add(1, &[0.0, 0.0, 0.0]).unwrap();
    idx.add(2, &[10.0, 10.0, 10.0]).unwrap();

    let flat = idx.search(&[0.0, 0.0, 0.0], 1).unwrap();
    assert_eq!(flat[0] as u64, 1); // closest
}

#[wasm_bindgen_test]
fn test_cosine_metric() {
    let store = WasmVectorStore::new(2, "cosine").unwrap();
    store.add(1, &[1.0, 0.0]).unwrap();
    store.add(2, &[0.0, 1.0]).unwrap();
    let flat = store.search(&[0.9, 0.1], 1).unwrap();
    assert_eq!(flat[0] as u64, 1);
}

#[wasm_bindgen_test]
fn test_invalid_metric() {
    let result = WasmVectorStore::new(3, "invalid");
    assert!(result.is_err());
}
```

- [ ] **Step 2: Run WASM tests**

```bash
cd /Users/anton/code/vanedb/vanedb-wasm
wasm-pack test --node
```

Expected: All 7 tests pass

- [ ] **Step 3: Add WASM smoke test to CI**

Add a new job to `.github/workflows/ci.yml`:
```yaml
  test-wasm:
    name: Test (WASM)
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      - uses: dtolnay/rust-toolchain@stable
        with:
          targets: wasm32-unknown-unknown
      - name: Install wasm-pack
        run: curl https://rustwasm.github.io/wasm-pack/installer/init.sh -sSf | sh
      - name: Test WASM
        run: cd vanedb-wasm && wasm-pack test --node
```

- [ ] **Step 4: Commit and push**

```bash
cd /Users/anton/code/vanedb
cargo fmt --all
git add vanedb-wasm/tests/ .github/workflows/ci.yml
git commit -m "test: add WASM tests and CI job"
git push
```

---

## Notes

- **No save/load in WASM:** The `wasm32-unknown-unknown` target has no filesystem. HnswIndex.save/load is excluded from the WASM API. For persistence in the browser, users would serialize to/from `ArrayBuffer` (future enhancement).
- **Search returns flat array:** wasm-bindgen can't return `Vec<(u64, f32)>` directly. Instead, `search()` returns `Vec<f32>` as `[id0, dist0, id1, dist1, ...]`. JavaScript consumers parse this:
  ```js
  const flat = index.search(query, 10);
  const results = [];
  for (let i = 0; i < flat.length; i += 2) {
    results.push({ id: flat[i], distance: flat[i+1] });
  }
  ```
- **Scalar fallback:** WASM uses the scalar distance implementations (no NEON/AVX2). This is automatic — the `#[cfg(target_arch)]` dispatch in `distance/mod.rs` falls through to scalar for `wasm32`.
- **`--target web` vs `--target nodejs`:** `wasm-pack build --target web` generates ES module output for browsers. `wasm-pack test --node` tests in Node.js. Both work from the same crate.
