# VaneDB Python Bindings — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Create Python bindings for VaneDB via PyO3 so users can `pip install vanedb` and use VectorStore + HnswIndex from Python.

**Architecture:** Separate `vanedb-py` crate in the workspace, using PyO3 to wrap the core `vanedb` crate. Each Rust type gets a `Py*` wrapper: `PyVectorStore`, `PyHnswIndex`. Search results are returned as lists of `(id, distance)` tuples. Vectors are accepted as Python lists or numpy arrays. Built with `maturin` for wheel generation.

**Tech Stack:** `pyo3 0.23` (Python FFI), `maturin` (build tool), `numpy` (optional, for array input)

**Spec:** `docs/superpowers/specs/2026-03-29-vanedb-rust-rewrite-design.md` — Phase 8

---

## File Structure

| File | Responsibility |
|------|---------------|
| `Cargo.toml` (workspace root) | Add `vanedb-py` to members |
| `vanedb-py/Cargo.toml` | PyO3 crate config, cdylib target |
| `vanedb-py/pyproject.toml` | maturin build config, package metadata |
| `vanedb-py/src/lib.rs` | `#[pymodule]`, all Python-facing types |
| `vanedb-py/tests/test_vanedb.py` | Python pytest suite |

---

### Task 1: Crate setup + maturin config

**Files:**
- Modify: `Cargo.toml` (workspace root)
- Create: `vanedb-py/Cargo.toml`, `vanedb-py/pyproject.toml`, `vanedb-py/src/lib.rs`

- [ ] **Step 1: Install maturin**

```bash
pip install maturin
```

- [ ] **Step 2: Add vanedb-py to workspace**

Update workspace root `Cargo.toml`:
```toml
[workspace]
members = ["vanedb", "vanedb-py"]
resolver = "2"
```

- [ ] **Step 3: Create vanedb-py/Cargo.toml**

Create `vanedb-py/Cargo.toml`:
```toml
[package]
name = "vanedb-py"
version = "0.1.0"
edition = "2021"
publish = false

[lib]
name = "vanedb"
crate-type = ["cdylib"]

[dependencies]
pyo3 = { version = "0.23", features = ["extension-module"] }
vanedb = { path = "../vanedb" }
```

Note: The `[lib] name = "vanedb"` makes the Python module importable as `import vanedb`, not `import vanedb_py`.

- [ ] **Step 4: Create vanedb-py/pyproject.toml**

Create `vanedb-py/pyproject.toml`:
```toml
[build-system]
requires = ["maturin>=1.0,<2.0"]
build-backend = "maturin"

[project]
name = "vanedb"
version = "0.1.0"
description = "Embeddable vector database for edge AI"
requires-python = ">=3.9"
license = "MIT"
keywords = ["vector", "database", "embeddings", "similarity-search"]

[tool.maturin]
features = ["pyo3/extension-module"]
```

- [ ] **Step 5: Create minimal lib.rs**

Create `vanedb-py/src/lib.rs`:
```rust
use pyo3::prelude::*;

#[pymodule]
fn vanedb(m: &Bound<'_, PyModule>) -> PyResult<()> {
    m.add("__version__", "0.1.0")?;
    Ok(())
}
```

- [ ] **Step 6: Build and verify**

```bash
cd /Users/anton/code/vanedb/vanedb-py
maturin develop
python3 -c "import vanedb; print(vanedb.__version__)"
```

Expected output: `0.1.0`

- [ ] **Step 7: Commit**

```bash
cd /Users/anton/code/vanedb
cargo fmt --all
git add Cargo.toml vanedb-py/
git commit -m "feat: add vanedb-py crate with maturin setup"
```

---

### Task 2: VectorStore Python wrapper

**Files:**
- Modify: `vanedb-py/src/lib.rs`

- [ ] **Step 1: Add VectorStore and DistanceMetric wrappers**

