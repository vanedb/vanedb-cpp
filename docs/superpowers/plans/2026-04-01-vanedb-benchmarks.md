# VaneDB Benchmarks + CI Regression — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add criterion benchmarks for all core operations and a CI workflow that catches performance regressions on PRs.

**Architecture:** Three benchmark files (distance, store, hnsw) using `criterion`. A separate GitHub Actions workflow runs benchmarks on PRs: checks out `main` as baseline, runs the PR branch, compares with `critcmp`, and fails if any benchmark regresses >5%. Benchmarks also run locally via `cargo bench`.

**Tech Stack:** `criterion 0.5` (benchmarking), `critcmp` (comparison tool, installed in CI)

**Spec:** `docs/superpowers/specs/2026-03-29-vanedb-rust-rewrite-design.md` — Phase 6

---

## File Structure

| File | Responsibility |
|------|---------------|
| `vanedb/Cargo.toml` | Add `criterion` dev-dependency + `[[bench]]` entries |
| `vanedb/benches/distance_bench.rs` | Benchmark L2/Cosine/Dot at 128d, 768d, 1536d |
| `vanedb/benches/store_bench.rs` | Benchmark VectorStore search at 1k, 10k vectors |
| `vanedb/benches/hnsw_bench.rs` | Benchmark HNSW build + search at 10k vectors |
| `.github/workflows/bench.yml` | PR benchmark regression CI (>5% = fail) |

---

### Task 1: Criterion setup + distance benchmarks

**Files:**
- Modify: `vanedb/Cargo.toml`
- Create: `vanedb/benches/distance_bench.rs`

- [ ] **Step 1: Add criterion to Cargo.toml**

Add to `[dev-dependencies]` in `vanedb/Cargo.toml`:
```toml
criterion = { version = "0.5", features = ["html_reports"] }
```

Add bench entries at the end of `vanedb/Cargo.toml`:
```toml
[[bench]]
name = "distance_bench"
harness = false

[[bench]]
name = "store_bench"
harness = false

[[bench]]
name = "hnsw_bench"
harness = false
```

- [ ] **Step 2: Create distance benchmarks**

Create `vanedb/benches/distance_bench.rs`:
```rust
use criterion::{black_box, criterion_group, criterion_main, BenchmarkId, Criterion};
use vanedb::distance::{self, DistanceMetric};

fn gen_vectors(dim: usize) -> (Vec<f32>, Vec<f32>) {
    let a: Vec<f32> = (0..dim).map(|i| (i as f32 * 0.01).sin()).collect();
    let b: Vec<f32> = (0..dim).map(|i| (i as f32 * 0.02).cos()).collect();
    (a, b)
}

fn bench_distance(c: &mut Criterion) {
    let metrics = [
        ("L2", DistanceMetric::L2),
        ("Cosine", DistanceMetric::Cosine),
        ("Dot", DistanceMetric::Dot),
    ];
    let dims = [128, 768, 1536];

    for (name, metric) in &metrics {
        let mut group = c.benchmark_group(*name);
        let dist_fn = distance::distance_fn(*metric);
        for &dim in &dims {
            let (a, b) = gen_vectors(dim);
            group.bench_with_input(BenchmarkId::from_parameter(dim), &dim, |bench, _| {
                bench.iter(|| dist_fn(black_box(&a), black_box(&b)));
            });
        }
        group.finish();
    }
}

criterion_group!(benches, bench_distance);
criterion_main!(benches);
```

- [ ] **Step 3: Run benchmarks**

Run: `cargo bench --bench distance_bench -- --quick`
Expected: Benchmarks run and print results (ns/iter for each metric × dimension)

- [ ] **Step 4: Commit**

```bash
cargo fmt --all
git add vanedb/Cargo.toml vanedb/benches/distance_bench.rs
git commit -m "bench: add criterion distance benchmarks for L2/Cosine/Dot"
```

---

### Task 2: VectorStore + HNSW benchmarks

**Files:**
- Create: `vanedb/benches/store_bench.rs`, `vanedb/benches/hnsw_bench.rs`

- [ ] **Step 1: Create VectorStore benchmarks**

Create `vanedb/benches/store_bench.rs`:
```rust
use criterion::{criterion_group, criterion_main, BenchmarkId, Criterion};
use vanedb::{DistanceMetric, VectorStore};

fn gen_data(n: usize, dim: usize) -> Vec<Vec<f32>> {
    (0..n)
        .map(|i| {
            (0..dim)
                .map(|d| ((i * 31 + d * 7) % 1000) as f32 / 100.0)
                .collect()
        })
        .collect()
}

fn bench_store_search(c: &mut Criterion) {
    let dim = 128;
    let mut group = c.benchmark_group("VectorStore_search");

    for &n in &[1_000, 10_000] {
        let store = VectorStore::new(dim, DistanceMetric::L2).unwrap();
        let data = gen_data(n, dim);
        for (i, v) in data.iter().enumerate() {
            store.add(i as u64, v).unwrap();
        }
        let query: Vec<f32> = (0..dim).map(|d| (d * 13 % 1000) as f32 / 100.0).collect();

        group.bench_with_input(BenchmarkId::from_parameter(n), &n, |bench, _| {
            bench.iter(|| store.search(&query, 10).unwrap());
        });
    }
    group.finish();
}

fn bench_store_add(c: &mut Criterion) {
    let dim = 128;
    let data = gen_data(10_000, dim);

    c.bench_function("VectorStore_add_10k", |bench| {
        bench.iter(|| {
            let store = VectorStore::new(dim, DistanceMetric::L2).unwrap();
            for (i, v) in data.iter().enumerate() {
                store.add(i as u64, v).unwrap();
            }
        });
    });
}

criterion_group!(benches, bench_store_search, bench_store_add);
criterion_main!(benches);
```