Replace `vanedb-py/src/lib.rs` with:
```rust
use pyo3::exceptions::PyValueError;
use pyo3::prelude::*;

use vanedb::distance::DistanceMetric;
use vanedb::store::{SearchResult, VectorStore};
use vanedb::VaneError;

fn to_pyerr(e: VaneError) -> PyErr {
    PyValueError::new_err(e.to_string())
}

/// Distance metric enum.
#[pyclass(eq, eq_int)]
#[derive(Clone, Copy, PartialEq)]
enum PyDistanceMetric {
    L2 = 0,
    Cosine = 1,
    Dot = 2,
}

impl From<PyDistanceMetric> for DistanceMetric {
    fn from(m: PyDistanceMetric) -> Self {
        match m {
            PyDistanceMetric::L2 => DistanceMetric::L2,
            PyDistanceMetric::Cosine => DistanceMetric::Cosine,
            PyDistanceMetric::Dot => DistanceMetric::Dot,
        }
    }
}

/// Brute-force vector store with thread-safe k-NN search.
#[pyclass]
struct PyVectorStore {
    inner: VectorStore,
}

#[pymethods]
impl PyVectorStore {
    #[new]
    #[pyo3(signature = (dim, metric=PyDistanceMetric::L2))]
    fn new(dim: usize, metric: PyDistanceMetric) -> PyResult<Self> {
        let inner = VectorStore::new(dim, metric.into()).map_err(to_pyerr)?;
        Ok(Self { inner })
    }

    fn add(&self, id: u64, vector: Vec<f32>) -> PyResult<()> {
        self.inner.add(id, &vector).map_err(to_pyerr)
    }

    fn search(&self, query: Vec<f32>, k: usize) -> PyResult<Vec<(u64, f32)>> {
        let results = self.inner.search(&query, k).map_err(to_pyerr)?;
        Ok(results.into_iter().map(|r| (r.id, r.distance)).collect())
    }

    fn get(&self, id: u64) -> PyResult<Vec<f32>> {
        self.inner.get(id).map_err(to_pyerr)
    }

    fn remove(&self, id: u64) -> PyResult<()> {
        self.inner.remove(id).map_err(to_pyerr)
    }

    fn contains(&self, id: u64) -> bool {
        self.inner.contains(id)
    }

    fn __len__(&self) -> usize {
        self.inner.len()
    }

    #[getter]
    fn dimension(&self) -> usize {
        self.inner.dimension()
    }
}

#[pymodule]
fn vanedb(m: &Bound<'_, PyModule>) -> PyResult<()> {
    m.add("__version__", "0.1.0")?;
    m.add_class::<PyDistanceMetric>()?;
    m.add_class::<PyVectorStore>()?;
    Ok(())
}
```

- [ ] **Step 2: Build and test from Python**

```bash
cd /Users/anton/code/vanedb/vanedb-py
maturin develop
python3 -c "
import vanedb

store = vanedb.PyVectorStore(3)
store.add(1, [1.0, 0.0, 0.0])
store.add(2, [0.0, 1.0, 0.0])
store.add(3, [0.0, 0.0, 1.0])
print(f'Size: {len(store)}')
print(f'Dim: {store.dimension}')
results = store.search([0.9, 0.1, 0.0], 2)
print(f'Search results: {results}')
assert results[0][0] == 1, f'Expected id 1, got {results[0][0]}'
print('OK')
"
```

Expected: `Size: 3`, `Dim: 3`, search returns id 1 first, prints `OK`

- [ ] **Step 3: Commit**

```bash
cd /Users/anton/code/vanedb
cargo fmt --all
git add vanedb-py/src/lib.rs
git commit -m "feat: add Python VectorStore bindings via PyO3"
```

---

### Task 3: HnswIndex Python wrapper

**Files:**
- Modify: `vanedb-py/src/lib.rs`

- [ ] **Step 1: Add HnswIndex wrapper**

Add after the `PyVectorStore` impl block, before the `#[pymodule]` function:
```rust
use vanedb::hnsw::HnswIndex;

/// HNSW approximate nearest-neighbor index.
#[pyclass]
struct PyHnswIndex {
    inner: HnswIndex,
}

#[pymethods]
impl PyHnswIndex {
    #[new]
    #[pyo3(signature = (dim, metric=PyDistanceMetric::L2, capacity=100000, m=16, ef_construction=200, seed=42))]
    fn new(
        dim: usize,
        metric: PyDistanceMetric,
        capacity: usize,
        m: usize,
        ef_construction: usize,
        seed: u64,
    ) -> PyResult<Self> {
        let inner = HnswIndex::builder(dim, metric.into())
            .capacity(capacity)
            .m(m)
            .ef_construction(ef_construction)
            .seed(seed)
            .build()
            .map_err(to_pyerr)?;
        Ok(Self { inner })
    }

    fn add(&self, id: u64, vector: Vec<f32>) -> PyResult<()> {
        self.inner.add(id, &vector).map_err(to_pyerr)
    }

    fn search(&self, query: Vec<f32>, k: usize) -> PyResult<Vec<(u64, f32)>> {
        let results = self.inner.search(&query, k).map_err(to_pyerr)?;
        Ok(results.into_iter().map(|r| (r.id, r.distance)).collect())
    }

    fn get_vector(&self, id: u64) -> PyResult<Vec<f32>> {
        self.inner.get_vector(id).map_err(to_pyerr)
    }

    fn contains(&self, id: u64) -> bool {
        self.inner.contains(id)
    }

    fn save(&self, path: &str) -> PyResult<()> {
        self.inner.save(path).map_err(to_pyerr)
    }

    #[staticmethod]
    fn load(path: &str) -> PyResult<Self> {
        let inner = HnswIndex::load(path).map_err(to_pyerr)?;
        Ok(Self { inner })
    }

    #[getter]
    fn ef_search(&self) -> usize {
        self.inner.get_ef_search()
    }

    #[setter]
    fn set_ef_search(&self, ef: usize) {
        self.inner.set_ef_search(ef);
    }

    fn __len__(&self) -> usize {
        self.inner.size()
    }

    #[getter]
    fn dimension(&self) -> usize {
        self.inner.dimension()
    }

    #[getter]
    fn capacity(&self) -> usize {
        self.inner.capacity()
    }
}
```

Also register it in the `#[pymodule]` function:
```rust
    m.add_class::<PyHnswIndex>()?;
```

- [ ] **Step 2: Build and test from Python**

```bash
cd /Users/anton/code/vanedb/vanedb-py
maturin develop
python3 -c "
import vanedb

idx = vanedb.PyHnswIndex(3, capacity=100)
idx.add(1, [1.0, 0.0, 0.0])
idx.add(2, [0.0, 1.0, 0.0])
idx.add(3, [0.0, 0.0, 1.0])
print(f'Size: {len(idx)}')
results = idx.search([0.9, 0.1, 0.0], 2)
print(f'Search: {results}')
assert results[0][0] == 1

# Save/load
idx.save('/tmp/test_hnsw.bin')
loaded = vanedb.PyHnswIndex.load('/tmp/test_hnsw.bin')
print(f'Loaded size: {len(loaded)}')
assert len(loaded) == 3

# ef_search property
idx.ef_search = 100
assert idx.ef_search == 100
print('OK')
"
```

- [ ] **Step 3: Commit**

```bash
cd /Users/anton/code/vanedb
cargo fmt --all
git add vanedb-py/src/lib.rs
git commit -m "feat: add Python HnswIndex bindings with save/load"
```

---

### Task 4: Python test suite + push

**Files:**
- Create: `vanedb-py/tests/test_vanedb.py`

- [ ] **Step 1: Create pytest suite**