- [ ] **Step 2: Create HNSW benchmarks**

Create `vanedb/benches/hnsw_bench.rs`:
```rust
use criterion::{criterion_group, criterion_main, Criterion};
use vanedb::{DistanceMetric, HnswIndex};

fn gen_data(n: usize, dim: usize) -> Vec<Vec<f32>> {
    (0..n)
        .map(|i| {
            (0..dim)
                .map(|d| ((i * 31 + d * 7) % 1000) as f32 / 100.0)
                .collect()
        })
        .collect()
}

fn bench_hnsw_build(c: &mut Criterion) {
    let dim = 128;
    let n = 10_000;
    let data = gen_data(n, dim);

    c.bench_function("HNSW_build_10k", |bench| {
        bench.iter(|| {
            let idx = HnswIndex::builder(dim, DistanceMetric::L2)
                .capacity(n)
                .m(16)
                .ef_construction(200)
                .seed(42)
                .build()
                .unwrap();
            for (i, v) in data.iter().enumerate() {
                idx.add(i as u64, v).unwrap();
            }
        });
    });
}

fn bench_hnsw_search(c: &mut Criterion) {
    let dim = 128;
    let n = 10_000;
    let data = gen_data(n, dim);

    let idx = HnswIndex::builder(dim, DistanceMetric::L2)
        .capacity(n)
        .m(16)
        .ef_construction(200)
        .seed(42)
        .build()
        .unwrap();
    for (i, v) in data.iter().enumerate() {
        idx.add(i as u64, v).unwrap();
    }
    idx.set_ef_search(50);

    let query: Vec<f32> = (0..dim).map(|d| (d * 13 % 1000) as f32 / 100.0).collect();

    c.bench_function("HNSW_search_10k", |bench| {
        bench.iter(|| idx.search(&query, 10).unwrap());
    });
}

fn bench_hnsw_save_load(c: &mut Criterion) {
    let dim = 128;
    let n = 10_000;
    let data = gen_data(n, dim);
    let path = std::env::temp_dir().join("vanedb_bench_hnsw.bin");

    let idx = HnswIndex::builder(dim, DistanceMetric::L2)
        .capacity(n)
        .m(16)
        .ef_construction(200)
        .seed(42)
        .build()
        .unwrap();
    for (i, v) in data.iter().enumerate() {
        idx.add(i as u64, v).unwrap();
    }

    c.bench_function("HNSW_save_10k", |bench| {
        bench.iter(|| idx.save(&path).unwrap());
    });

    c.bench_function("HNSW_load_10k", |bench| {
        bench.iter(|| HnswIndex::load(&path).unwrap());
    });

    let _ = std::fs::remove_file(&path);
}

criterion_group!(benches, bench_hnsw_build, bench_hnsw_search, bench_hnsw_save_load);
criterion_main!(benches);
```

- [ ] **Step 3: Run all benchmarks**

Run: `cargo bench -- --quick`
Expected: All benchmarks run (distance, store, hnsw)

- [ ] **Step 4: Commit**

```bash
cargo fmt --all
git add vanedb/benches/
git commit -m "bench: add VectorStore and HNSW benchmarks"
```

---

### Task 3: CI benchmark regression workflow

**Files:**
- Create: `.github/workflows/bench.yml`

- [ ] **Step 1: Create benchmark CI workflow**

Create `.github/workflows/bench.yml`:
```yaml
name: Benchmark

on:
  pull_request:
    branches: [main]

env:
  CARGO_TERM_COLOR: always

jobs:
  benchmark:
    name: Performance Regression Check
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
        with:
          fetch-depth: 0

      - uses: dtolnay/rust-toolchain@stable

      - name: Install critcmp
        run: cargo install critcmp

      - name: Benchmark (PR branch)
        run: cargo bench -- --save-baseline pr

      - name: Checkout main
        run: git checkout origin/main

      - name: Benchmark (main baseline)
        run: cargo bench -- --save-baseline main

      - name: Compare benchmarks
        run: |
          echo "## Benchmark Comparison (main vs PR)" >> $GITHUB_STEP_SUMMARY
          echo '```' >> $GITHUB_STEP_SUMMARY
          critcmp main pr >> $GITHUB_STEP_SUMMARY 2>&1 || true
          echo '```' >> $GITHUB_STEP_SUMMARY
          critcmp main pr --threshold 5
```

- [ ] **Step 2: Commit and push**

```bash
git add .github/workflows/bench.yml
git commit -m "ci: add benchmark regression workflow with critcmp"
git push
```

- [ ] **Step 3: Verify CI runs**

Run: `gh run list --repo vanedb/vanedb --limit 1`
Expected: CI triggered (bench workflow won't run on push to main — only on PRs)

---

## Notes

- The bench workflow only runs on PRs (not pushes to main). To test it, create a branch + PR.
- `--quick` flag for local runs skips warm-up iterations for faster feedback.
- `critcmp --threshold 5` exits non-zero if any benchmark regresses >5%.
- HNSW build benchmark (10k vectors) will be slow (~seconds). Criterion handles warm-up automatically.
- Benchmark data is deterministic (seeded) for reproducible results across runs.