Create `vanedb-py/tests/test_vanedb.py`:
```python
import vanedb
import os
import tempfile


def test_version():
    assert vanedb.__version__ == "0.1.0"


# --- VectorStore ---

def test_vector_store_basic():
    store = vanedb.PyVectorStore(3)
    store.add(1, [1.0, 2.0, 3.0])
    store.add(2, [4.0, 5.0, 6.0])
    assert len(store) == 2
    assert store.dimension == 3
    assert store.contains(1)
    assert not store.contains(99)


def test_vector_store_get():
    store = vanedb.PyVectorStore(3)
    store.add(1, [1.0, 2.0, 3.0])
    assert store.get(1) == [1.0, 2.0, 3.0]


def test_vector_store_search():
    store = vanedb.PyVectorStore(2)
    store.add(1, [0.0, 0.0])
    store.add(2, [1.0, 0.0])
    store.add(3, [10.0, 10.0])
    results = store.search([0.0, 0.1], 2)
    assert len(results) == 2
    assert results[0][0] == 1  # closest


def test_vector_store_cosine():
    store = vanedb.PyVectorStore(2, vanedb.PyDistanceMetric.Cosine)
    store.add(1, [1.0, 0.0])
    store.add(2, [0.0, 1.0])
    results = store.search([0.9, 0.1], 1)
    assert results[0][0] == 1


def test_vector_store_remove():
    store = vanedb.PyVectorStore(2)
    store.add(1, [1.0, 2.0])
    store.add(2, [3.0, 4.0])
    store.remove(1)
    assert len(store) == 1
    assert not store.contains(1)
    assert store.contains(2)


def test_vector_store_errors():
    store = vanedb.PyVectorStore(3)
    try:
        store.add(1, [1.0, 2.0])  # wrong dim
        assert False, "Should have raised"
    except ValueError:
        pass

    store.add(1, [1.0, 2.0, 3.0])
    try:
        store.add(1, [4.0, 5.0, 6.0])  # duplicate
        assert False, "Should have raised"
    except ValueError:
        pass


# --- HnswIndex ---

def test_hnsw_basic():
    idx = vanedb.PyHnswIndex(3, capacity=100)
    idx.add(1, [1.0, 0.0, 0.0])
    idx.add(2, [0.0, 1.0, 0.0])
    assert len(idx) == 2
    assert idx.dimension == 3
    assert idx.capacity == 100
    assert idx.contains(1)


def test_hnsw_search():
    idx = vanedb.PyHnswIndex(3, capacity=100)
    idx.add(1, [0.0, 0.0, 0.0])
    idx.add(2, [10.0, 10.0, 10.0])
    results = idx.search([0.0, 0.0, 0.0], 1)
    assert results[0][0] == 1
    assert results[0][1] < 1e-6  # exact match


def test_hnsw_save_load():
    with tempfile.NamedTemporaryFile(suffix=".bin", delete=False) as f:
        path = f.name

    try:
        idx = vanedb.PyHnswIndex(4, capacity=100, seed=42)
        for i in range(20):
            idx.add(i, [float(i)] * 4)
        idx.save(path)

        loaded = vanedb.PyHnswIndex.load(path)
        assert len(loaded) == 20
        assert loaded.get_vector(5) == [5.0, 5.0, 5.0, 5.0]

        # Search results should match
        r1 = idx.search([5.5] * 4, 3)
        r2 = loaded.search([5.5] * 4, 3)
        assert [r[0] for r in r1] == [r[0] for r in r2]
    finally:
        os.unlink(path)


def test_hnsw_ef_search():
    idx = vanedb.PyHnswIndex(3, capacity=100)
    assert idx.ef_search == 50  # default
    idx.ef_search = 200
    assert idx.ef_search == 200


def test_hnsw_errors():
    idx = vanedb.PyHnswIndex(3, capacity=2)
    idx.add(0, [0.0, 0.0, 0.0])
    idx.add(1, [1.0, 1.0, 1.0])
    try:
        idx.add(2, [2.0, 2.0, 2.0])  # full
        assert False, "Should have raised"
    except ValueError:
        pass
```

- [ ] **Step 2: Install pytest and run**

```bash
pip install pytest
cd /Users/anton/code/vanedb/vanedb-py
maturin develop
pytest tests/test_vanedb.py -v
```

Expected: All 12 tests pass

- [ ] **Step 3: Commit and push**

```bash
cd /Users/anton/code/vanedb
cargo fmt --all
git add vanedb-py/tests/
git commit -m "test: add Python test suite for VectorStore and HnswIndex"
git push
```

---

## Notes

- **`PyVectorStore` vs `VectorStore`:** The Python class is named `PyVectorStore` to avoid collision with the Rust module name. Users import it as `vanedb.PyVectorStore`. A future improvement could use `#[pyo3(name = "VectorStore")]` to rename it.
- **Return format:** Search returns `list[tuple[int, float]]` — `[(id, distance), ...]`. This is simple and works without numpy.
- **Thread safety:** PyO3 releases the GIL during Rust operations automatically. Multiple Python threads can search concurrently.
- **NumPy support:** Not included in this plan (YAGNI). Can be added later via `numpy` PyO3 feature to accept `ndarray` inputs.
- **maturin develop:** Builds and installs the package in the current Python environment in development mode. No need for `pip install -e .`.
